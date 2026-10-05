import gleam/json
import gleeunit
import gleeunit/should
import lsp/scan.{type Entry, type Token, Entry, Token}
import lsp/semantic
import lsp/uri

pub fn main() -> Nil {
  gleeunit.main()
}

// --- uri --------------------------------------------------------------------

pub fn windows_uri_drops_the_slash_before_the_drive_test() {
  uri.to_path("file:///C:/src/main.bz")
  |> should.equal("C:/src/main.bz")
}

pub fn posix_uri_keeps_its_leading_slash_test() {
  uri.to_path("file:///home/danie/main.bz")
  |> should.equal("/home/danie/main.bz")
}

pub fn percent_escapes_are_decoded_test() {
  uri.to_path("file:///C:/my%20projects/a%2Bb.bz")
  |> should.equal("C:/my projects/a+b.bz")
}

pub fn non_file_uris_pass_through_test() {
  uri.to_path("untitled:Untitled-1")
  |> should.equal("untitled:Untitled-1")
}

// --- position maths ---------------------------------------------------------

fn entry_at(line: Int, column: Int, length: Int) -> Entry {
  Entry(
    name: "x",
    kind: "Binding",
    line: line,
    column: column,
    length: length,
    inferred: "Int",
    def_line: line,
    def_column: column,
    is_definition: True,
  )
}

pub fn entry_covers_its_first_character_test() {
  // Entry is 1-based (line 3, column 5); the LSP position is 0-based.
  scan.entry_covers(entry_at(3, 5, 4), 2, 4)
  |> should.be_true
}

pub fn entry_covers_its_last_character_test() {
  scan.entry_covers(entry_at(3, 5, 4), 2, 7)
  |> should.be_true
}

pub fn entry_stops_at_its_end_test() {
  scan.entry_covers(entry_at(3, 5, 4), 2, 8)
  |> should.be_false
}

pub fn entry_does_not_cover_another_line_test() {
  scan.entry_covers(entry_at(3, 5, 4), 3, 5)
  |> should.be_false
}

// --- semantic tokens --------------------------------------------------------

fn token(line: Int, column: Int, length: Int, kind: String) -> Token {
  Token(line: line, column: column, length: length, kind: kind, value: "")
}

fn encoded(tokens: List(Token), index: List(Entry)) -> String {
  json.to_string(semantic.encode(tokens, index))
}

pub fn first_token_is_relative_to_the_start_of_the_file_test() {
  // deltaLine 0, deltaStart 0, length 5, type keyword (5), no modifiers.
  encoded([token(1, 1, 5, "ConstantKeyword")], [])
  |> should.equal("[0,0,5,5,0]")
}

pub fn tokens_on_one_line_are_relative_to_the_previous_token_test() {
  encoded(
    [token(1, 1, 5, "ConstantKeyword"), token(1, 7, 1, "IntegerLiteral")],
    [],
  )
  |> should.equal("[0,0,5,5,0,0,6,1,8,0]")
}

pub fn a_new_line_resets_the_column_to_absolute_test() {
  encoded([token(1, 1, 5, "ConstantKeyword"), token(3, 3, 3, "EndStmt")], [])
  |> should.equal("[0,0,5,5,0,2,2,3,5,0]")
}

pub fn punctuation_is_left_out_test() {
  encoded([token(1, 1, 1, "LParen"), token(1, 2, 1, "RParen")], [])
  |> should.equal("[]")
}

pub fn skipped_punctuation_does_not_disturb_later_deltas_test() {
  // The paren emits nothing, so the number's delta is measured from `const`.
  encoded(
    [
      token(1, 1, 5, "ConstantKeyword"),
      token(1, 7, 1, "LParen"),
      token(1, 9, 1, "IntegerLiteral"),
    ],
    [],
  )
  |> should.equal("[0,0,5,5,0,0,8,1,8,0]")
}

pub fn an_identifier_takes_its_kind_from_the_index_test() {
  // type function (2) with the declaration modifier (1).
  encoded([token(2, 6, 8, "Identifier")], [
    Entry(
      name: "identity",
      kind: "Function",
      line: 2,
      column: 6,
      length: 8,
      inferred: "Fn(Int) :> Int",
      def_line: 2,
      def_column: 6,
      is_definition: True,
    ),
  ])
  |> should.equal("[1,5,8,2,1]")
}

pub fn a_constant_is_marked_readonly_test() {
  // variable (4) with declaration|readonly (3).
  encoded([token(1, 7, 1, "Identifier")], [
    Entry(
      name: "x",
      kind: "Constant",
      line: 1,
      column: 7,
      length: 1,
      inferred: "Int",
      def_line: 1,
      def_column: 7,
      is_definition: True,
    ),
  ])
  |> should.equal("[0,6,1,4,3]")
}

pub fn an_identifier_after_a_colon_is_a_type_test() {
  // Annotations never reach the index, so position is the only signal.
  encoded([token(1, 5, 1, "Colon"), token(1, 7, 3, "Identifier")], [])
  |> should.equal("[0,6,3,1,0]")
}

pub fn an_identifier_after_an_arrow_is_a_type_test() {
  encoded([token(1, 5, 2, "RtnTypeOp"), token(1, 8, 3, "Identifier")], [])
  |> should.equal("[0,4,2,9,0,0,3,3,1,0]")
}

pub fn an_unresolved_identifier_falls_back_to_a_variable_test() {
  encoded([token(1, 1, 3, "Identifier")], [])
  |> should.equal("[0,0,3,4,0]")
}
