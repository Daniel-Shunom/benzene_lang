//// Server state and request dispatch.
////
//// Edits do not analyse immediately. A `didChange` marks the document dirty
//// and returns; the main loop flushes once the client has been quiet for the
//// debounce interval. Holding a key down therefore costs one compiler run
//// instead of one per repeat, and because the reader is a separate process the
//// backlog collapses rather than queues.
////
//// A request never waits for that timer. Anything the user explicitly asked
//// for -- hover, go to definition, completion -- flushes its document first,
//// so answers are always computed from the buffer as it is now.

import gleam/dict.{type Dict}
import gleam/dynamic.{type Dynamic}
import gleam/dynamic/decode
import gleam/int
import gleam/json
import gleam/list
import gleam/option.{type Option, None, Some}
import gleam/result
import gleam/set.{type Set}
import lsp/feature
import lsp/rpc
import lsp/scan
import lsp/semantic
import lsp/uri as file_uri

/// Runs `work`, calling `recover` if it throws.
@external(erlang, "ether_lsp_ffi", "guard_with")
fn guard_with(work: fn() -> State, recover: fn() -> State) -> State

const server_version = "0.2.0"

/// The shortest pause that triggers a re-check. Long enough that ordinary
/// typing produces one run, short enough that the gap between words already
/// shows results.
const min_debounce_ms = 120

/// The longest the server will make the user wait, however slow the compiler
/// turns out to be.
const max_debounce_ms = 600

/// A scan slower than this is logged. Normal runs are a few milliseconds, so
/// anything here means the file or the machine is worth looking at.
const slow_scan_ms = 250

/// Request ids are "number | string" in JSON-RPC and must be echoed back in
/// the same shape they arrived in.
pub type Id {
  IntId(Int)
  TextId(String)
}

pub type Document {
  Document(
    text: String,
    /// The most recent successful analysis. Kept across a failed run so
    /// navigation still works while the compiler is broken.
    analysis: Option(scan.Scan),
    /// The text of the last *attempt*, successful or not. Re-running is
    /// skipped while it still matches `text` -- which is what stops a failing
    /// compiler from being spawned once per request.
    attempted: Option(String),
    /// Whether that attempt failed. Saving retries only in that case, so a
    /// broken toolchain can be fixed and picked up with `:w`, while an
    /// unchanged save after a good run stays free.
    failed: Bool,
  )
}

pub type State {
  State(
    documents: Dict(String, Document),
    executable: String,
    /// Negotiated during `initialize`. The lexer counts bytes, so utf-8 is the
    /// encoding this server is actually correct in.
    position_encoding: String,
    running: Bool,
    /// Documents edited since their last analysis.
    dirty: Set(String),
    /// How long the last compiler run took. The debounce follows it, so a
    /// project where checking costs half a second is not re-checked every
    /// 120ms while someone is still typing.
    last_scan_ms: Int,
  )
}

pub fn new(executable: String) -> State {
  State(
    documents: dict.new(),
    executable: executable,
    position_encoding: "utf-16",
    running: True,
    dirty: set.new(),
    last_scan_ms: 0,
  )
}

/// How long the main loop should wait for the next message: the debounce
/// interval when work is queued, otherwise indefinitely.
pub fn idle_timeout(state: State) -> Int {
  case set.is_empty(state.dirty) {
    True -> -1
    False -> debounce(state)
  }
}

/// Waits roughly as long as the last check took, bounded at both ends.
///
/// On a small file that is the 120ms floor and feels immediate. On one where
/// the compiler needs half a second, backing off keeps the server from
/// spending all its time on checks that the next keystroke invalidates.
fn debounce(state: State) -> Int {
  int.clamp(state.last_scan_ms, min_debounce_ms, max_debounce_ms)
}

/// Analyses every document edited since the last flush.
pub fn flush(state: State) -> State {
  set.fold(state.dirty, state, fn(carried, uri) { analyze(carried, uri, False) })
}

// --- dispatch ---------------------------------------------------------------

pub fn handle(state: State, message: Dynamic) -> State {
  let method = decode.run(message, decode.at(["method"], decode.string))
  let id =
    decode.run(message, decode.at(["id"], id_decoder()))
    |> option.from_result

  case method, id {
    Error(_), _ -> state

    // A request is the user waiting on an answer, so the document is brought
    // up to date first rather than replying from a debounced-stale analysis.
    Ok(name), Some(request) -> {
      let prepared = flush_document(state, message)
      guard_with(fn() { answer(prepared, name, request, message) }, fn() {
        // Every request must be answered. A handler that throws would
        // otherwise leave the editor waiting on that id for the rest of the
        // session, which looks like a hang rather than a bug.
        respond_error(request, -32_603, "internal error handling " <> name)
        prepared
      })
    }

    Ok(name), None -> notify(state, name, message)
  }
}

fn notify(state: State, method: String, message: Dynamic) -> State {
  case method {
    "exit" -> State(..state, running: False)

    "textDocument/didOpen" -> did_open(state, message)
    "textDocument/didChange" -> did_change(state, message)
    // Saving is the natural moment to retry after fixing whatever broke the
    // compiler, so it forces a run when the last one failed.
    "textDocument/didSave" -> save(state, message)
    "textDocument/didClose" -> did_close(state, message)

    // `initialized`, `$/cancelRequest` and `$/setTrace` need no reply and no
    // state. Cancellation in particular is safe to drop: every request here is
    // answered from data already in memory.
    _ -> state
  }
}

fn answer(state: State, method: String, id: Id, message: Dynamic) -> State {
  case method {
    "initialize" -> initialize(state, id, message)

    "shutdown" -> {
      respond(id, json.null())
      state
    }

    "textDocument/hover" ->
      with_position(state, id, message, fn(analysis, line, character) {
        feature.hover(analysis, line, character)
      })

    "textDocument/definition" ->
      with_position(state, id, message, fn(analysis, line, character) {
        feature.definition(uri_of(message), analysis, line, character)
      })

    "textDocument/references" -> {
      let include =
        decode.run(
          message,
          decode.at(["params", "context", "includeDeclaration"], decode.bool),
        )
        |> result.unwrap(True)

      with_position(state, id, message, fn(analysis, line, character) {
        feature.references(uri_of(message), analysis, line, character, include)
      })
    }

    "textDocument/documentHighlight" ->
      with_position(state, id, message, fn(analysis, line, character) {
        feature.document_highlight(analysis, line, character)
      })

    "textDocument/completion" -> {
      let source =
        document(state, message)
        |> result.map(fn(each) { each.text })
        |> result.unwrap("")

      with_position(state, id, message, fn(analysis, line, character) {
        feature.completion(analysis, source, line, character)
      })
    }

    "textDocument/signatureHelp" ->
      with_position(state, id, message, fn(analysis, line, character) {
        feature.signature_help(analysis, line, character)
      })

    "textDocument/prepareRename" ->
      with_position(state, id, message, fn(analysis, line, character) {
        feature.prepare_rename(analysis, line, character)
      })

    "textDocument/rename" -> rename(state, id, message)

    "textDocument/documentSymbol" -> {
      respond(id, case analysis_for(state, message) {
        Ok(analysis) -> feature.document_symbols(analysis)
        Error(_) -> json.preprocessed_array([])
      })
      state
    }

    "textDocument/semanticTokens/full" -> {
      respond(id, case analysis_for(state, message) {
        Ok(analysis) ->
          json.object([
            #("data", semantic.encode(analysis.tokens, analysis.index)),
          ])
        Error(_) -> json.object([#("data", json.array([], json.int))])
      })
      state
    }

    "textDocument/foldingRange" -> {
      respond(id, case analysis_for(state, message) {
        Ok(analysis) -> feature.folding_ranges(analysis)
        Error(_) -> json.preprocessed_array([])
      })
      state
    }

    "textDocument/inlayHint" ->
      with_line_range(state, id, message, fn(analysis, from, to) {
        feature.inlay_hints(analysis, from, to)
      })

    "textDocument/codeAction" ->
      with_line_range(state, id, message, fn(analysis, from, to) {
        feature.code_actions(uri_of(message), analysis, from, to)
      })

    // Unimplemented requests still owe a reply, or the client waits forever.
    _ -> {
      respond_error(id, -32_601, "unsupported method: " <> method)
      state
    }
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
      #("capabilities", capabilities(encoding)),
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

fn capabilities(encoding: String) -> json.Json {
  json.object([
    #("positionEncoding", json.string(encoding)),
    #(
      "textDocumentSync",
      json.object([
        #("openClose", json.bool(True)),
        // 1 = full document sync. Benzene files are small and the compiler
        // re-checks from scratch anyway, so incremental sync would add
        // bookkeeping for no gain.
        #("change", json.int(1)),
        #("save", json.bool(True)),
      ]),
    ),
    #("hoverProvider", json.bool(True)),
    #("definitionProvider", json.bool(True)),
    #("referencesProvider", json.bool(True)),
    #("documentHighlightProvider", json.bool(True)),
    #("documentSymbolProvider", json.bool(True)),
    #("foldingRangeProvider", json.bool(True)),
    #("inlayHintProvider", json.bool(True)),
    #(
      "codeActionProvider",
      json.object([
        #("codeActionKinds", json.array(["refactor.rewrite"], json.string)),
      ]),
    ),
    #("renameProvider", json.object([#("prepareProvider", json.bool(True))])),
    #(
      "completionProvider",
      json.object([
        // `:` opens an annotation and `>` completes the `:>` arrow; both are
        // points where the useful suggestions are types rather than values.
        #("triggerCharacters", json.array([":", ">"], json.string)),
      ]),
    ),
    #(
      "signatureHelpProvider",
      json.object([
        #("triggerCharacters", json.array(["(", ","], json.string)),
      ]),
    ),
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
  ])
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
    // Opening is not typing: the user is looking at the file now, so it is
    // checked immediately rather than after the debounce.
    Ok(#(uri, text)) -> analyze(store(state, uri, text), uri, False)
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
        Ok(text) -> store(state, uri, text)
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
      State(
        ..state,
        documents: dict.delete(state.documents, uri),
        dirty: set.delete(state.dirty, uri),
      )
    }
  }
}

/// Records new text and queues the document for analysis.
fn store(state: State, uri: String, text: String) -> State {
  let existing =
    dict.get(state.documents, uri)
    |> result.unwrap(Document(
      text: "",
      analysis: None,
      attempted: None,
      failed: False,
    ))

  State(
    ..state,
    documents: dict.insert(
      state.documents,
      uri,
      Document(..existing, text: text),
    ),
    dirty: set.insert(state.dirty, uri),
  )
}

/// Brings the document named in `message` up to date, if it is queued.
fn flush_document(state: State, message: Dynamic) -> State {
  case document_uri(message) {
    Error(_) -> state
    Ok(uri) -> analyze(state, uri, False)
  }
}

/// Saving retries a failed run. It is the natural gesture after fixing
/// whatever broke the compiler, and re-running an identical successful check
/// would be pure waste.
fn save(state: State, message: Dynamic) -> State {
  case document_uri(message) {
    Error(_) -> state
    Ok(uri) -> {
      let retry =
        dict.get(state.documents, uri)
        |> result.map(fn(each) { each.failed })
        |> result.unwrap(False)

      analyze(state, uri, retry)
    }
  }
}

/// Runs the compiler over a document and publishes what it says.
///
/// Skipped when this exact text has already been through the compiler, unless
/// `force` says to try again regardless.
fn analyze(state: State, uri: String, force: Bool) -> State {
  case dict.get(state.documents, uri) {
    Error(_) -> State(..state, dirty: set.delete(state.dirty, uri))

    Ok(document) ->
      case !force && document.attempted == Some(document.text) {
        True -> State(..state, dirty: set.delete(state.dirty, uri))
        False -> run_scan(state, uri, document)
      }
  }
}

fn run_scan(state: State, uri: String, document: Document) -> State {
  let started = rpc.now_ms()
  let outcome = scan.run(state.executable, file_uri.to_path(uri), document.text)
  let elapsed = rpc.now_ms() - started

  let dirty = set.delete(state.dirty, uri)

  case outcome {
    Ok(analysis) -> {
      publish(uri, feature.diagnostics(analysis))
      case elapsed >= slow_scan_ms {
        True ->
          rpc.log(
            "ether-lsp: slow scan - " <> feature.summary(analysis, elapsed),
          )
        False -> Nil
      }

      State(
        ..state,
        dirty: dirty,
        last_scan_ms: elapsed,
        documents: dict.insert(
          state.documents,
          uri,
          Document(
            text: document.text,
            analysis: Some(analysis),
            attempted: Some(document.text),
            failed: False,
          ),
        ),
      )
    }

    Error(reason) -> {
      // Failure here means the compiler could not be run at all -- a bad path,
      // a crash, a timeout. Reporting it in the editor beats silently showing
      // a clean file, which is what an empty diagnostic list would imply.
      rpc.log("ether-lsp: scan failed - " <> reason)
      publish(uri, [feature.tooling_diagnostic(reason)])

      // The previous analysis is kept -- stale navigation beats none -- but
      // the attempt is recorded so the next hover does not spawn the same
      // failing process again. An edit, or a save, is what retries.
      State(
        ..state,
        dirty: dirty,
        last_scan_ms: elapsed,
        documents: dict.insert(
          state.documents,
          uri,
          Document(..document, attempted: Some(document.text), failed: True),
        ),
      )
    }
  }
}

// --- request helpers --------------------------------------------------------

/// Replies using the analysis and the cursor position from `message`, or with
/// `null` when either is missing.
fn with_position(
  state: State,
  id: Id,
  message: Dynamic,
  build: fn(scan.Scan, Int, Int) -> json.Json,
) -> State {
  let answer = {
    use analysis <- result.try(analysis_for(state, message))
    use position <- result.try(position_of(message))
    let #(line, character) = position
    Ok(build(analysis, line, character))
  }

  respond(id, result.unwrap(answer, json.null()))
  state
}

fn rename(state: State, id: Id, message: Dynamic) -> State {
  let new_name =
    decode.run(message, decode.at(["params", "newName"], decode.string))
    |> result.unwrap("")

  let outcome = {
    use analysis <- result.try(
      analysis_for(state, message)
      |> result.replace_error("this document has not been analysed yet"),
    )
    use position <- result.try(
      position_of(message)
      |> result.replace_error("the request carried no cursor position"),
    )
    let #(line, character) = position
    feature.rename(uri_of(message), analysis, line, character, new_name)
  }

  case outcome {
    Ok(edit) -> respond(id, edit)
    // A refused rename is reported as a request error so the editor shows the
    // reason, rather than silently applying nothing.
    Error(reason) -> respond_error(id, -32_602, reason)
  }

  state
}

/// Replies using the analysis and the line range from `message`.
///
/// Both inlay hints and code actions are asked about a visible region rather
/// than a point, and both answer with an empty list when there is nothing to
/// say -- never `null`, which some clients treat as an error.
fn with_line_range(
  state: State,
  id: Id,
  message: Dynamic,
  build: fn(scan.Scan, Int, Int) -> json.Json,
) -> State {
  let range =
    decode.run(message, {
      use from <- decode.subfield(
        ["params", "range", "start", "line"],
        decode.int,
      )
      use to <- decode.subfield(["params", "range", "end", "line"], decode.int)
      decode.success(#(from, to))
    })

  respond(id, case analysis_for(state, message), range {
    Ok(analysis), Ok(#(from, to)) -> build(analysis, from, to)
    // A client that sends no range is asking about the whole file.
    Ok(analysis), Error(_) -> build(analysis, 0, 1_000_000)
    Error(_), _ -> json.preprocessed_array([])
  })

  state
}

fn document_uri(message: Dynamic) -> Result(String, Nil) {
  decode.run(
    message,
    decode.at(["params", "textDocument", "uri"], decode.string),
  )
  |> result.replace_error(Nil)
}

fn uri_of(message: Dynamic) -> String {
  result.unwrap(document_uri(message), "")
}

fn document(state: State, message: Dynamic) -> Result(Document, Nil) {
  use uri <- result.try(document_uri(message))
  dict.get(state.documents, uri)
}

fn analysis_for(state: State, message: Dynamic) -> Result(scan.Scan, Nil) {
  use found <- result.try(document(state, message))
  option.to_result(found.analysis, Nil)
}

fn position_of(message: Dynamic) -> Result(#(Int, Int), Nil) {
  decode.run(message, {
    use line <- decode.subfield(["params", "position", "line"], decode.int)
    use character <- decode.subfield(
      ["params", "position", "character"],
      decode.int,
    )
    decode.success(#(line, character))
  })
  |> result.replace_error(Nil)
}

// --- wire -------------------------------------------------------------------

fn publish(uri: String, diagnostics: List(json.Json)) -> Nil {
  send_notification(
    "textDocument/publishDiagnostics",
    json.object([
      #("uri", json.string(uri)),
      #("diagnostics", json.preprocessed_array(diagnostics)),
    ]),
  )
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

fn send_notification(method: String, params: json.Json) -> Nil {
  rpc.write_message(
    json.object([
      #("jsonrpc", json.string("2.0")),
      #("method", json.string(method)),
      #("params", params),
    ]),
  )
}

/// Re-exported so the entry point can describe what it is serving.
pub fn version() -> String {
  server_version
}
