//// Request dispatch and the feature handlers.
////
//// The server is a plain fold over incoming messages: LSP over stdio is
//// strictly sequential, so there is no concurrency to manage and no actor to
//// supervise. Each handler writes its own replies and returns the next state.

import gleam/dict.{type Dict}
import gleam/dynamic.{type Dynamic}
import gleam/dynamic/decode
import gleam/int
import gleam/json
import gleam/list
import gleam/option.{type Option, None, Some}
import gleam/result
import lsp/rpc
import lsp/scan
import lsp/semantic
import lsp/uri as file_uri

const server_version = "0.1.0"

/// Request ids are "number | string" in JSON-RPC and must be echoed back in
/// the same shape they arrived in.
pub type Id {
  IntId(Int)
  TextId(String)
}

pub type Document {
  Document(text: String, analysis: Option(scan.Scan))
}

pub type State {
  State(
    documents: Dict(String, Document),
    executable: String,
    /// Negotiated during `initialize`. The lexer counts bytes, so utf-8 is the
    /// encoding this server is actually correct in.
    position_encoding: String,
    running: Bool,
  )
}

pub fn new(executable: String) -> State {
  State(
    documents: dict.new(),
    executable: executable,
    position_encoding: "utf-16",
    running: True,
  )
}

// --- dispatch ---------------------------------------------------------------

pub fn handle(state: State, message: Dynamic) -> State {
  let method = decode.run(message, decode.at(["method"], decode.string))
  let id =
    decode.run(message, decode.at(["id"], id_decoder()))
    |> option.from_result

  case method {
    Error(_) -> state
    Ok(name) -> route(state, name, id, message)
  }
}

fn route(
  state: State,
  method: String,
  id: Option(Id),
  message: Dynamic,
) -> State {
  case method, id {
    "initialize", Some(request) -> initialize(state, request, message)

    "shutdown", Some(request) -> {
      respond(request, json.null())
      state
    }

    "exit", _ -> State(..state, running: False)

    "initialized", _ -> state

    "textDocument/didOpen", _ -> did_open(state, message)
    "textDocument/didChange", _ -> did_change(state, message)
    "textDocument/didSave", _ -> reanalyze(state, message)
    "textDocument/didClose", _ -> did_close(state, message)

    "textDocument/hover", Some(request) -> hover(state, request, message)
    "textDocument/definition", Some(request) ->
      definition(state, request, message)
    "textDocument/documentSymbol", Some(request) ->
      document_symbols(state, request, message)
    "textDocument/semanticTokens/full", Some(request) ->
      semantic_tokens(state, request, message)

    // A request we do not implement still owes the client a reply, or it waits
    // forever. Unknown notifications can simply be dropped.
    _, Some(request) -> {
      respond_error(request, -32_601, "unsupported method: " <> method)
      state
    }
    _, None -> state
  }
}

// --- lifecycle --------------------------------------------------------------

fn initialize(state: State, id: Id, message: Dynamic) -> State {
  let offered =
    decode.run(
      message,
      decode.at(
        ["params", "capabilities", "general", "positionEncodings"],
        decode.list(decode.string),
      ),
    )
    |> result.unwrap([])

  // Byte columns are what the lexer produces, so utf-8 is taken whenever the
  // client supports it. Under utf-16 the two agree for ASCII and drift only on
  // lines holding multi-byte characters.
  let encoding = case list.contains(offered, "utf-8") {
    True -> "utf-8"
    False -> "utf-16"
  }

  respond(
    id,
    json.object([
      #(
        "capabilities",
        json.object([
          #("positionEncoding", json.string(encoding)),
          #(
            "textDocumentSync",
            json.object([
              #("openClose", json.bool(True)),
              // 1 = full document sync. Benzene files are small and the
              // compiler re-checks from scratch anyway, so incremental sync
              // would add bookkeeping for no gain.
              #("change", json.int(1)),
              #("save", json.bool(True)),
            ]),
          ),
          #("hoverProvider", json.bool(True)),
          #("definitionProvider", json.bool(True)),
          #("documentSymbolProvider", json.bool(True)),
          #(
            "semanticTokensProvider",
            json.object([
              #(
                "legend",
                json.object([
                  #("tokenTypes", json.array(semantic.token_types, json.string)),
                  #(
                    "tokenModifiers",
                    json.array(semantic.token_modifiers, json.string),
                  ),
                ]),
              ),
              #("full", json.bool(True)),
            ]),
          ),
        ]),
      ),
      #(
        "serverInfo",
        json.object([
          #("name", json.string("ether-lsp")),
          #("version", json.string(server_version)),
        ]),
      ),
    ]),
  )

  State(..state, position_encoding: encoding)
}

// --- document synchronisation ----------------------------------------------

fn did_open(state: State, message: Dynamic) -> State {
  let fields =
    decode.run(message, {
      use uri <- decode.subfield(
        ["params", "textDocument", "uri"],
        decode.string,
      )
      use text <- decode.subfield(
        ["params", "textDocument", "text"],
        decode.string,
      )
      decode.success(#(uri, text))
    })

  case fields {
    Error(_) -> state
    Ok(#(uri, text)) -> store_and_analyze(state, uri, text)
  }
}

fn did_change(state: State, message: Dynamic) -> State {
  let fields =
    decode.run(message, {
      use uri <- decode.subfield(
        ["params", "textDocument", "uri"],
        decode.string,
      )
      use changes <- decode.subfield(
        ["params", "contentChanges"],
        decode.list(decode.at(["text"], decode.string)),
      )
      decode.success(#(uri, changes))
    })

  case fields {
    Error(_) -> state
    // Full sync, so the last change carries the entire buffer.
    Ok(#(uri, changes)) ->
      case list.last(changes) {
        Error(_) -> state
        Ok(text) -> store_and_analyze(state, uri, text)
      }
  }
}

fn did_close(state: State, message: Dynamic) -> State {
  case document_uri(message) {
    Error(_) -> state
    Ok(uri) -> {
      // Diagnostics belong to the server, so a closed file keeps showing them
      // in the editor until they are explicitly cleared.
      publish(uri, [])
      State(..state, documents: dict.delete(state.documents, uri))
    }
  }
}

fn reanalyze(state: State, message: Dynamic) -> State {
  case document_uri(message) {
    Error(_) -> state
    Ok(uri) ->
      case dict.get(state.documents, uri) {
        Error(_) -> state
        Ok(document) -> store_and_analyze(state, uri, document.text)
      }
  }
}

fn store_and_analyze(state: State, uri: String, text: String) -> State {
  case scan.run(state.executable, file_uri.to_path(uri), text) {
    Ok(analysis) -> {
      publish(uri, list.map(analysis.diagnostics, encode_diagnostic))
      State(
        ..state,
        documents: dict.insert(
          state.documents,
          uri,
          Document(text: text, analysis: Some(analysis)),
        ),
      )
    }

    Error(reason) -> {
      // Failure here means the compiler could not be run at all -- a bad path,
      // say. Showing that in the editor beats silently reporting a clean file,
      // which is what an empty diagnostic list would imply.
      rpc.log("ether scan failed: " <> reason)
      publish(uri, [tooling_diagnostic(reason)])
      State(
        ..state,
        documents: dict.insert(
          state.documents,
          uri,
          Document(text: text, analysis: None),
        ),
      )
    }
  }
}

// --- features ---------------------------------------------------------------

fn hover(state: State, id: Id, message: Dynamic) -> State {
  case lookup_entry(state, message) {
    Error(_) -> respond(id, json.null())
    Ok(entry) -> {
      let signature = case entry.inferred {
        "" -> entry.name
        rendered -> entry.name <> " : " <> rendered
      }

      respond(
        id,
        json.object([
          #(
            "contents",
            json.object([
              #("kind", json.string("markdown")),
              #(
                "value",
                json.string(
                  "```benzene\n"
                  <> signature
                  <> "\n```\n\n"
                  <> describe_kind(entry.kind),
                ),
              ),
            ]),
          ),
          #("range", span(entry.line, entry.column, entry.length)),
        ]),
      )
    }
  }

  state
}

fn definition(state: State, id: Id, message: Dynamic) -> State {
  let target = {
    use entry <- result.try(lookup_entry(state, message))
    use uri <- result.try(document_uri(message) |> result.replace_error(Nil))
    case entry.def_line {
      0 -> Error(Nil)
      line -> Ok(#(uri, line, entry.def_column, entry.length))
    }
  }

  case target {
    Error(_) -> respond(id, json.null())
    Ok(#(uri, line, column, length)) ->
      respond(
        id,
        json.object([
          #("uri", json.string(uri)),
          #("range", span(line, column, length)),
        ]),
      )
  }

  state
}

fn document_symbols(state: State, id: Id, message: Dynamic) -> State {
  case analysis_for(state, message) {
    Error(_) -> respond(id, json.preprocessed_array([]))
    Ok(analysis) -> {
      let symbols =
        analysis.index
        |> list.filter(fn(entry) { entry.is_definition })
        |> list.filter(fn(entry) { symbol_kind(entry.kind) != 0 })
        |> list.map(fn(entry) {
          json.object([
            #("name", json.string(entry.name)),
            #("detail", json.string(entry.inferred)),
            #("kind", json.int(symbol_kind(entry.kind))),
            #("range", span(entry.line, entry.column, entry.length)),
            #("selectionRange", span(entry.line, entry.column, entry.length)),
          ])
        })

      respond(id, json.preprocessed_array(symbols))
    }
  }

  state
}

fn semantic_tokens(state: State, id: Id, message: Dynamic) -> State {
  case analysis_for(state, message) {
    Error(_) -> respond(id, json.object([#("data", json.array([], json.int))]))
    Ok(analysis) ->
      respond(
        id,
        json.object([
          #("data", semantic.encode(analysis.tokens, analysis.index)),
        ]),
      )
  }

  state
}

// --- shared lookups ---------------------------------------------------------

fn document_uri(message: Dynamic) -> Result(String, List(decode.DecodeError)) {
  decode.run(
    message,
    decode.at(["params", "textDocument", "uri"], decode.string),
  )
}

fn analysis_for(state: State, message: Dynamic) -> Result(scan.Scan, Nil) {
  use uri <- result.try(document_uri(message) |> result.replace_error(Nil))
  use document <- result.try(dict.get(state.documents, uri))
  option.to_result(document.analysis, Nil)
}

/// Finds the identifier under the cursor in the most recent analysis.
fn lookup_entry(state: State, message: Dynamic) -> Result(scan.Entry, Nil) {
  use analysis <- result.try(analysis_for(state, message))
  use position <- result.try(
    decode.run(message, {
      use line <- decode.subfield(["params", "position", "line"], decode.int)
      use character <- decode.subfield(
        ["params", "position", "character"],
        decode.int,
      )
      decode.success(#(line, character))
    })
    |> result.replace_error(Nil),
  )

  let #(line, character) = position
  analysis.index
  |> list.find(fn(entry) { scan.entry_covers(entry, line, character) })
}

// --- encoding ---------------------------------------------------------------

/// Builds an LSP range from the lexer's 1-based line/column plus a length.
fn span(line: Int, column: Int, length: Int) -> json.Json {
  let line = int.max(line - 1, 0)
  let character = int.max(column - 1, 0)

  json.object([
    #(
      "start",
      json.object([
        #("line", json.int(line)),
        #("character", json.int(character)),
      ]),
    ),
    #(
      "end",
      json.object([
        #("line", json.int(line)),
        #("character", json.int(character + length)),
      ]),
    ),
  ])
}

fn encode_diagnostic(diagnostic: scan.Diagnostic) -> json.Json {
  json.object([
    #("range", span(diagnostic.line, diagnostic.column, diagnostic.length)),
    #("severity", json.int(severity(diagnostic.severity))),
    #("source", json.string("ether/" <> diagnostic.phase)),
    #("message", json.string(diagnostic.message)),
  ])
}

/// Reported when `ether` itself could not be run, so the problem shows up in
/// the editor instead of only in the log.
fn tooling_diagnostic(reason: String) -> json.Json {
  json.object([
    #("range", span(1, 1, 1)),
    #("severity", json.int(1)),
    #("source", json.string("ether-lsp")),
    #("message", json.string("could not run the ether compiler: " <> reason)),
  ])
}

fn publish(uri: String, diagnostics: List(json.Json)) -> Nil {
  notify(
    "textDocument/publishDiagnostics",
    json.object([
      #("uri", json.string(uri)),
      #("diagnostics", json.preprocessed_array(diagnostics)),
    ]),
  )
}

fn severity(level: String) -> Int {
  case level {
    "error" -> 1
    "warning" -> 2
    _ -> 3
  }
}

/// LSP SymbolKind. Zero means "leave it out of the outline".
fn symbol_kind(kind: String) -> Int {
  case kind {
    "Function" -> 12
    "Constant" -> 14
    "Binding" -> 13
    "Type" -> 5
    "Module" -> 2
    _ -> 0
  }
}

fn describe_kind(kind: String) -> String {
  case kind {
    "Function" -> "function"
    "FuncParam" -> "function parameter"
    "Binding" -> "let binding"
    "Constant" -> "module constant"
    "Type" -> "type"
    "Module" -> "module"
    _ -> "unresolved - the compiler could not bind this name"
  }
}

fn id_decoder() -> decode.Decoder(Id) {
  decode.one_of(decode.int |> decode.map(IntId), [
    decode.string |> decode.map(TextId),
  ])
}

fn encode_id(id: Id) -> json.Json {
  case id {
    IntId(value) -> json.int(value)
    TextId(value) -> json.string(value)
  }
}

fn respond(id: Id, payload: json.Json) -> Nil {
  rpc.write_message(
    json.object([
      #("jsonrpc", json.string("2.0")),
      #("id", encode_id(id)),
      #("result", payload),
    ]),
  )
}

fn respond_error(id: Id, code: Int, reason: String) -> Nil {
  rpc.write_message(
    json.object([
      #("jsonrpc", json.string("2.0")),
      #("id", encode_id(id)),
      #(
        "error",
        json.object([
          #("code", json.int(code)),
          #("message", json.string(reason)),
        ]),
      ),
    ]),
  )
}

fn notify(method: String, params: json.Json) -> Nil {
  rpc.write_message(
    json.object([
      #("jsonrpc", json.string("2.0")),
      #("method", json.string(method)),
      #("params", params),
    ]),
  )
}
