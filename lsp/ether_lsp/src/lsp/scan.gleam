//// Runs the `ether` front-end over a buffer and decodes what it reports.
////
//// Everything the server knows about a Benzene file comes through here. The
//// language server deliberately owns no lexer, parser or type checker of its
//// own: duplicating them in Gleam would mean two implementations drifting
//// apart, and the editor showing types the compiler does not agree with.

import gleam/bit_array
import gleam/dynamic/decode
import gleam/int
import gleam/json
import gleam/result
import gleam/string

/// A lexical token, 1-based line/column as the lexer reports them.
pub type Token {
  Token(line: Int, column: Int, length: Int, kind: String, value: String)
}

pub type Diagnostic {
  Diagnostic(
    line: Int,
    column: Int,
    length: Int,
    severity: String,
    phase: String,
    message: String,
    related: List(Diagnostic),
  )
}

/// An identifier occurrence: where it is, what it means, and where it was
/// declared. `def_line` of 0 means the resolver could not bind it.
pub type Entry {
  Entry(
    name: String,
    kind: String,
    line: Int,
    column: Int,
    length: Int,
    inferred: String,
    /// Human-readable signature; functions only, empty elsewhere.
    detail: String,
    /// A function's return type on its own. Supplied separately because
    /// recovering it from the rendered `Fn(...) :> R` is ambiguous as soon as a
    /// parameter or the return is itself a function.
    returns: String,
    def_line: Int,
    def_column: Int,
    /// Name token of the enclosing function, or 0 at module scope.
    scope_line: Int,
    scope_column: Int,
    is_definition: Bool,
    /// Whether the declaration carried an explicit type annotation. Inlay
    /// hints are only worth showing where it did not.
    annotated: Bool,
  )
}

pub type Scan {
  Scan(
    path: String,
    tokens: List(Token),
    diagnostics: List(Diagnostic),
    index: List(Entry),
  )
}

/// Identifies one scan, so a result that arrives after the buffer has moved on
/// can be told apart from the one being waited for.
pub type Ref

@external(erlang, "ether_lsp_ffi", "start_scan")
fn start_scan(executable: String, args: List(String), payload: BitArray) -> Ref

/// Begins checking `source` as the contents of `path`, without touching the
/// file on disk -- so an unsaved buffer is checked exactly as the user sees it.
///
/// Returns immediately. The result arrives as a message carrying the same
/// `Ref`, and is turned back into a `Scan` by `decode`.
pub fn start(executable: String, path: String, source: String) -> Ref {
  let body = bit_array.from_string(source)

  // Length-prefixed so the child knows where the payload ends without needing
  // the pipe closed. See `read_stdin_payload` in cli/src/commands/scan.
  let payload =
    bit_array.append(
      bit_array.from_string(int.to_string(bit_array.byte_size(body)) <> "\n"),
      body,
    )

  start_scan(executable, ["scan", path, "-stdin"], payload)
}

/// Turns the bytes a finished scan produced into an analysis.
pub fn decode(output: BitArray) -> Result(Scan, String) {
  use text <- result.try(
    bit_array.to_string(output)
    |> result.replace_error("ether produced output that was not valid utf-8"),
  )

  json.parse(string.trim(text), scan_decoder())
  |> result.replace_error("could not decode ether's scan output")
}

fn scan_decoder() -> decode.Decoder(Scan) {
  use path <- decode.field("path", decode.string)
  use tokens <- decode.field("tokens", decode.list(token_decoder()))
  use diagnostics <- decode.field(
    "diagnostics",
    decode.list(diagnostic_decoder()),
  )
  use index <- decode.field("index", decode.list(entry_decoder()))
  decode.success(Scan(path:, tokens:, diagnostics:, index:))
}

fn token_decoder() -> decode.Decoder(Token) {
  use line <- decode.field("line", decode.int)
  use column <- decode.field("column", decode.int)
  use length <- decode.field("length", decode.int)
  use kind <- decode.field("type", decode.string)
  use value <- decode.field("value", decode.string)
  decode.success(Token(line:, column:, length:, kind:, value:))
}

fn diagnostic_decoder() -> decode.Decoder(Diagnostic) {
  use <- decode.recursive
  use line <- decode.field("line", decode.int)
  use column <- decode.field("column", decode.int)
  use length <- decode.field("length", decode.int)
  use severity <- decode.field("severity", decode.string)
  use phase <- decode.field("phase", decode.string)
  use message <- decode.field("message", decode.string)
  use related <- decode.field("related", decode.list(diagnostic_decoder()))
  decode.success(Diagnostic(
    line:,
    column:,
    length:,
    severity:,
    phase:,
    message:,
    related:,
  ))
}

fn entry_decoder() -> decode.Decoder(Entry) {
  use name <- decode.field("name", decode.string)
  use kind <- decode.field("kind", decode.string)
  use line <- decode.field("line", decode.int)
  use column <- decode.field("column", decode.int)
  use length <- decode.field("length", decode.int)
  use inferred <- decode.field("type", decode.string)
  use detail <- decode.field("detail", decode.string)
  use returns <- decode.field("returns", decode.string)
  use def_line <- decode.field("defLine", decode.int)
  use def_column <- decode.field("defColumn", decode.int)
  use scope_line <- decode.field("scopeLine", decode.int)
  use scope_column <- decode.field("scopeColumn", decode.int)
  use is_definition <- decode.field("isDefinition", decode.bool)
  use annotated <- decode.field("annotated", decode.bool)
  decode.success(Entry(
    name:,
    kind:,
    line:,
    column:,
    length:,
    inferred:,
    detail:,
    returns:,
    def_line:,
    def_column:,
    scope_line:,
    scope_column:,
    is_definition:,
    annotated:,
  ))
}

/// True when the 0-based LSP position falls inside this entry's name span.
///
/// The end is inclusive: a cursor resting just past the last character is
/// still "on" the word, which is where it sits after typing one.
pub fn entry_covers(entry: Entry, line: Int, character: Int) -> Bool {
  entry.line - 1 == line
  && character >= entry.column - 1
  && character <= entry.column - 1 + entry.length
}

/// Two entries refer to the same thing when they share a declaration site.
/// Entries the resolver could not bind (`def_line` 0) never match, including
/// against each other -- two unknown names are not known to be the same name.
pub fn same_symbol(left: Entry, right: Entry) -> Bool {
  left.def_line != 0
  && left.def_line == right.def_line
  && left.def_column == right.def_column
}
