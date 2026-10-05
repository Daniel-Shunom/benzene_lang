//// Translation between the `file://` URIs the editor speaks and the plain
//// paths the compiler expects.
////
//// The path is only ever a label -- `ether scan -stdin` never opens it -- but
//// it shows up in diagnostics, so it is worth getting readable.

import gleam/int
import gleam/list
import gleam/result
import gleam/string

/// `file:///C:/src/main.bz` -> `C:/src/main.bz`, `file:///home/a.bz` ->
/// `/home/a.bz`. Anything that is not a file URI is handed back untouched.
pub fn to_path(uri: String) -> String {
  case string.starts_with(uri, "file://") {
    False -> uri
    True -> {
      let rest = string.drop_start(uri, 7)
      // Strip an empty authority, then the slash that precedes a Windows
      // drive letter -- `/C:/x` is not a usable path, `C:/x` is.
      let rest = case string.starts_with(rest, "/") {
        True -> string.drop_start(rest, 1)
        False -> rest
      }
      let decoded = percent_decode(rest)
      case is_windows_drive(decoded) {
        True -> decoded
        False -> "/" <> decoded
      }
    }
  }
}

fn is_windows_drive(path: String) -> Bool {
  case string.to_graphemes(path) {
    [letter, ":", ..] -> string.lowercase(letter) != string.uppercase(letter)
    _ -> False
  }
}

fn percent_decode(input: String) -> String {
  case string.split(input, "%") {
    [] -> input
    [first, ..rest] ->
      list.fold(rest, first, fn(acc, chunk) {
        let hex = string.slice(chunk, 0, 2)
        case int.base_parse(hex, 16) {
          Ok(code) ->
            acc
            <> string.from_utf_codepoints([
              result.unwrap(string.utf_codepoint(code), unknown_codepoint()),
            ])
            <> string.drop_start(chunk, 2)
          Error(_) -> acc <> "%" <> chunk
        }
      })
  }
}

fn unknown_codepoint() -> UtfCodepoint {
  let assert Ok(codepoint) = string.utf_codepoint(0xFFFD)
  codepoint
}
