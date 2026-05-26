import handlers/handler
import gleam/bool
import gleam/http
import wisp

pub fn router(req: wisp.Request) -> wisp.Response {
  case req.method {
    http.Post -> {
      use <- bool.guard(
        wisp.path_segments(req) != ["mcp"],
        wisp.not_found()
      )
      handler.handler(req)
    }
    _ -> wisp.method_not_allowed([http.Post])
  }
}
