//// Maps Benzene tokens onto the LSP semantic-token legend.
////
//// Highlighting is driven by the real lexer rather than a regex grammar, so
//// it cannot disagree with the compiler about where a token starts or what it
//// is. Identifiers are resolved further through the type checker's index,
//// which is what lets a call to a function colour differently from a local
//// binding.

import gleam/dict.{type Dict}
import gleam/int
import gleam/json
import gleam/list
import gleam/option.{type Option, None, Some}
import lsp/scan.{type Entry, type Token}

/// Index into this list is the `tokenType` number sent over the wire, so the
/// order is part of the protocol -- it must match what `initialize` advertises.
pub const token_types = [
  "namespace", "type", "function", "parameter", "variable", "keyword", "comment",
  "string", "number", "operator", "typeParameter",
]

pub const token_modifiers = ["declaration", "readonly"]

const type_namespace = 0

const type_type = 1

const type_function = 2

const type_parameter = 3

const type_variable = 4

const type_keyword = 5

const type_comment = 6

const type_string = 7

const type_number = 8

const type_operator = 9

const type_type_parameter = 10

const modifier_declaration = 1

const modifier_readonly = 2

/// Encodes a whole file as the flat delta array the protocol expects: five
/// integers per token, each position relative to the previous token.
pub fn encode(tokens: List(Token), index: List(Entry)) -> json.Json {
  let lookup =
    list.fold(index, dict.new(), fn(acc, entry) {
      dict.insert(acc, #(entry.line, entry.column), entry)
    })

  let #(values, _, _, _) =
    list.fold(tokens, #([], 1, 1, ""), fn(state, token) {
      let #(acc, previous_line, previous_column, previous_kind) = state

      case classify(token, previous_kind, lookup) {
        // Skipped tokens still advance `previous_kind`: a colon emits nothing
        // itself but is exactly what marks the next identifier as a type.
        None -> #(acc, previous_line, previous_column, token.kind)
        Some(#(kind, modifiers)) -> {
          let delta_line = token.line - previous_line
          let delta_column = case delta_line {
            0 -> token.column - previous_column
            _ -> token.column - 1
          }

          let encoded = [
            modifiers,
            kind,
            token.length,
            delta_column,
            delta_line,
            ..acc
          ]
          #(encoded, token.line, token.column, token.kind)
        }
      }
    })

  json.array(list.reverse(values), json.int)
}

/// `None` for tokens with nothing worth colouring -- brackets, commas, colons.
/// Leaving them out keeps the payload small and lets the editor's own
/// bracket handling apply.
fn classify(
  token: Token,
  previous_kind: String,
  lookup: Dict(#(Int, Int), Entry),
) -> Option(#(Int, Int)) {
  case token.kind {
    "ImportKeyword"
    | "ConstantKeyword"
    | "LetKeyword"
    | "TypeKeyword"
    | "FuncStart"
    | "EndStmt"
    | "Case"
    | "Default"
    | "Lambda Expression"
    | "CommentKeyword" -> Some(#(type_keyword, 0))

    "TrueLiteral" | "FalseLiteral" | "NilLiteral" ->
      Some(#(type_keyword, modifier_readonly))

    "ImportModule" -> Some(#(type_namespace, 0))
    "TypeClass" | "TypeVal" -> Some(#(type_type, 0))

    "IntegerLiteral" | "FloatLiteral" -> Some(#(type_number, 0))
    "StringLiteral" | "UT StringLiteral" -> Some(#(type_string, 0))

    "Single-Line Comment" | "Multi-Line Comment" | "UT Comment" ->
      Some(#(type_comment, 0))

    "Plus"
    | "Minus"
    | "Divide"
    | "Multiply"
    | "Percent"
    | "Gt"
    | "Ge"
    | "Lt"
    | "Le"
    | "Eq"
    | "NtEq"
    | "EqEq"
    | "And"
    | "Or"
    | "Not"
    | "Pipe Operator"
    | "RtnTypeOp" -> Some(#(type_operator, 0))

    "Identifier" -> Some(identifier_kind(token, previous_kind, lookup))

    _ -> None
  }
}

/// An identifier's colour comes from what the resolver decided it was.
///
/// Type annotations are the one case the index cannot answer: the parser
/// stores them verbatim rather than resolving them to a symbol, so they never
/// appear in the index. What it does know is that an identifier directly after
/// `:` or `:>` is in annotation position and nothing else.
fn identifier_kind(
  token: Token,
  previous_kind: String,
  lookup: Dict(#(Int, Int), Entry),
) -> #(Int, Int) {
  case dict.get(lookup, #(token.line, token.column)) {
    Error(_) ->
      case previous_kind {
        "Colon" | "RtnTypeOp" -> #(type_type, 0)
        _ -> #(type_variable, 0)
      }
    Ok(entry) -> {
      let modifiers = case entry.is_definition {
        True -> modifier_declaration
        False -> 0
      }

      case entry.kind {
        "Function" -> #(type_function, modifiers)
        "FuncParam" -> #(type_parameter, modifiers)
        "Constant" -> #(
          type_variable,
          int.bitwise_or(modifiers, modifier_readonly),
        )
        "Binding" -> #(type_variable, modifiers)
        "Type" -> #(type_type, modifiers)
        "TypeParam" -> #(type_type_parameter, modifiers)
        "Module" -> #(type_namespace, modifiers)
        _ -> #(type_variable, modifiers)
      }
    }
  }
}
