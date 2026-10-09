//// Shared JSON shapes.
////
//// The compiler reports 1-based line and column; LSP counts both from zero.
//// Every conversion between the two goes through `span` so the adjustment
//// lives in exactly one place.

import gleam/int
import gleam/json
import lsp/scan

/// A zero-based LSP position.
pub fn position(line: Int, character: Int) -> json.Json {
  json.object([
    #("line", json.int(int.max(line, 0))),
    #("character", json.int(int.max(character, 0))),
  ])
}

/// An LSP range built from the compiler's 1-based line/column plus a length.
pub fn span(line: Int, column: Int, length: Int) -> json.Json {
  let line = int.max(line - 1, 0)
  let character = int.max(column - 1, 0)

  json.object([
    #("start", position(line, character)),
    #("end", position(line, character + length)),
  ])
}

/// The span covering an index entry's name.
pub fn entry_span(entry: scan.Entry) -> json.Json {
  span(entry.line, entry.column, entry.length)
}

/// The span covering an entry's declaration. Length is taken from the
/// occurrence, since a name is the same length wherever it appears.
pub fn definition_span(entry: scan.Entry) -> json.Json {
  span(entry.def_line, entry.def_column, entry.length)
}

pub fn markdown(value: String) -> json.Json {
  json.object([
    #("kind", json.string("markdown")),
    #("value", json.string(value)),
  ])
}

/// Renders an entry as the code block shown in hover and completion detail.
pub fn describe(entry: scan.Entry) -> String {
  let signature = case entry.detail {
    "" ->
      case entry.inferred {
        "" -> entry.name
        rendered -> entry.name <> " : " <> rendered
      }
    detail -> detail
  }

  "```benzene\n" <> signature <> "\n```\n\n" <> describe_kind(entry.kind)
}

pub fn describe_kind(kind: String) -> String {
  case kind {
    "Function" -> "function"
    "FuncParam" -> "function parameter"
    "TypeParam" -> "type parameter"
    "Binding" -> "let binding"
    "Constant" -> "module constant"
    "Type" -> "type"
    "Module" -> "module"
    _ -> "unresolved - the compiler could not bind this name"
  }
}

/// LSP SymbolKind. Zero means "leave it out of the outline".
pub fn symbol_kind(kind: String) -> Int {
  case kind {
    "Function" -> 12
    "Constant" -> 14
    "Binding" -> 13
    "Type" -> 5
    "TypeParam" -> 26
    "Module" -> 2
    _ -> 0
  }
}

/// LSP CompletionItemKind.
pub fn completion_kind(kind: String) -> Int {
  case kind {
    "Function" -> 3
    "Constant" -> 21
    "Binding" -> 6
    "FuncParam" -> 6
    "Type" -> 22
    "TypeParam" -> 25
    "Module" -> 9
    "Keyword" -> 14
    _ -> 1
  }
}
