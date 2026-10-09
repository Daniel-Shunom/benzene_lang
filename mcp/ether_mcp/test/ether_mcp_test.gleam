import construct as ct
import generated_examples
import generation
import gleam/list
import gleam/option.{None, Some}
import gleam/string
import gleeunit

pub fn main() -> Nil {
  gleeunit.main()
}

pub fn function_terminator_and_indentation_test() {
  let value =
    ct.Function("main", Some(ct.NamedType("Int")), [], [ct.Integer(1)])
  assert ct.to_string(value, 1) == "  func main() :> Int\n    1\n  end"
  assert ct.to_string(value, -1) == ct.to_string(value, 0)
}

pub fn recursive_function_alias_test() {
  let value =
    ct.TypeDeclaration(
      "Handler",
      ["data"],
      Some(ct.FunctionType(
        [ct.AppliedType("List", [ct.NamedType("data")])],
        ct.NamedType("String"),
      )),
      None,
    )
  assert generation.generate(value)
    == Ok("type Handler(data) = Fn(List(data)) :> String")
}

pub fn constructor_body_and_labels_test() {
  let value =
    ct.TypeDeclaration(
      "Result",
      ["a", "b"],
      None,
      Some([
        ct.AppliedType("Ok", [ct.NamedType("a")]),
        ct.LabelledType("Error", [ct.TypeField("message", ct.NamedType("b"))]),
      ]),
    )
  assert generation.generate(value)
    == Ok("type Result(a, b) {\n  Ok(a)\n  Error(message: b)\n}")
  assert ct.to_string(ct.TypeDeclaration("Empty", [], None, Some([])), 0)
    == "type Empty {}"
  assert ct.to_string(ct.TypeDeclaration("Empty", [], None, None), 0)
    == "type Empty"
}

pub fn case_branches_are_generated_test() {
  let value =
    ct.CaseExpr([ct.Identifier("value")], [
      ct.CaseEval(
        [ct.Call("Ok", [ct.Identifier("item")])],
        ct.Identifier("item"),
      ),
      ct.CaseEval([ct.Wildcard], ct.Integer(0)),
    ])
  assert generation.generate(value)
    == Ok("case value:\n  Ok(item) :> item\n  _ :> 0\nend")
}

pub fn operator_grouping_and_call_arguments_test() {
  let value =
    ct.Call("consume", [
      ct.Binary(
        ct.Multiply,
        ct.Binary(ct.Add, ct.Integer(1), ct.Integer(2)),
        ct.Integer(3),
      ),
      ct.Integer(-4),
    ])
  assert generation.generate(value) == Ok("consume({ { 1 + 2 } * 3 }, { -4 })")
}

pub fn negative_body_statement_boundary_test() {
  assert ct.to_string(
      ct.Function("main", None, [], [ct.Integer(10), ct.Integer(-3)]),
      0,
    )
    == "func main()\n  10\n  { -3 }\nend"
}

pub fn string_escaping_test() {
  assert ct.to_string(ct.Text("\"\\\n\r\t\u{0000}"), 0)
    == "\"\\\"\\\\\\n\\r\\t\\0\""
}

pub fn comment_escaping_test() {
  assert ct.to_string(ct.Comment(ct.SingleLine, "\n{ text"), 0)
    == "Cmt | \nCmt | { text"
  assert ct.to_string(ct.Comment(ct.MultiLine, "`}"), 0) == "Cmt {\n  ```}\n}"
}

pub fn decimal_exponents_are_expanded_test() {
  assert ct.to_string(ct.Decimal(0.0000001), 0) == "0.00000010"
  assert !string.contains(
    ct.to_string(ct.Decimal(100_000_000_000_000_000_000.0), 0),
    "e",
  )
}

pub fn examples_pass_formation_test() {
  assert list.all(generated_examples.examples(), fn(example) {
    generation.validate(example.1) == []
  })
}

pub fn invalid_case_arity_test() {
  let errors =
    generation.validate(
      ct.CaseExpr([ct.Integer(1), ct.Integer(2)], [
        ct.CaseEval([ct.Wildcard], ct.Integer(0)),
      ]),
    )
  assert errors
    == [
      generation.GenerationError(
        "root.branches",
        "Each branch needs one pattern per condition",
      ),
    ]
}

pub fn module_and_body_scope_validation_test() {
  assert generation.validate(ct.Module([ct.Let("value", None, ct.Integer(1))]))
    != []
  assert generation.validate(
      ct.Function("main", None, [], [ct.Const("value", None, ct.Integer(1))]),
    )
    != []
  assert generation.validate(
      ct.Function("main", None, [], [ct.Function("nested", None, [], [])]),
    )
    != []
}

pub fn invalid_identifier_and_duplicate_parameters_test() {
  assert generation.validate(ct.Call("injected()", [])) != []
  assert generation.validate(ct.Let("let", None, ct.Integer(1))) != []
  assert generation.validate(
      ct.Function(
        "main",
        None,
        [ct.FuncParam("x", None), ct.FuncParam("x", None)],
        [],
      ),
    )
    != []
}

pub fn conflicting_alias_and_members_test() {
  assert generation.validate(ct.TypeDeclaration(
      "Bad",
      [],
      Some(ct.NamedType("Int")),
      Some([]),
    ))
    != []
}

pub fn constants_require_literals_test() {
  assert generation.validate(ct.Const("value", None, ct.Call("foo", []))) != []
  assert generation.validate(ct.Const("value", None, ct.Integer(-1))) != []
}

pub fn wildcard_and_raw_source_validation_test() {
  assert generation.validate(ct.Wildcard) != []
  assert generation.validate(ct.Literal("unverified source")) != []
}

pub fn pipeline_formation_test() {
  assert generation.validate(ct.Pipeline([ct.Call("foo", [])])) != []
  assert generation.validate(ct.Pipeline([ct.Call("foo", []), ct.Integer(1)]))
    != []
}
