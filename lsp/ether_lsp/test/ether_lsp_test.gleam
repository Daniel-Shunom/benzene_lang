import gleam/json
import gleam/list
import gleam/string
import gleeunit
import gleeunit/should
import lsp/encode
import lsp/feature
import lsp/scan.{
  type Entry, type Scan, type Token, Diagnostic, Entry, Scan, Token,
}
import lsp/semantic
import lsp/text
import lsp/uri

pub fn main() -> Nil {
  gleeunit.main()
}

// --- fixtures ---------------------------------------------------------------

/// A binding entry with everything defaulted, so each test states only the
/// fields it is actually about.
fn entry(name: String, line: Int, column: Int, length: Int) -> Entry {
  Entry(
    name: name,
    kind: "Binding",
    line: line,
    column: column,
    length: length,
    inferred: "Int",
    detail: "",
    def_line: line,
    def_column: column,
    scope_line: 0,
    scope_column: 0,
    is_definition: True,
    annotated: False,
  )
}

fn token(line: Int, column: Int, length: Int, kind: String) -> Token {
  Token(line: line, column: column, length: length, kind: kind, value: "")
}

fn analysis(tokens: List(Token), index: List(Entry)) -> Scan {
  Scan(path: "a.bz", tokens: tokens, diagnostics: [], index: index)
}

/// Mirrors `func identity(x: Int) :> Int / x / end` plus a call site, which is
/// the smallest program exercising a declaration, a use, and a scope.
fn program() -> Scan {
  let declaration =
    Entry(
      name: "identity",
      kind: "Function",
      line: 1,
      column: 6,
      length: 8,
      inferred: "Fn(Int) :> Int",
      detail: "identity(x: Int) :> Int",
      def_line: 1,
      def_column: 6,
      scope_line: 0,
      scope_column: 0,
      is_definition: True,
      annotated: True,
    )

  let parameter =
    Entry(
      ..entry("x", 1, 15, 1),
      kind: "FuncParam",
      scope_line: 1,
      scope_column: 6,
      annotated: True,
    )

  let use_of_parameter =
    Entry(
      ..entry("x", 2, 3, 1),
      kind: "FuncParam",
      def_line: 1,
      def_column: 15,
      scope_line: 1,
      scope_column: 6,
      is_definition: False,
    )

  let call =
    Entry(..declaration, line: 5, column: 10, is_definition: False, detail: "")

  analysis(
    [
      token(1, 1, 4, "FuncStart"),
      token(1, 6, 8, "Identifier"),
      token(1, 14, 1, "LParen"),
      token(1, 15, 1, "Identifier"),
      token(1, 21, 1, "RParen"),
      token(2, 3, 1, "Identifier"),
      token(3, 1, 3, "EndStmt"),
    ],
    [declaration, parameter, use_of_parameter, call],
  )
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

pub fn entry_covers_its_first_character_test() {
  // Entry is 1-based (line 3, column 5); the LSP position is 0-based.
  scan.entry_covers(entry("x", 3, 5, 4), 2, 4)
  |> should.be_true
}

pub fn entry_covers_the_position_just_past_its_end_test() {
  // Inclusive, so a cursor resting immediately after a name still refers to
  // it -- which is where the cursor sits having just finished typing one.
  scan.entry_covers(entry("x", 3, 5, 4), 2, 8)
  |> should.be_true
}

pub fn entry_stops_one_past_its_end_test() {
  scan.entry_covers(entry("x", 3, 5, 4), 2, 9)
  |> should.be_false
}

pub fn entry_does_not_cover_another_line_test() {
  scan.entry_covers(entry("x", 3, 5, 4), 3, 5)
  |> should.be_false
}

pub fn entries_sharing_a_declaration_are_the_same_symbol_test() {
  let declaration = entry("x", 1, 1, 1)
  let use_site = Entry(..entry("x", 9, 4, 1), def_line: 1, def_column: 1)
  scan.same_symbol(declaration, use_site)
  |> should.be_true
}

pub fn unresolved_entries_are_never_the_same_symbol_test() {
  // Two names the resolver could not bind are not known to be the same name.
  let left = Entry(..entry("x", 1, 1, 1), def_line: 0, def_column: 0)
  let right = Entry(..entry("x", 2, 1, 1), def_line: 0, def_column: 0)
  scan.same_symbol(left, right)
  |> should.be_false
}

// --- text utilities ---------------------------------------------------------

pub fn line_at_returns_the_requested_line_test() {
  text.line_at("one\ntwo\nthree", 1)
  |> should.equal("two")
}

pub fn line_at_strips_carriage_returns_test() {
  text.line_at("one\r\ntwo\r\n", 0)
  |> should.equal("one")
}

pub fn line_at_past_the_end_is_empty_test() {
  text.line_at("one\n", 9)
  |> should.equal("")
}

pub fn word_before_reads_back_to_the_word_boundary_test() {
  text.word_before("let total_count = 1", 0, 15)
  |> should.equal("total_count")
}

pub fn word_before_is_empty_after_punctuation_test() {
  text.word_before("let x = ", 0, 8)
  |> should.equal("")
}

pub fn a_colon_puts_the_cursor_in_type_position_test() {
  text.in_type_position("let x: ", 0, 7)
  |> should.be_true
}

pub fn a_partially_typed_type_is_still_type_position_test() {
  text.in_type_position("let x: In", 0, 9)
  |> should.be_true
}

pub fn the_return_arrow_puts_the_cursor_in_type_position_test() {
  text.in_type_position("func f() :> ", 0, 12)
  |> should.be_true
}

pub fn an_ordinary_expression_is_not_type_position_test() {
  text.in_type_position("let x = ", 0, 8)
  |> should.be_false
}

pub fn identifiers_must_start_with_a_letter_test() {
  text.is_identifier("total") |> should.be_true
  text.is_identifier("total_2") |> should.be_true
  text.is_identifier("2total") |> should.be_false
  text.is_identifier("") |> should.be_false
  text.is_identifier("has space") |> should.be_false
  text.is_identifier("has-dash") |> should.be_false
}

pub fn writable_types_are_bare_names_test() {
  text.is_writable_type("Int") |> should.be_true
  text.is_writable_type("MyType") |> should.be_true
  // A keyword is fine as a type name even though it is not a valid binding
  // name -- `let v: Nil` parses.
  text.is_writable_type("Nil") |> should.be_true

  text.is_writable_type("'t0") |> should.be_false
  text.is_writable_type("Fn(Int) :> Int") |> should.be_false
  text.is_writable_type("List(Int)") |> should.be_false
  text.is_writable_type("") |> should.be_false
}

pub fn keywords_are_not_valid_identifiers_test() {
  // Renaming something to `end` would produce a file that no longer parses.
  text.is_identifier("end") |> should.be_false
  text.is_identifier("func") |> should.be_false
}

// --- semantic tokens --------------------------------------------------------

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
  encoded([token(1, 6, 8, "Identifier")], [
    Entry(..entry("identity", 1, 6, 8), kind: "Function"),
  ])
  |> should.equal("[0,5,8,2,1]")
}

pub fn a_constant_is_marked_readonly_test() {
  // variable (4) with declaration|readonly (3).
  encoded([token(1, 7, 1, "Identifier")], [
    Entry(..entry("x", 1, 7, 1), kind: "Constant"),
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

// --- navigation -------------------------------------------------------------

pub fn hover_reports_the_signature_of_a_function_test() {
  json.to_string(feature.hover(program(), 0, 6))
  |> string.contains("identity(x: Int) :> Int")
  |> should.be_true
}

pub fn hover_off_a_symbol_is_null_test() {
  json.to_string(feature.hover(program(), 9, 9))
  |> should.equal("null")
}

pub fn definition_points_at_the_declaration_test() {
  // The call on line 5 resolves back to the declaration on line 1.
  json.to_string(feature.definition("file:///a.bz", program(), 4, 10))
  |> string.contains("\"line\":0")
  |> should.be_true
}

pub fn definition_of_an_unresolved_name_is_null_test() {
  let unresolved =
    analysis([], [
      Entry(..entry("mystery", 1, 1, 7), def_line: 0, def_column: 0),
    ])
  json.to_string(feature.definition("file:///a.bz", unresolved, 0, 1))
  |> should.equal("null")
}

pub fn references_finds_every_occurrence_test() {
  // The parameter is declared on line 1 and used on line 2.
  let found =
    json.to_string(feature.references("file:///a.bz", program(), 0, 15, True))
  string.contains(found, "\"line\":0") |> should.be_true
  string.contains(found, "\"line\":1") |> should.be_true
}

pub fn references_can_exclude_the_declaration_test() {
  let found =
    json.to_string(feature.references("file:///a.bz", program(), 0, 15, False))
  // Only the use on line 2 remains.
  string.contains(found, "\"line\":1") |> should.be_true
  string.contains(found, "\"line\":0") |> should.be_false
}

pub fn document_highlight_marks_the_declaration_as_a_write_test() {
  let found = json.to_string(feature.document_highlight(program(), 0, 15))
  // 3 = Write on the declaration, 2 = Read on the use.
  string.contains(found, "\"kind\":3") |> should.be_true
  string.contains(found, "\"kind\":2") |> should.be_true
}

// --- outline ----------------------------------------------------------------

pub fn the_outline_nests_locals_under_their_function_test() {
  let outline =
    analysis([], [
      Entry(..entry("outer", 1, 6, 5), kind: "Function"),
      Entry(..entry("inner", 2, 7, 5), scope_line: 1, scope_column: 6),
    ])

  // `inner` appears inside `outer`'s children rather than at the top level.
  json.to_string(feature.document_symbols(outline))
  |> string.contains("\"children\":[{\"name\":\"inner\"")
  |> should.be_true
}

pub fn parameters_are_left_out_of_the_outline_test() {
  let outline =
    analysis([], [
      Entry(..entry("f", 1, 6, 1), kind: "Function"),
      Entry(
        ..entry("p", 1, 8, 1),
        kind: "FuncParam",
        scope_line: 1,
        scope_column: 6,
      ),
    ])

  json.to_string(feature.document_symbols(outline))
  |> string.contains("\"p\"")
  |> should.be_false
}

// --- completion -------------------------------------------------------------

pub fn completion_offers_only_types_after_a_colon_test() {
  let rendered = json.to_string(feature.completion(program(), "let x: ", 0, 7))
  string.contains(rendered, "\"Int\"") |> should.be_true
  // `identity` is a value, so it has no business in annotation position.
  string.contains(rendered, "\"identity\"") |> should.be_false
}

pub fn completion_offers_values_and_keywords_elsewhere_test() {
  let rendered = json.to_string(feature.completion(program(), "let y = ", 0, 8))
  string.contains(rendered, "\"identity\"") |> should.be_true
  string.contains(rendered, "\"func\"") |> should.be_true
}

pub fn completion_offers_each_label_once_test() {
  // `Nil` is both a keyword and a built-in type, so it reaches the dedupe from
  // two directions.
  let rendered = json.to_string(feature.completion(program(), "let y = ", 0, 8))
  let occurrences = string.split(rendered, "\"label\":\"Nil\"") |> list.length
  occurrences |> should.equal(2)
}

pub fn completion_hides_locals_from_other_functions_test() {
  let two_functions =
    analysis([], [
      Entry(..entry("first", 1, 6, 5), kind: "Function"),
      Entry(..entry("hidden", 2, 7, 6), scope_line: 1, scope_column: 6),
      Entry(..entry("second", 5, 6, 6), kind: "Function"),
      Entry(..entry("shown", 6, 7, 5), scope_line: 5, scope_column: 6),
    ])

  // The cursor is on line 7 (0-based 6), inside `second`.
  let rendered = json.to_string(feature.completion(two_functions, "  ", 6, 2))
  string.contains(rendered, "\"shown\"") |> should.be_true
  string.contains(rendered, "\"hidden\"") |> should.be_false
  // Module-level functions stay visible from anywhere.
  string.contains(rendered, "\"first\"") |> should.be_true
}

// --- inlay hints ------------------------------------------------------------

pub fn an_unannotated_binding_gets_its_inferred_type_test() {
  let bindings = analysis([], [entry("total", 3, 7, 5)])
  json.to_string(feature.inlay_hints(bindings, 0, 100))
  |> string.contains("\": Int\"")
  |> should.be_true
}

pub fn an_annotated_binding_gets_no_hint_test() {
  // Repeating a type the user already wrote is noise.
  let bindings =
    analysis([], [Entry(..entry("total", 3, 7, 5), annotated: True)])
  json.to_string(feature.inlay_hints(bindings, 0, 100))
  |> should.equal("[]")
}

pub fn hints_outside_the_requested_range_are_skipped_test() {
  let bindings = analysis([], [entry("total", 50, 7, 5)])
  json.to_string(feature.inlay_hints(bindings, 0, 10))
  |> should.equal("[]")
}

pub fn a_function_return_hint_lands_after_the_parameter_list_test() {
  let fragment =
    analysis(
      [
        token(1, 1, 4, "FuncStart"),
        token(1, 6, 1, "Identifier"),
        token(1, 7, 1, "LParen"),
        token(1, 8, 1, "RParen"),
      ],
      [
        Entry(..entry("f", 1, 6, 1), kind: "Function", inferred: "Fn() :> Int"),
      ],
    )

  let rendered = json.to_string(feature.inlay_hints(fragment, 0, 100))
  // Character 8 is just past the `)` at column 8.
  string.contains(rendered, "\"character\":8") |> should.be_true
  string.contains(rendered, "\" :> Int\"") |> should.be_true
}

// --- code actions -----------------------------------------------------------

pub fn an_unannotated_binding_offers_to_be_annotated_test() {
  let bindings = analysis([], [entry("total", 3, 7, 5)])
  let rendered =
    json.to_string(feature.code_actions("file:///a.bz", bindings, 0, 100))

  string.contains(rendered, "Annotate `total` as Int") |> should.be_true
  string.contains(rendered, "\"newText\":\": Int\"") |> should.be_true
  string.contains(rendered, "refactor.rewrite") |> should.be_true
}

pub fn the_annotation_edit_is_an_insertion_test() {
  // A zero-width range inserts; anything wider would eat the name.
  let bindings = analysis([], [entry("total", 3, 7, 5)])
  json.to_string(feature.code_actions("file:///a.bz", bindings, 0, 100))
  |> string.contains(
    "\"range\":{\"start\":{\"line\":2,\"character\":11},\"end\":{\"line\":2,\"character\":11}}",
  )
  |> should.be_true
}

pub fn an_unsolved_type_is_not_offered_as_an_edit_test() {
  // `'t0` is useful to see as a hint, but writing it into the file would make
  // it stop parsing: annotations are a bare identifier and nothing else.
  let generic =
    analysis([], [Entry(..entry("value", 2, 7, 5), inferred: "'t0")])

  json.to_string(feature.code_actions("file:///a.bz", generic, 0, 100))
  |> should.equal("[]")

  // The hint still shows it.
  json.to_string(feature.inlay_hints(generic, 0, 100))
  |> string.contains("'t0")
  |> should.be_true
}

pub fn a_constructed_type_is_not_offered_as_an_edit_test() {
  let higher_order =
    analysis([], [Entry(..entry("f", 2, 7, 1), inferred: "Fn(Int) :> Int")])

  json.to_string(feature.code_actions("file:///a.bz", higher_order, 0, 100))
  |> should.equal("[]")
}

pub fn an_annotated_binding_offers_nothing_test() {
  let bindings =
    analysis([], [Entry(..entry("total", 3, 7, 5), annotated: True)])
  json.to_string(feature.code_actions("file:///a.bz", bindings, 0, 100))
  |> should.equal("[]")
}

pub fn a_code_action_matches_the_hint_it_replaces_test() {
  // Accepting the action must write exactly what the hint showed, so both read
  // the same annotation.
  let bindings = analysis([], [entry("total", 3, 7, 5)])
  let hint = json.to_string(feature.inlay_hints(bindings, 0, 100))
  let action =
    json.to_string(feature.code_actions("file:///a.bz", bindings, 0, 100))

  string.contains(hint, "\"label\":\": Int\"") |> should.be_true
  string.contains(action, "\"newText\":\": Int\"") |> should.be_true
}

// --- signature help ---------------------------------------------------------

pub fn signature_help_reports_the_callee_test() {
  let call =
    analysis(
      [
        token(5, 10, 8, "Identifier"),
        token(5, 18, 1, "LParen"),
        token(5, 19, 1, "IntegerLiteral"),
      ],
      program().index,
    )

  let rendered = json.to_string(feature.signature_help(call, 4, 19))
  string.contains(rendered, "identity(x: Int) :> Int") |> should.be_true
  string.contains(rendered, "\"activeParameter\":0") |> should.be_true
}

pub fn signature_help_counts_commas_to_the_active_parameter_test() {
  let two_args =
    Entry(
      ..entry("pair", 1, 6, 4),
      kind: "Function",
      detail: "pair(a: Int, b: Int) :> Int",
    )

  let call =
    analysis(
      [
        token(5, 1, 4, "Identifier"),
        token(5, 5, 1, "LParen"),
        token(5, 6, 1, "IntegerLiteral"),
        token(5, 7, 1, "Delimitter"),
        token(5, 9, 1, "IntegerLiteral"),
      ],
      [
        two_args,
        Entry(
          ..two_args,
          line: 5,
          column: 1,
          is_definition: False,
          detail: "",
          def_line: 1,
          def_column: 6,
        ),
      ],
    )

  json.to_string(feature.signature_help(call, 4, 9))
  |> string.contains("\"activeParameter\":1")
  |> should.be_true
}

pub fn signature_help_outside_a_call_is_null_test() {
  json.to_string(feature.signature_help(program(), 4, 0))
  |> should.equal("null")
}

// --- folding ----------------------------------------------------------------

pub fn a_function_body_folds_test() {
  // `func` on line 1, `end` on line 3: the fold hides line 2 and leaves `end`.
  let rendered = json.to_string(feature.folding_ranges(program()))
  string.contains(rendered, "\"startLine\":0") |> should.be_true
  string.contains(rendered, "\"endLine\":1") |> should.be_true
}

pub fn a_single_line_block_does_not_fold_test() {
  let inline =
    analysis([token(1, 1, 1, "LBrace"), token(1, 5, 1, "RBrace")], [])
  json.to_string(feature.folding_ranges(inline))
  |> should.equal("[]")
}

pub fn an_unbalanced_end_is_ignored_test() {
  // Mid-edit a file is often unbalanced; it must not crash or invent a range.
  let broken = analysis([token(4, 1, 3, "EndStmt")], [])
  json.to_string(feature.folding_ranges(broken))
  |> should.equal("[]")
}

// --- rename -----------------------------------------------------------------

pub fn rename_edits_every_occurrence_test() {
  let assert Ok(edit) =
    feature.rename("file:///a.bz", program(), 0, 15, "renamed")
  let rendered = json.to_string(edit)
  string.contains(rendered, "\"newText\":\"renamed\"") |> should.be_true
  // Declaration on line 1 and use on line 2 are both rewritten.
  string.contains(rendered, "\"line\":0") |> should.be_true
  string.contains(rendered, "\"line\":1") |> should.be_true
}

pub fn rename_refuses_an_illegal_name_test() {
  feature.rename("file:///a.bz", program(), 0, 15, "not a name")
  |> should.be_error
}

pub fn rename_refuses_a_keyword_test() {
  feature.rename("file:///a.bz", program(), 0, 15, "end")
  |> should.be_error
}

pub fn rename_refuses_an_unresolved_name_test() {
  // Without a declaration the other uses cannot be found, so renaming would
  // silently change only one of them.
  let unresolved =
    analysis([], [
      Entry(..entry("mystery", 1, 1, 7), def_line: 0, def_column: 0),
    ])
  feature.rename("file:///a.bz", unresolved, 0, 1, "fine")
  |> should.be_error
}

pub fn prepare_rename_refuses_an_unresolved_name_test() {
  let unresolved =
    analysis([], [
      Entry(..entry("mystery", 1, 1, 7), def_line: 0, def_column: 0),
    ])
  json.to_string(feature.prepare_rename(unresolved, 0, 1))
  |> should.equal("null")
}

// --- diagnostics ------------------------------------------------------------

pub fn a_diagnostic_carries_its_phase_as_the_source_test() {
  let reported =
    Scan(path: "a.bz", tokens: [], index: [], diagnostics: [
      Diagnostic(
        line: 2,
        column: 7,
        length: 1,
        severity: "error",
        phase: "resolver",
        message: "Duplicate",
        related: [],
      ),
    ])

  let rendered =
    json.to_string(json.preprocessed_array(feature.diagnostics(reported)))
  string.contains(rendered, "\"source\":\"ether/resolver\"") |> should.be_true
  string.contains(rendered, "\"severity\":1") |> should.be_true
  // Zero-based line 1 with a one-character span.
  string.contains(rendered, "\"character\":6") |> should.be_true
}

pub fn related_diagnostics_are_carried_through_test() {
  let reported =
    Scan(path: "a.bz", tokens: [], index: [], diagnostics: [
      Diagnostic(
        line: 2,
        column: 1,
        length: 1,
        severity: "error",
        phase: "types",
        message: "mismatch",
        related: [
          Diagnostic(
            line: 1,
            column: 1,
            length: 1,
            severity: "note",
            phase: "types",
            message: "first declared here",
            related: [],
          ),
        ],
      ),
    ])

  json.to_string(json.preprocessed_array(feature.diagnostics(reported)))
  |> string.contains("first declared here")
  |> should.be_true
}

// --- encoding ---------------------------------------------------------------

pub fn a_span_converts_to_zero_based_positions_test() {
  json.to_string(encode.span(3, 5, 4))
  |> should.equal(
    "{\"start\":{\"line\":2,\"character\":4},\"end\":{\"line\":2,\"character\":8}}",
  )
}

pub fn a_span_at_the_origin_does_not_go_negative_test() {
  json.to_string(encode.span(0, 0, 1))
  |> should.equal(
    "{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":1}}",
  )
}
