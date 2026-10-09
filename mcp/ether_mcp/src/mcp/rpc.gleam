import generation
import gleam/dynamic.{type Dynamic}
import gleam/dynamic/decode
import gleam/json
import gleam/option.{type Option, None, Some}
import mcp/compiler
import mcp/decode as constructs
import mcp/json_value
import mcp/schema

pub const protocol_version = "2025-11-25"

pub type Reply {
  Reply(status: Int, body: Option(json.Json))
}

type Request {
  Request(id: Option(json.Json), method: String, params: Dynamic)
}

fn request_decoder() {
  use version <- decode.field("jsonrpc", decode.string)
  use id <- decode.optional_field(
    "id",
    None,
    decode.map(
      decode.one_of(decode.map(decode.int, json.int), [
        decode.map(decode.string, json.string),
      ]),
      Some,
    ),
  )
  use method <- decode.field("method", decode.string)
  use params <- decode.optional_field(
    "params",
    gleam_dynamic_null(),
    decode.dynamic,
  )
  case version == "2.0" {
    True -> decode.success(Request(id, method, params))
    False -> decode.failure(Request(id, method, params), "jsonrpc 2.0")
  }
}

fn gleam_dynamic_null() -> Dynamic {
  let assert Ok(value) = json.parse("null", decode.dynamic)
  value
}

pub fn handle(body: String) -> Reply {
  case json.parse(body, decode.dynamic) {
    Error(_) -> Reply(400, Some(error(json.null(), -32_700, "Invalid JSON")))
    Ok(value) ->
      case decode.run(value, request_decoder()) {
        Error(_) ->
          Reply(
            400,
            Some(error(json.null(), -32_600, "Invalid JSON-RPC request")),
          )
        Ok(Request(None, _, _)) -> Reply(202, None)
        Ok(Request(Some(id), method, params)) ->
          Reply(200, Some(dispatch(id, method, params)))
      }
  }
}

fn dispatch(id: json.Json, method: String, params: Dynamic) -> json.Json {
  case method {
    "initialize" -> {
      let decoder = {
        use _ <- decode.field("protocolVersion", decode.string)
        use _ <- decode.field(
          "capabilities",
          decode.dict(decode.string, decode.dynamic),
        )
        use _ <- decode.field("clientInfo", {
          use _ <- decode.field("name", decode.string)
          use _ <- decode.field("version", decode.string)
          decode.success(Nil)
        })
        decode.success(Nil)
      }
      case decode.run(params, decoder) {
        Error(_) -> error(id, -32_602, "Invalid initialization parameters")
        Ok(_) ->
          success(
            id,
            json.object([
              #("protocolVersion", json.string(protocol_version)),
              #("capabilities", json.object([#("tools", json.object([]))])),
              #(
                "serverInfo",
                json.object([
                  #("name", json.string("benzene-mcp")),
                  #("version", json.string("1.0.0")),
                ]),
              ),
              #(
                "instructions",
                json.string(
                  "Use generate_program with structured constructs. Validation uses the Benzene compiler. No source files are written or programs executed.",
                ),
              ),
            ]),
          )
      }
    }
    "ping" -> success(id, json.object([]))
    "tools/list" -> success(id, json.object([#("tools", tools())]))
    "tools/call" -> {
      let decoder = {
        use name <- decode.field("name", decode.string)
        use arguments <- decode.field("arguments", decode.dynamic)
        decode.success(#(name, arguments))
      }
      case decode.run(params, decoder) {
        Error(_) -> error(id, -32_602, "Expected tool name and arguments")
        Ok(#("generate_program", arguments)) -> success(id, generate(arguments))
        Ok(#("check_program", arguments)) -> success(id, check(arguments))
        Ok(_) -> error(id, -32_602, "Unknown tool")
      }
    }
    _ -> error(id, -32_601, "Method not found")
  }
}

pub fn tools() -> json.Json {
  let assert Ok(input_schema) =
    json.parse(schema.source(), json_value.decoder(0))
  json.preprocessed_array([
    tool(
      "generate_program",
      "Generate Benzene source from structured constructs. By default resolves symbols and type-checks it using ether. Use validate=false for individual fragments; no files are written.",
      input_schema,
    ),
    tool(
      "check_program",
      "Validate existing Benzene source using the real lexer, parser, resolver and type checker; no code is executed.",
      json.object([
        #("type", json.string("object")),
        #(
          "properties",
          json.object([
            #("source", json.object([#("type", json.string("string"))])),
            #(
              "path",
              json.object([
                #("type", json.string("string")),
                #("default", json.string("generated.bz")),
              ]),
            ),
          ]),
        ),
        #("required", json.array(["source"], json.string)),
        #("additionalProperties", json.bool(False)),
      ]),
    ),
  ])
}

fn tool(
  name: String,
  description: String,
  input_schema: json.Json,
) -> json.Json {
  json.object([
    #("name", json.string(name)),
    #("description", json.string(description)),
    #("inputSchema", input_schema),
    #(
      "annotations",
      json.object([
        #("readOnlyHint", json.bool(True)),
        #("openWorldHint", json.bool(False)),
      ]),
    ),
  ])
}

fn generate(arguments: Dynamic) -> json.Json {
  let decoder = {
    use construct <- decode.field("construct", constructs.construct_decoder(0))
    use validate <- decode.optional_field("validate", True, decode.bool)
    use path <- decode.optional_field("path", "generated.bz", decode.string)
    decode.success(#(construct, validate, path))
  }
  case decode.run(arguments, decoder) {
    Error(errors) -> decoding_error(errors)
    Ok(#(construct, validate, path)) ->
      case generation.generate(construct) {
        Error(errors) ->
          tool_result(
            True,
            json.object([
              #("stage", json.string("formation")),
              #(
                "errors",
                json.array(errors, fn(item) {
                  json.object([
                    #("path", json.string(item.path)),
                    #("message", json.string(item.message)),
                  ])
                }),
              ),
            ]),
          )
        Ok(source) ->
          case validate {
            True -> checked(source, path)
            False ->
              tool_result(
                False,
                json.object([
                  #("source", json.string(source)),
                  #("validated", json.bool(False)),
                ]),
              )
          }
      }
  }
}

fn check(arguments: Dynamic) -> json.Json {
  let decoder = {
    use source <- decode.field("source", decode.string)
    use path <- decode.optional_field("path", "generated.bz", decode.string)
    decode.success(#(source, path))
  }
  case decode.run(arguments, decoder) {
    Error(errors) -> decoding_error(errors)
    Ok(#(source, path)) -> checked(source, path)
  }
}

fn checked(source: String, path: String) -> json.Json {
  case compiler.check(path, source) {
    Error(message) ->
      tool_result(
        True,
        json.object([
          #("source", json.string(source)),
          #("validated", json.bool(False)),
          #("stage", json.string("compiler")),
          #("message", json.string(message)),
        ]),
      )
    Ok(report) ->
      tool_result(
        !report.valid,
        json.object([
          #("source", json.string(source)),
          #("validated", json.bool(True)),
          #("valid", json.bool(report.valid)),
          #("analysis", report.analysis),
        ]),
      )
  }
}

fn decoding_error(errors: List(decode.DecodeError)) -> json.Json {
  tool_result(
    True,
    json.object([
      #("stage", json.string("decoding")),
      #(
        "errors",
        json.array(errors, fn(item) {
          json.object([
            #("path", json.array(item.path, json.string)),
            #("expected", json.string(item.expected)),
            #("found", json.string(item.found)),
          ])
        }),
      ),
    ]),
  )
}

fn tool_result(failed: Bool, data: json.Json) -> json.Json {
  json.object([
    #("isError", json.bool(failed)),
    #("structuredContent", data),
    #(
      "content",
      json.preprocessed_array([
        json.object([
          #("type", json.string("text")),
          #("text", json.string(json.to_string(data))),
        ]),
      ]),
    ),
  ])
}

fn success(id: json.Json, result: json.Json) -> json.Json {
  json.object([
    #("jsonrpc", json.string("2.0")),
    #("id", id),
    #("result", result),
  ])
}

fn error(id: json.Json, code: Int, message: String) -> json.Json {
  json.object([
    #("jsonrpc", json.string("2.0")),
    #("id", id),
    #(
      "error",
      json.object([
        #("code", json.int(code)),
        #("message", json.string(message)),
      ]),
    ),
  ])
}
