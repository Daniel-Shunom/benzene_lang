import gleam/erlang/process
import mist
import router/router
import wisp/wisp_mist

@external(erlang, "ether_mcp_ffi", "port")
fn port() -> Int

pub fn main() -> Nil {
  let assert Ok(_) =
    router.router
    |> wisp_mist.handler(
      "benzene-mcp-local-server-does-not-use-cookies-or-session-signatures",
    )
    |> mist.new
    |> mist.bind("127.0.0.1")
    |> mist.port(port())
    |> mist.start
  process.sleep_forever()
}
