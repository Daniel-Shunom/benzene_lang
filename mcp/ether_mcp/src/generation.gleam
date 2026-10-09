//// Formation checks for tool requests. The compiler remains authoritative for
//// symbol resolution, alias expansion, inference and pattern field types.

import construct as ct
import gleam/int
import gleam/list
import gleam/option.{type Option, None, Some}
import gleam/string

pub type GenerationError {
  GenerationError(path: String, message: String)
}

type Position {
  Fragment
  Top
  Body
  Value
  Pattern
}

pub fn generate(
  construct: ct.Construct,
) -> Result(String, List(GenerationError)) {
  case validate(construct) {
    [] -> Ok(ct.to_string(construct, 0))
    errors -> Error(errors)
  }
}

pub fn validate(construct: ct.Construct) -> List(GenerationError) {
  check(construct, Fragment, "root")
}

fn require(ok: Bool, path: String, message: String) -> List(GenerationError) {
  case ok {
    True -> []
    False -> [GenerationError(path, message)]
  }
}

fn valid_name(name: String) -> Bool {
  case string.to_graphemes(name) {
    [] -> False
    [first, ..rest] ->
      alpha(first)
      && list.all(rest, fn(char) {
        alpha(char) || string.contains("0123456789_", char)
      })
      && !list.contains(
        [
          "func",
          "end",
          "case",
          "default",
          "let",
          "const",
          "type",
          "Load",
          "Cmt",
          "Fn",
          "True",
          "False",
          "Nil",
        ],
        name,
      )
  }
}

fn alpha(char: String) -> Bool {
  char != ""
  && string.contains(
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ",
    char,
  )
}

fn name_errors(name: String, path: String) -> List(GenerationError) {
  require(
    valid_name(name),
    path,
    "Expected a non-reserved identifier beginning with a letter",
  )
}

fn distinct(names: List(String), path: String) -> List(GenerationError) {
  require(
    list.length(list.unique(names)) == list.length(names),
    path,
    "Names must be unique",
  )
}

fn type_errors(value: ct.DataType, path: String) -> List(GenerationError) {
  case value {
    ct.NamedType("Nil") -> []
    ct.NamedType(name) -> name_errors(name, path)
    ct.AppliedType(name, args) ->
      list.append(
        name_errors(name, path),
        list.flat_map(args, fn(arg) { type_errors(arg, path <> ".arguments") }),
      )
    ct.FunctionType(args, returns) ->
      list.append(
        list.flat_map(args, fn(arg) { type_errors(arg, path <> ".parameters") }),
        type_errors(returns, path <> ".returns"),
      )
    ct.LabelledType(name, fields) ->
      list.flatten([
        name_errors(name, path),
        distinct(list.map(fields, fn(field) { field.name }), path <> ".fields"),
        list.flat_map(fields, fn(field) {
          list.append(
            name_errors(field.name, path <> ".fields"),
            type_errors(field.data_type, path <> ".fields." <> field.name),
          )
        }),
      ])
  }
}

fn optional_type(
  value: Option(ct.DataType),
  path: String,
) -> List(GenerationError) {
  case value {
    None -> []
    Some(value) -> type_errors(value, path)
  }
}

fn parameter_errors(
  parameters: List(ct.FuncParam),
  path: String,
) -> List(GenerationError) {
  list.append(
    distinct(list.map(parameters, fn(param) { param.param }), path),
    list.flat_map(parameters, fn(param) {
      list.append(
        name_errors(param.param, path),
        optional_type(param.data_type, path <> "." <> param.param),
      )
    }),
  )
}

fn children(
  values: List(ct.Construct),
  position: Position,
  path: String,
) -> List(GenerationError) {
  values
  |> list.index_map(fn(value, index) {
    check(value, position, path <> "[" <> int.to_string(index) <> "]")
  })
  |> list.flatten
}

fn value_position(position: Position, path: String) -> List(GenerationError) {
  require(
    position != Top,
    path,
    "Module bodies contain declarations, imports and comments",
  )
}

fn is_constant(value: ct.Construct) -> Bool {
  case value {
    ct.Integer(value) -> value >= 0
    ct.Decimal(value) -> value >=. 0.0
    ct.Text(_) | ct.Boolean(_) | ct.NilValue -> True
    _ -> False
  }
}

fn check(
  value: ct.Construct,
  position: Position,
  path: String,
) -> List(GenerationError) {
  case value {
    ct.Module(items) ->
      list.append(
        require(position == Fragment, path, "Modules cannot be nested"),
        children(items, Top, path <> ".module"),
      )
    ct.Function(name, returns, params, body) ->
      list.flatten([
        require(
          position == Fragment || position == Top,
          path,
          "Named functions belong at module scope",
        ),
        name_errors(name, path),
        optional_type(returns, path <> ".returns"),
        parameter_errors(params, path <> ".parameters"),
        children(body, Body, path <> ".body"),
      ])
    ct.Lambda(returns, params, body) ->
      list.flatten([
        value_position(position, path),
        require(position != Pattern, path, "Lambdas are not patterns"),
        optional_type(returns, path <> ".returns"),
        parameter_errors(params, path <> ".parameters"),
        children(body, Body, path <> ".body"),
      ])
    ct.Let(name, kind, value) ->
      list.flatten([
        require(
          position == Fragment || position == Body,
          path,
          "Let bindings belong in function or scope bodies",
        ),
        name_errors(name, path),
        optional_type(kind, path <> ".type"),
        check(value, Value, path <> ".value"),
      ])
    ct.Const(name, kind, value) ->
      list.flatten([
        require(
          position == Fragment || position == Top,
          path,
          "Constants belong at module scope",
        ),
        name_errors(name, path),
        optional_type(kind, path <> ".type"),
        require(
          is_constant(value),
          path <> ".value",
          "Constants require unsigned literal values",
        ),
        check(value, Value, path <> ".value"),
      ])
    ct.TypeDeclaration(name, parameters, target, members) ->
      list.flatten([
        require(
          position == Fragment || position == Top,
          path,
          "Type declarations belong at module scope",
        ),
        name_errors(name, path),
        distinct(parameters, path <> ".parameters"),
        list.flat_map(parameters, fn(param) {
          name_errors(param, path <> ".parameters")
        }),
        require(
          target == None || members == None,
          path,
          "Choose an alias target or a constructor body, not both",
        ),
        optional_type(target, path <> ".alias"),
        case members {
          None -> []
          Some(members) ->
            list.flat_map(members, fn(member) {
              list.append(
                require(
                  case member {
                    ct.FunctionType(_, _) -> False
                    _ -> True
                  },
                  path <> ".members",
                  "Constructor members need names",
                ),
                type_errors(member, path <> ".members"),
              )
            })
        },
      ])
    ct.Import(module_path) ->
      list.append(
        require(
          position == Fragment || position == Top,
          path,
          "Imports belong at module scope",
        ),
        require(
          list.all(string.split(module_path, "."), valid_name),
          path,
          "Expected a dotted module path",
        ),
      )
    ct.Comment(_, _) ->
      require(
        position == Fragment || position == Body || position == Top,
        path,
        "Comments are statements, not values",
      )
    ct.CaseExpr(conditions, branches) ->
      list.flatten([
        value_position(position, path),
        require(position != Pattern, path, "Case expressions are not patterns"),
        require(
          conditions != [],
          path,
          "A case requires at least one condition",
        ),
        require(branches != [], path, "A case requires at least one branch"),
        children(conditions, Value, path <> ".conditions"),
        list.flat_map(branches, fn(branch) {
          list.flatten([
            require(
              list.length(branch.cases) == list.length(conditions),
              path <> ".branches",
              "Each branch needs one pattern per condition",
            ),
            children(branch.cases, Pattern, path <> ".patterns"),
            check(branch.return_value, Value, path <> ".result"),
          ])
        }),
      ])
    ct.ScopedExpr(body) ->
      list.append(
        value_position(position, path),
        children(body, Body, path <> ".body"),
      )
    ct.Call(name, args) ->
      list.flatten([
        value_position(position, path),
        name_errors(name, path),
        children(
          args,
          case position {
            Pattern -> Pattern
            _ -> Value
          },
          path <> ".arguments",
        ),
      ])
    ct.Pipeline(calls) ->
      list.flatten([
        value_position(position, path),
        require(position != Pattern, path, "Pipelines are not patterns"),
        require(
          list.length(calls) >= 2,
          path,
          "A pipeline requires at least two calls",
        ),
        require(
          list.all(calls, fn(call) {
            case call {
              ct.Call(_, _) -> True
              _ -> False
            }
          }),
          path,
          "Pipeline steps must be calls",
        ),
        children(calls, Value, path <> ".calls"),
      ])
    ct.Binary(_, left, right) ->
      list.flatten([
        value_position(position, path),
        check(left, Value, path <> ".left"),
        check(right, Value, path <> ".right"),
      ])
    ct.Unary(_, value) ->
      list.append(
        value_position(position, path),
        check(value, Value, path <> ".value"),
      )
    ct.ListExpr(values) | ct.TupleExpr(values) ->
      list.append(
        value_position(position, path),
        children(
          values,
          case position {
            Pattern -> Pattern
            _ -> Value
          },
          path <> ".values",
        ),
      )
    ct.Wildcard ->
      require(position == Pattern, path, "Wildcards are only valid in patterns")
    ct.Identifier(name) ->
      list.append(value_position(position, path), name_errors(name, path))
    ct.Literal(_) -> [
      GenerationError(
        path,
        "Raw Literal source is unchecked; use a structured construct",
      ),
    ]
    ct.Integer(_) | ct.Decimal(_) | ct.Text(_) | ct.Boolean(_) | ct.NilValue ->
      value_position(position, path)
  }
}
