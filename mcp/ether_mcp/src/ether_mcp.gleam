import construct as ct
import generation
import gleam/io
import gleam/option.{None, Some}

pub fn main() -> Nil {
  case generation.generate(example()) {
    Ok(source) -> io.println(source)
    Error(errors) -> echo errors |> fn(_) { Nil }
  }
}

/// A complete, compiler-checked module assembled without source fragments.
pub fn example() -> ct.Construct {
  let integer = ct.NamedType("Int")
  let text = ct.NamedType("String")
  ct.Module([
    ct.Comment(ct.SingleLine, "Generated from structured MCP constructs"),
    ct.TypeDeclaration(
      "Result",
      ["a", "b"],
      None,
      Some([
        ct.AppliedType("Ok", [ct.NamedType("a")]),
        ct.LabelledType("Error", [ct.TypeField("message", ct.NamedType("b"))]),
      ]),
    ),
    ct.TypeDeclaration(
      "Handler",
      ["data"],
      Some(ct.FunctionType([ct.NamedType("data")], text)),
      None,
    ),
    ct.Function("describe", Some(text), [ct.FuncParam("value", Some(integer))], [
      ct.Text("success"),
    ]),
    ct.Function(
      "message",
      Some(text),
      [ct.FuncParam("result", Some(ct.AppliedType("Result", [integer, text])))],
      [
        ct.CaseExpr([ct.Identifier("result")], [
          ct.CaseEval(
            [ct.Call("Ok", [ct.Identifier("value")])],
            ct.Call("describe", [ct.Identifier("value")]),
          ),
          ct.CaseEval(
            [ct.Call("Error", [ct.Identifier("value")])],
            ct.Identifier("value"),
          ),
        ]),
      ],
    ),
    ct.Function("main", Some(text), [], [
      ct.Let(
        "handler",
        Some(ct.AppliedType("Handler", [integer])),
        ct.Identifier("describe"),
      ),
      ct.Let(
        "result",
        Some(ct.AppliedType("Result", [integer, text])),
        ct.Call("Ok", [ct.Integer(42)]),
      ),
      ct.Call("message", [ct.Identifier("result")]),
    ]),
  ])
}
