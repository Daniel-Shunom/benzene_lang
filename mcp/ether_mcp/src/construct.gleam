import gleam/bool
import gleam/list
import gleam/option.{type Option}
import gleam/string

pub type CommentType {
  SingleLine
  MultiLine
}

pub type DataType =
  String

pub type FuncParam {
  FuncParam(param: String, data_type: Option(DataType))
}

pub type CaseEval {
  CaseEval(cases: List(Construct), return_value: Construct)
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
  CaseExpr(conditions: List(Construct), case_evals: List(CaseEval))
  ScopedExpr(expr: List(Construct))
  Literal(data: String)
  Module(data: List(Construct))
}

pub fn to_string(construct: Construct, indent: Int) -> String {
  render_stmt(construct, indent, False)
}

fn render_stmt(construct: Construct, indent: Int, bound: Bool) -> String {
  case construct {
    Module(data) ->
      list.map(data, fn(c) { render_stmt(c, indent, bound) })
      |> string.join("\n\n")

    Function(id, return_type, param_list, body) ->
      render_function(id, return_type, param_list, body, indent, bound)

    Let(id, data_type, value) ->
      pad(indent)
      <> emit_let_expr(id, data_type, render_expr(value, indent, True))

    Const(id, data_type, value) ->
      pad(indent)
      <> emit_const_expr(id, data_type, render_expr(value, indent, True))

    Comment(cmtype, data) ->
      pad(indent) <> emit_comment(cmtype, data, indent)

    ScopedExpr(expr) ->
      render_block(expr, indent, bound)

    Literal(data) ->
      pad(indent) <> data

    CaseExpr(_, _) ->
      ""
  }
}

fn render_expr(construct: Construct, indent: Int, bound: Bool) -> String {
  case construct {
    ScopedExpr(expr) -> {
      render_block(expr, indent, bound)
    }
    _ -> {
      case construct {
        Literal(data) -> data
        _ -> render_stmt(construct, indent, bound)
      }
    }
  }
}

fn render_block(expr: List(Construct), indent: Int, bound: Bool) -> String {
  let body =
    list.map(expr, fn(c) { render_stmt(c, indent + 1, bound) })
    |> string.join("\n")

  pad(indent) <> "{\n" <> body <> "\n" <> pad(indent) <> "}\n"
}

fn render_function(
  identifier: String,
  return_type: Option(DataType),
  param_list: List(FuncParam),
  body: List(Construct),
  indent: Int,
  bound: Bool,
) -> String {
  let header =
    pad(indent)
    <> "func "
    <> identifier
    <> "("
    <> param_list_to_str(param_list)
    <> ")"
    <> case return_type {
      option.Some(data) -> " :> " <> data
      option.None -> ""
    }
    <> "\n"

  let body_str =
    list.map(body, fn(c) { render_stmt(c, indent + 1, bound) })
    |> string.join("\n")

  header <> body_str <> "\n" <> pad(indent) <> "end.\n"
}

pub fn data_to_str(data_type: DataType) -> String {
  data_type
}

fn emit_let_expr(identifier, data_type, value) {
  case data_type {
    option.Some(data) -> "let " <> identifier <> ": " <> data <> " = " <> string.trim_start(value)
    option.None -> "let " <> identifier <> " = " <> string.trim_start(value)
  }
}

fn emit_const_expr(identifier, data_type, value) {
  case data_type {
    option.Some(data) -> "const " <> identifier <> ": " <> data <> " = " <> value
    option.None -> "const " <> identifier <> " = " <> value
  }
}

fn emit_comment(comment_type, data, indent) -> String {
  let mline = string.to_graphemes(data)
    |> list.map(fn(char) {
      case char == "\n" {
        False -> char
        True -> char <> pad(indent + 1)
      }
    })
    |> string.join("")
  case comment_type {
    SingleLine -> "Cmt " <> data
    MultiLine ->
      "Cmt {\n"
      <> pad(indent + 1)
      <> mline
      <> "\n"
      <> pad(indent)
      <> "}\n"
  }
}

fn param_list_to_str(plist: List(FuncParam)) -> String {
  list.map(plist, fn(param) {
    param.param
    <> case param.data_type {
      option.Some(data) -> ": " <> data_to_str(data)
      option.None -> ""
    }
  })
  |> string.join(", ")
}

fn pad(indent: Int) {
  string.repeat(spacing, indent)
}

const spacing = "   "
