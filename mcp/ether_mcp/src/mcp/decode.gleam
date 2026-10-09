//// JSON construct decoding. Nodes use a kind tag and named fields.

import construct as ct
import gleam/dynamic/decode
import gleam/option.{None}

pub fn construct_decoder(depth: Int) -> decode.Decoder(ct.Construct) {
  case depth >= 64 {
    True -> decode.failure(ct.NilValue, "construct nesting below 64 levels")
    False -> {
      use kind <- decode.field("kind", decode.string)
      case kind {
        "Module" -> {
          use data <- decode.field(
            "data",
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.Module(data))
        }
        "Let" -> {
          use identifier <- decode.field("identifier", decode.string)
          use data_type <- decode.optional_field(
            "data_type",
            None,
            decode.optional(type_decoder(depth + 1)),
          )
          use value <- decode.field("value", construct_decoder(depth + 1))
          decode.success(ct.Let(identifier, data_type, value))
        }
        "Const" -> {
          use identifier <- decode.field("identifier", decode.string)
          use data_type <- decode.optional_field(
            "data_type",
            None,
            decode.optional(type_decoder(depth + 1)),
          )
          use value <- decode.field("value", construct_decoder(depth + 1))
          decode.success(ct.Const(identifier, data_type, value))
        }
        "Function" -> {
          use identifier <- decode.field("identifier", decode.string)
          use return_type <- decode.optional_field(
            "return_type",
            None,
            decode.optional(type_decoder(depth + 1)),
          )
          use param_list <- decode.optional_field(
            "param_list",
            [],
            decode.list(param_decoder(depth + 1)),
          )
          use body <- decode.field(
            "body",
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.Function(identifier, return_type, param_list, body))
        }
        "Lambda" -> {
          use return_type <- decode.optional_field(
            "return_type",
            None,
            decode.optional(type_decoder(depth + 1)),
          )
          use param_list <- decode.optional_field(
            "param_list",
            [],
            decode.list(param_decoder(depth + 1)),
          )
          use body <- decode.field(
            "body",
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.Lambda(return_type, param_list, body))
        }
        "TypeDeclaration" -> {
          use identifier <- decode.field("identifier", decode.string)
          use parameters <- decode.optional_field(
            "parameters",
            [],
            decode.list(decode.string),
          )
          use alias_target <- decode.optional_field(
            "alias_target",
            None,
            decode.optional(type_decoder(depth + 1)),
          )
          use members <- decode.optional_field(
            "members",
            None,
            decode.optional(decode.list(type_decoder(depth + 1))),
          )
          decode.success(ct.TypeDeclaration(
            identifier,
            parameters,
            alias_target,
            members,
          ))
        }
        "Import" -> {
          use module_path <- decode.field("module_path", decode.string)
          decode.success(ct.Import(module_path))
        }
        "Comment" -> {
          use comment_type <- decode.field("comment_type", comment_decoder())
          use data <- decode.field("data", decode.string)
          decode.success(ct.Comment(comment_type, data))
        }
        "CaseExpr" -> {
          use conditions <- decode.field(
            "conditions",
            decode.list(construct_decoder(depth + 1)),
          )
          use case_evals <- decode.field(
            "case_evals",
            decode.list(branch_decoder(depth + 1)),
          )
          decode.success(ct.CaseExpr(conditions, case_evals))
        }
        "ScopedExpr" -> {
          use expr <- decode.field(
            "expr",
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.ScopedExpr(expr))
        }
        "Identifier" -> {
          use name <- decode.field("name", decode.string)
          decode.success(ct.Identifier(name))
        }
        "Integer" -> {
          use value <- decode.field("value", decode.int)
          decode.success(ct.Integer(value))
        }
        "Decimal" -> {
          use value <- decode.field("value", decode.float)
          decode.success(ct.Decimal(value))
        }
        "Text" -> {
          use value <- decode.field("value", decode.string)
          decode.success(ct.Text(value))
        }
        "Boolean" -> {
          use value <- decode.field("value", decode.bool)
          decode.success(ct.Boolean(value))
        }
        "NilValue" -> {
          decode.success(ct.NilValue)
        }
        "Wildcard" -> {
          decode.success(ct.Wildcard)
        }
        "Call" -> {
          use identifier <- decode.field("identifier", decode.string)
          use arguments <- decode.optional_field(
            "arguments",
            [],
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.Call(identifier, arguments))
        }
        "Binary" -> {
          use operator <- decode.field("operator", binary_decoder())
          use left <- decode.field("left", construct_decoder(depth + 1))
          use right <- decode.field("right", construct_decoder(depth + 1))
          decode.success(ct.Binary(operator, left, right))
        }
        "Unary" -> {
          use operator <- decode.field("operator", unary_decoder())
          use value <- decode.field("value", construct_decoder(depth + 1))
          decode.success(ct.Unary(operator, value))
        }
        "Pipeline" -> {
          use calls <- decode.field(
            "calls",
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.Pipeline(calls))
        }
        "ListExpr" -> {
          use values <- decode.field(
            "values",
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.ListExpr(values))
        }
        "TupleExpr" -> {
          use values <- decode.field(
            "values",
            decode.list(construct_decoder(depth + 1)),
          )
          decode.success(ct.TupleExpr(values))
        }
        _ -> decode.failure(ct.NilValue, "supported ct.Construct kind")
      }
    }
  }
}

pub fn type_decoder(depth: Int) -> decode.Decoder(ct.DataType) {
  case depth >= 64 {
    True ->
      decode.failure(
        ct.NamedType("Invalid"),
        "construct nesting below 64 levels",
      )
    False -> {
      use kind <- decode.field("kind", decode.string)
      case kind {
        "NamedType" -> {
          use name <- decode.field("name", decode.string)
          decode.success(ct.NamedType(name))
        }
        "AppliedType" -> {
          use name <- decode.field("name", decode.string)
          use arguments <- decode.field(
            "arguments",
            decode.list(type_decoder(depth + 1)),
          )
          decode.success(ct.AppliedType(name, arguments))
        }
        "FunctionType" -> {
          use parameters <- decode.field(
            "parameters",
            decode.list(type_decoder(depth + 1)),
          )
          use returns <- decode.field("returns", type_decoder(depth + 1))
          decode.success(ct.FunctionType(parameters, returns))
        }
        "LabelledType" -> {
          use name <- decode.field("name", decode.string)
          use fields <- decode.field(
            "fields",
            decode.list(field_decoder(depth + 1)),
          )
          decode.success(ct.LabelledType(name, fields))
        }
        _ ->
          decode.failure(ct.NamedType("Invalid"), "supported ct.DataType kind")
      }
    }
  }
}

fn param_decoder(depth: Int) {
  use param <- decode.field("param", decode.string)
  use data_type <- decode.optional_field(
    "data_type",
    None,
    decode.optional(type_decoder(depth + 1)),
  )
  decode.success(ct.FuncParam(param, data_type))
}

fn branch_decoder(depth: Int) {
  use cases <- decode.field("cases", decode.list(construct_decoder(depth + 1)))
  use return_value <- decode.field("return_value", construct_decoder(depth + 1))
  decode.success(ct.CaseEval(cases, return_value))
}

fn field_decoder(depth: Int) {
  use name <- decode.field("name", decode.string)
  use data_type <- decode.field("data_type", type_decoder(depth + 1))
  decode.success(ct.TypeField(name, data_type))
}

fn binary_decoder() -> decode.Decoder(ct.BinaryOperator) {
  use name <- decode.then(decode.string)
  case name {
    "Add" -> decode.success(ct.Add)
    "Subtract" -> decode.success(ct.Subtract)
    "Multiply" -> decode.success(ct.Multiply)
    "Divide" -> decode.success(ct.Divide)
    "Equal" -> decode.success(ct.Equal)
    "NotEqual" -> decode.success(ct.NotEqual)
    "LessThan" -> decode.success(ct.LessThan)
    "LessOrEqual" -> decode.success(ct.LessOrEqual)
    "GreaterThan" -> decode.success(ct.GreaterThan)
    "GreaterOrEqual" -> decode.success(ct.GreaterOrEqual)
    "And" -> decode.success(ct.And)
    "Or" -> decode.success(ct.Or)
    _ -> decode.failure(ct.Add, "supported ct.BinaryOperator")
  }
}

fn unary_decoder() -> decode.Decoder(ct.UnaryOperator) {
  use name <- decode.then(decode.string)
  case name {
    "Negate" -> decode.success(ct.Negate)
    "Not" -> decode.success(ct.Not)
    _ -> decode.failure(ct.Negate, "supported ct.UnaryOperator")
  }
}

fn comment_decoder() -> decode.Decoder(ct.CommentType) {
  use name <- decode.then(decode.string)
  case name {
    "SingleLine" -> decode.success(ct.SingleLine)
    "MultiLine" -> decode.success(ct.MultiLine)
    _ -> decode.failure(ct.SingleLine, "supported ct.CommentType")
  }
}
