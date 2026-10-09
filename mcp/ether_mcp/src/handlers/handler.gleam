import gleam/bit_array
import gleam/json
import gleam/option.{None, Some}
import mcp/rpc
import wisp

pub fn handler(req: wisp.Request) -> wisp.Response {
  case wisp.read_body_bits(req) {
    Error(_) ->
      wisp.json_response("{\"error\":\"Request body could not be read\"}", 400)
    Ok(body) ->
      case bit_array.to_string(body) {
        Error(_) ->
          wisp.json_response("{\"error\":\"Request body must be UTF-8\"}", 400)
        Ok(body) -> {
          let reply = rpc.handle(body)
          case reply.body {
            None -> wisp.response(reply.status)
            Some(value) ->
              wisp.json_response(json.to_string(value), reply.status)
          }
        }
      }
  }
}
