//// Language server for Benzene.
////
//// Speaks LSP over stdin/stdout and answers every question by running the
//// `ether` compiler's `scan` subcommand, so diagnostics, hover types and
//// highlighting all come from the same front-end that builds the code.

import lsp/rpc
import lsp/server

@external(erlang, "ether_lsp_ffi", "find_compiler")
fn find_compiler() -> Result(String, String)

@external(erlang, "ether_lsp_ffi", "halt")
fn halt() -> Nil

/// Runs `work`, returning `fallback` if it throws.
@external(erlang, "ether_lsp_ffi", "guard")
fn guard(work: fn() -> server.State, fallback: server.State) -> server.State

pub fn main() -> Nil {
  rpc.configure_stdio()

  case find_compiler() {
    Error(reason) -> {
      // Nothing useful can be served without the compiler, and failing loudly
      // here is far easier to diagnose than an editor that silently never
      // reports anything.
      rpc.log("ether-lsp: " <> reason)
      halt()
    }

    Ok(executable) -> {
      rpc.log(
        "ether-lsp " <> server.version() <> ": using compiler at " <> executable,
      )
      rpc.start_reader()
      loop(server.new(executable))
    }
  }
}

/// The message loop.
///
/// Waiting is bounded only when analysis is queued. In that case the timeout is
/// the debounce interval, and reaching it is the signal that the client has
/// stopped typing and the work should run.
fn loop(state: server.State) -> Nil {
  case rpc.receive_frame(server.idle_timeout(state)) {
    rpc.Idle -> loop(guard(fn() { server.flush(state) }, state))

    rpc.Closed(reason) -> {
      // The editor closing the pipe is the normal way this process ends.
      rpc.log("ether-lsp: stopping (" <> reason <> ")")
      halt()
    }

    rpc.Frame(message) -> {
      // One malformed request must not end the session, so a crash inside a
      // handler costs that message and nothing else. The fallback is the state
      // from before the message, which is the last state known to be good.
      let next = guard(fn() { server.handle(state, message) }, state)
      case next.running {
        True -> loop(next)
        False -> halt()
      }
    }
  }
}
