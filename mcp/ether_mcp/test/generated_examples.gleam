import construct as ct
import ether_mcp
import generation
import gleam/io
import gleam/json
import gleam/list
import gleam/option.{None, Some}

pub fn examples() -> List(#(String, ct.Construct)) {
  [
    #("generic_alias_and_case", ether_mcp.example()),
    #(
      "expressions",
      ct.Module([
        ct.Function(
          "identity",
          Some(ct.NamedType("Int")),
          [ct.FuncParam("value", Some(ct.NamedType("Int")))],
          [ct.Identifier("value")],
        ),
        ct.Function("expressions", None, [], [
          ct.Let(
            "arithmetic",
            None,
            ct.Call("identity", [
              ct.Binary(
                ct.Multiply,
                ct.Binary(ct.Add, ct.Integer(1), ct.Integer(2)),
                ct.Integer(3),
              ),
            ]),
          ),
          ct.Let("negative", None, ct.Call("identity", [ct.Integer(-2)])),
          ct.Let("inverted", None, ct.Unary(ct.Not, ct.Boolean(False))),
          ct.Let(
            "values",
            None,
            ct.ListExpr([
              ct.Integer(1),
              ct.Binary(ct.Add, ct.Integer(2), ct.Integer(3)),
            ]),
          ),
          ct.Let(
            "tuple",
            None,
            ct.TupleExpr([
              ct.Identifier("values"),
              ct.Text(
                "quoted \" slash \\ newline\n tab\t return\r null\u{0000}",
              ),
            ]),
          ),
          ct.Let("tiny", None, ct.Decimal(0.0000001)),
          ct.Let("large", None, ct.Decimal(100_000_000_000_000_000_000.0)),
          ct.Identifier("tuple"),
        ]),
      ]),
    ),
    #(
      "lambda_and_scopes",
      ct.Module([
        ct.Function(
          "apply",
          Some(ct.NamedType("Int")),
          [
            ct.FuncParam(
              "callback",
              Some(ct.FunctionType([ct.NamedType("Int")], ct.NamedType("Int"))),
            ),
          ],
          [ct.Call("callback", [ct.Integer(4)])],
        ),
        ct.Function("main", None, [], [
          ct.Call("apply", [
            ct.Lambda(
              Some(ct.NamedType("Int")),
              [ct.FuncParam("value", Some(ct.NamedType("Int")))],
              [
                ct.ScopedExpr([
                  ct.Let(
                    "incremented",
                    None,
                    ct.Binary(ct.Add, ct.Identifier("value"), ct.Integer(1)),
                  ),
                  ct.Identifier("incremented"),
                ]),
              ],
            ),
          ]),
        ]),
      ]),
    ),
    #(
      "multi_case_and_comments",
      ct.Module([
        ct.Comment(ct.SingleLine, "{ must remain a line comment\nsecond line\n"),
        ct.Comment(
          ct.MultiLine,
          "backtick ` and closing brace }\nfunc hidden()",
        ),
        ct.Const("answer", Some(ct.NamedType("Int")), ct.Integer(42)),
        ct.TypeDeclaration("Empty", [], None, Some([])),
        ct.TypeDeclaration("Marker", [], None, None),
        ct.Function("main", None, [], [
          ct.CaseExpr([ct.Integer(1), ct.Boolean(True)], [
            ct.CaseEval(
              [ct.Integer(1), ct.Boolean(True)],
              ct.ScopedExpr([
                ct.Let("value", None, ct.Identifier("answer")),
                ct.Identifier("value"),
              ]),
            ),
            ct.CaseEval([ct.Wildcard, ct.Wildcard], ct.Integer(0)),
          ]),
        ]),
      ]),
    ),
    #(
      "pipeline",
      ct.Module([
        ct.Function("seed", Some(ct.NamedType("Int")), [], [ct.Integer(1)]),
        ct.Function(
          "identity",
          Some(ct.NamedType("Int")),
          [ct.FuncParam("value", Some(ct.NamedType("Int")))],
          [ct.Identifier("value")],
        ),
        ct.Function("main", None, [], [
          ct.Pipeline([
            ct.Call("seed", []),
            ct.Call("identity", [ct.Integer(2)]),
          ]),
        ]),
      ]),
    ),
  ]
}

pub fn main() -> Nil {
  examples()
  |> list.map(fn(example) {
    let #(name, construct) = example
    let assert Ok(source) = generation.generate(construct)
    json.object([#("name", json.string(name)), #("source", json.string(source))])
  })
  |> json.array(fn(value) { value })
  |> json.to_string
  |> io.println
}
