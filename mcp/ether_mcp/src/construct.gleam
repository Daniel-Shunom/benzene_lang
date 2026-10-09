//// Structured Benzene source construction, independent of the MCP transport.

import gleam/float
import gleam/int
import gleam/list
import gleam/option.{type Option, None, Some}
import gleam/string

pub type CommentType {
  SingleLine
  MultiLine
}

pub type DataType {
  NamedType(name: String)
  AppliedType(name: String, arguments: List(DataType))
  FunctionType(parameters: List(DataType), returns: DataType)
  LabelledType(name: String, fields: List(TypeField))
}

pub type TypeField {
  TypeField(name: String, data_type: DataType)
}

pub type FuncParam {
  FuncParam(param: String, data_type: Option(DataType))
}

pub type CaseEval {
  CaseEval(cases: List(Construct), return_value: Construct)
}

pub type BinaryOperator {
  Add
  Subtract
  Multiply
  Divide
  Equal
  NotEqual
  LessThan
  LessOrEqual
  GreaterThan
  GreaterOrEqual
  And
  Or
}

pub type UnaryOperator {
  Negate
  Not
}

pub type Construct {
  Let(identifier: String, data_type: Option(DataType), value: Construct)
  Const(identifier: String, data_type: Option(DataType), value: Construct)
  Comment(comment_type: CommentType, data: String)
  Function(
    identifier: String,
    return_type: Option(DataType),
    param_list: List(FuncParam),
    body: List(Construct),
  )
  Lambda(
    return_type: Option(DataType),
    param_list: List(FuncParam),
    body: List(Construct),
  )
  TypeDeclaration(
    identifier: String,
    parameters: List(String),
    alias_target: Option(DataType),
    members: Option(List(DataType)),
  )
  Import(module_path: String)
  CaseExpr(conditions: List(Construct), case_evals: List(CaseEval))
  ScopedExpr(expr: List(Construct))
  Identifier(name: String)
  Integer(value: Int)
  Decimal(value: Float)
  Text(value: String)
  Boolean(value: Bool)
  NilValue
  Wildcard
  Call(identifier: String, arguments: List(Construct))
  Binary(operator: BinaryOperator, left: Construct, right: Construct)
  Unary(operator: UnaryOperator, value: Construct)
  Pipeline(calls: List(Construct))
  ListExpr(values: List(Construct))
  TupleExpr(values: List(Construct))
  // Legacy unchecked source escape hatch. Validated generation rejects it.
  Literal(data: String)
  Module(data: List(Construct))
}

/// Low-level renderer; `generation.generate` additionally validates formation.
pub fn to_string(construct: Construct, indent: Int) -> String {
  statement(construct, case indent < 0 {
    True -> 0
    False -> indent
  })
}

pub fn data_to_str(data_type: DataType) -> String {
  case data_type {
    NamedType(name) -> name
    AppliedType(name, arguments) -> name <> "(" <> types(arguments) <> ")"
    FunctionType(parameters, returns) ->
      "Fn(" <> types(parameters) <> ") :> " <> data_to_str(returns)
    LabelledType(name, fields) ->
      name
      <> "("
      <> {
        fields
        |> list.map(fn(field) {
          field.name <> ": " <> data_to_str(field.data_type)
        })
        |> string.join(", ")
      }
      <> ")"
  }
}

fn types(values: List(DataType)) -> String {
  values |> list.map(data_to_str) |> string.join(", ")
}

fn annotation(value: Option(DataType), separator: String) -> String {
  case value {
    None -> ""
    Some(value) -> separator <> data_to_str(value)
  }
}

fn params(values: List(FuncParam)) -> String {
  values
  |> list.map(fn(value) { value.param <> annotation(value.data_type, ": ") })
  |> string.join(", ")
}

fn statement(construct: Construct, depth: Int) -> String {
  case construct {
    Module(data) ->
      data
      |> list.map(fn(item) { statement(item, depth) })
      |> string.join("\n\n")
    Function(name, returns, arguments, body) ->
      pad(depth)
      <> "func "
      <> name
      <> function_tail(returns, arguments, body, depth)
    Let(name, kind, value) ->
      pad(depth)
      <> "let "
      <> name
      <> annotation(kind, ": ")
      <> " = "
      <> expression(value, depth)
    Const(name, kind, value) ->
      pad(depth)
      <> "const "
      <> name
      <> annotation(kind, ": ")
      <> " = "
      <> expression(value, depth)
    Import(path) -> pad(depth) <> "Load " <> path
    TypeDeclaration(name, parameters, target, members) -> {
      let header =
        pad(depth)
        <> "type "
        <> name
        <> case parameters {
          [] -> ""
          _ -> "(" <> string.join(parameters, ", ") <> ")"
        }
      case target, members {
        Some(target), _ -> header <> " = " <> data_to_str(target)
        None, None -> header
        None, Some([]) -> header <> " {}"
        None, Some(members) ->
          header
          <> " {\n"
          <> {
            members
            |> list.map(fn(member) { pad(depth + 1) <> data_to_str(member) })
            |> string.join("\n")
          }
          <> "\n"
          <> pad(depth)
          <> "}"
      }
    }
    Comment(SingleLine, data) ->
      data
      |> string.split("\n")
      |> list.map(fn(line) {
        pad(depth)
        <> "Cmt "
        <> case
          string.trim(line) == ""
          || string.starts_with(string.trim_start(line), "{")
        {
          True -> "| " <> line
          False -> line
        }
      })
      |> string.join("\n")
    Comment(MultiLine, data) -> {
      let escaped =
        data |> string.replace("`", "``") |> string.replace("}", "`}")
      pad(depth)
      <> "Cmt {\n"
      <> {
        escaped
        |> string.split("\n")
        |> list.map(fn(line) { pad(depth + 1) <> line })
        |> string.join("\n")
      }
      <> "\n"
      <> pad(depth)
      <> "}"
    }
    _ -> pad(depth) <> expression(construct, depth)
  }
}

fn function_tail(
  returns: Option(DataType),
  arguments: List(FuncParam),
  body: List(Construct),
  depth: Int,
) -> String {
  "("
  <> params(arguments)
  <> ")"
  <> annotation(returns, " :> ")
  <> "\n"
  <> body_text(body, depth + 1)
  <> pad(depth)
  <> "end"
}

fn body_text(body: List(Construct), depth: Int) -> String {
  case body {
    [] -> ""
    _ ->
      {
        body
        |> list.map(fn(item) {
          // Newlines do not terminate expressions. Protect a leading unary
          // operator from joining the previous statement as a binary operator.
          case item {
            Unary(_, _) -> pad(depth) <> primary(item, depth)
            Integer(value) if value < 0 -> pad(depth) <> primary(item, depth)
            Decimal(value) if value <. 0.0 -> pad(depth) <> primary(item, depth)
            _ -> statement(item, depth)
          }
        })
        |> string.join("\n")
      }
      <> "\n"
  }
}

fn expression(construct: Construct, depth: Int) -> String {
  case construct {
    Identifier(name) -> name
    Integer(value) -> int.to_string(value)
    Decimal(value) -> decimal_text(value)
    Text(value) ->
      "\""
      <> {
        value
        |> string.replace("\\", "\\\\")
        |> string.replace("\"", "\\\"")
        |> string.replace("\n", "\\n")
        |> string.replace("\r", "\\r")
        |> string.replace("\t", "\\t")
        |> string.replace("\u{0000}", "\\0")
      }
      <> "\""
    Boolean(value) ->
      case value {
        True -> "True"
        False -> "False"
      }
    NilValue -> "Nil"
    Wildcard -> "_"
    Literal(data) -> data
    Call(name, arguments) ->
      name
      <> "("
      <> {
        arguments
        |> list.map(fn(arg) { primary(arg, depth) })
        |> string.join(", ")
      }
      <> ")"
    Binary(operator, left, right) ->
      primary(left, depth)
      <> " "
      <> binary_text(operator)
      <> " "
      <> primary(right, depth)
    Unary(operator, value) ->
      case operator {
        Negate -> "-"
        Not -> "~"
      }
      <> primary(value, depth)
    Pipeline(calls) ->
      calls
      |> list.map(fn(call) { expression(call, depth) })
      |> string.join(" |=> ")
    ListExpr(values) ->
      "["
      <> {
        values
        |> list.map(fn(value) { expression(value, depth) })
        |> string.join(", ")
      }
      <> "]"
    TupleExpr(values) ->
      "@{"
      <> {
        values
        |> list.map(fn(value) { expression(value, depth) })
        |> string.join(", ")
      }
      <> "}"
    ScopedExpr(body) -> "{\n" <> body_text(body, depth + 1) <> pad(depth) <> "}"
    Lambda(returns, arguments, body) ->
      "Fn" <> function_tail(returns, arguments, body, depth)
    CaseExpr(conditions, branches) ->
      "case "
      <> {
        conditions
        |> list.map(fn(value) { expression(value, depth) })
        |> string.join(", ")
      }
      <> ":\n"
      <> {
        branches
        |> list.map(fn(branch) {
          pad(depth + 1)
          <> {
            branch.cases
            |> list.map(fn(value) { expression(value, depth + 1) })
            |> string.join(", ")
          }
          <> " :> "
          <> expression(branch.return_value, depth + 1)
        })
        |> string.join("\n")
      }
      <> "\n"
      <> pad(depth)
      <> "end"
    _ -> statement(construct, depth)
  }
}

// The parser has no parenthesized grouping and call arguments must be primary
// expressions. Scopes preserve the meaning of nested operations and lambdas.
fn primary(value: Construct, depth: Int) -> String {
  case value {
    Binary(_, _, _)
    | Unary(_, _)
    | Pipeline(_)
    | CaseExpr(_, _)
    | Lambda(_, _, _) -> "{ " <> expression(value, depth) <> " }"
    Integer(value) if value < 0 -> "{ " <> int.to_string(value) <> " }"
    Decimal(value) if value <. 0.0 -> "{ " <> decimal_text(value) <> " }"
    _ -> expression(value, depth)
  }
}

fn decimal_text(value: Float) -> String {
  let rendered = float.to_string(value)
  case string.split_once(rendered, "e") {
    Error(_) -> rendered
    Ok(#(mantissa, exponent)) -> {
      let exponent = case int.parse(exponent) {
        Ok(value) -> value
        Error(_) -> 0
      }
      let negative = string.starts_with(mantissa, "-")
      let mantissa = case negative {
        True -> string.drop_start(mantissa, 1)
        False -> mantissa
      }
      let whole = case string.split(mantissa, ".") {
        [whole, ..] -> whole
        _ -> "0"
      }
      let digits = string.replace(mantissa, ".", "")
      let point = string.length(whole) + exponent
      let count = string.length(digits)
      let decimal = case point {
        point if point <= 0 -> "0." <> string.repeat("0", 0 - point) <> digits
        point if point >= count ->
          digits <> string.repeat("0", point - count) <> ".0"
        _ ->
          string.slice(digits, 0, point)
          <> "."
          <> string.drop_start(digits, point)
      }
      case negative {
        True -> "-" <> decimal
        False -> decimal
      }
    }
  }
}

fn binary_text(operator: BinaryOperator) -> String {
  case operator {
    Add -> "+"
    Subtract -> "-"
    Multiply -> "*"
    Divide -> "/"
    Equal -> "=="
    NotEqual -> "~="
    LessThan -> "<"
    LessOrEqual -> "<="
    GreaterThan -> ">"
    GreaterOrEqual -> ">="
    And -> "&&"
    Or -> "||"
  }
}

fn pad(depth: Int) -> String {
  string.repeat("  ", depth)
}
