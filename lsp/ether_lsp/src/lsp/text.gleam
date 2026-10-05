//// Reading the buffer directly.
////
//// Most questions are answered from the compiler's index, but a few need the
//// raw text: what the user has typed so far, and whether a proposed name is
//// even a legal identifier. Those are the cases where the last analysis is
//// necessarily behind the cursor.

import gleam/list
import gleam/string

/// The 0-based line at `index`, or an empty string past the end.
pub fn line_at(text: String, index: Int) -> String {
  case list.drop(string.split(text, "\n"), index) {
    [line, ..] -> strip_carriage_return(line)
    [] -> ""
  }
}

fn strip_carriage_return(line: String) -> String {
  case string.ends_with(line, "\r") {
    True -> string.drop_end(line, 1)
    False -> line
  }
}

/// Everything on `line` before `character`. Positions arrive in the encoding
/// that was negotiated at startup, which for every client this server has met
/// means byte offsets; `string.slice` counts graphemes, so the two agree for
/// the ASCII that Benzene identifiers and keywords are made of.
pub fn prefix(text: String, line: Int, character: Int) -> String {
  string.slice(line_at(text, line), 0, character)
}

/// The identifier being typed immediately before the cursor, if any.
pub fn word_before(text: String, line: Int, character: Int) -> String {
  prefix(text, line, character)
  |> string.to_graphemes
  |> list.reverse
  |> list.take_while(is_identifier_char)
  |> list.reverse
  |> string.concat
}

/// True when the cursor sits where only a type can go: directly after a `:`
/// annotation marker or a `:>` return arrow, with at most a partial name typed.
pub fn in_type_position(text: String, line: Int, character: Int) -> Bool {
  let before =
    prefix(text, line, character)
    |> string.drop_end(string.length(word_before(text, line, character)))
    |> string.trim_end

  string.ends_with(before, ":") || string.ends_with(before, ":>")
}

/// Whether `name` is something the lexer would read back as a single
/// identifier. Rename has to check: the editor will happily send anything.
pub fn is_identifier(name: String) -> Bool {
  has_identifier_shape(name) && !is_keyword(name)
}

/// Whether `rendered` can be written where the grammar expects a type.
///
/// Annotations are `":" <identifier>` and nothing more, so a solved type like
/// `Int` can be written down but a type variable (`'t0`) or a constructed type
/// (`Fn(Int) :> Int`) cannot -- inserting either would produce source that no
/// longer parses. Reserved words are allowed here, unlike in `is_identifier`:
/// `Nil` is both a keyword and a perfectly good type name.
pub fn is_writable_type(rendered: String) -> Bool {
  has_identifier_shape(rendered)
}

fn has_identifier_shape(name: String) -> Bool {
  case string.to_graphemes(name) {
    [] -> False
    [first, ..rest] -> is_alpha(first) && list.all(rest, is_identifier_char)
  }
}

fn is_identifier_char(character: String) -> Bool {
  is_alpha(character) || is_digit(character) || character == "_"
}

fn is_alpha(character: String) -> Bool {
  // No character-class predicate in the standard library, and a codepoint
  // range check would wrongly admit the rest of Unicode's letters, which this
  // lexer does not accept.
  string.contains("abcdefghijklmnopqrstuvwxyz", string.lowercase(character))
  && character != ""
}

fn is_digit(character: String) -> Bool {
  string.contains("0123456789", character) && character != ""
}

/// The reserved words, as the keyword table spells them.
pub const keywords = [
  "Load", "Cmt", "type", "const", "let", "func", "end", "case", "default", "Fn",
  "True", "False", "Nil",
]

/// Types the checker knows without being told. Offered during completion in
/// annotation position.
pub const builtin_types = ["Int", "Float", "String", "Bool", "Nil"]

fn is_keyword(name: String) -> Bool {
  list.contains(keywords, name)
}
