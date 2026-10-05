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
    def_line: Int,
    def_column: Int,
    is_definition: Bool,
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

@external(erlang, "ether_lsp_ffi", "run_scan")
fn run_scan(
  executable: String,
  args: List(String),
  payload: BitArray,
) -> Result(BitArray, String)

/// Checks `source` as the contents of `path`, without touching the file on
/// disk -- so an unsaved buffer is checked exactly as the user sees it.
pub fn run(
  executable: String,
  path: String,
  source: String,
) -> Result(Scan, String) {
  let body = bit_array.from_string(source)

  // Length-prefixed so the child knows where the payload ends without needing
  // the pipe closed. See `read_stdin_payload` in cli/src/commands/scan.
  let payload =
    bit_array.append(
      bit_array.from_string(int.to_string(bit_array.byte_size(body)) <> "\n"),
      body,
    )

  use output <- result.try(run_scan(
    executable,
    ["scan", path, "-stdin"],
    payload,
  ))
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
  use def_line <- decode.field("defLine", decode.int)
  use def_column <- decode.field("defColumn", decode.int)
  use is_definition <- decode.field("isDefinition", decode.bool)
  decode.success(Entry(
    name:,
    kind:,
    line:,
    column:,
    length:,
    inferred:,
    def_line:,
    def_column:,
    is_definition:,
  ))
}

/// True when the 0-based LSP position falls inside this entry's name span.
pub fn entry_covers(entry: Entry, line: Int, character: Int) -> Bool {
  entry.line - 1 == line
  && character >= entry.column - 1
  && character < entry.column - 1 + entry.length
}
