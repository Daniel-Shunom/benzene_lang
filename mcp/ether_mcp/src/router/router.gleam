import gleam/bool
import gleam/http
import gleam/http/request
import gleam/list
import gleam/option as gleam_option
import gleam/string
import gleam/uri
import handlers/handler
import mcp/rpc
import wisp

pub fn router(req: wisp.Request) -> wisp.Response {
  use <- bool.guard(wisp.path_segments(req) != ["mcp"], wisp.not_found())
  use <- bool.guard(!allowed_origin(req), wisp.response(403))
  let version = request.get_header(req, "mcp-protocol-version")
  use <- bool.guard(
    case version {
      Error(_) -> False
      Ok(value) ->
        !list.contains(
          [rpc.protocol_version, "2025-03-26", "2025-06-18"],
          value,
        )
    },
    wisp.json_response("{\"error\":\"Unsupported MCP protocol version\"}", 400),
  )
  case req.method {
    http.Post -> {
      use <- bool.guard(
        case request.get_header(req, "content-type") {
          Error(_) -> True
          Ok(value) ->
            case string.split(string.lowercase(value), ";") {
              [media_type, ..] -> string.trim(media_type) != "application/json"
              _ -> True
            }
        },
        wisp.response(415),
      )
      handler.handler(req)
    }
    _ -> wisp.method_not_allowed([http.Post])
  }
}

fn allowed_origin(req: wisp.Request) -> Bool {
  case request.get_header(req, "origin") {
    Error(_) -> True
    Ok(origin) ->
      case uri.parse(origin) {
        Error(_) -> False
        Ok(origin) -> {
          case origin.scheme, origin.host {
            gleam_option.Some("http"), gleam_option.Some(host) ->
              list.contains(["localhost", "127.0.0.1", "[::1]", "::1"], host)
            _, _ -> False
          }
        }
      }
  }
}
