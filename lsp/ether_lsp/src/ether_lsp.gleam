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
      rpc.log("ether-lsp: using compiler at " <> executable)
      loop(server.new(executable))
    }
  }
}

fn loop(state: server.State) -> Nil {
  case rpc.read_message() {
    Error(reason) -> {
      // A read failure is the editor closing the pipe, which is the normal way
      // this process ends.
      rpc.log("ether-lsp: stopping (" <> reason <> ")")
      halt()
    }

    Ok(message) -> {
      let next = server.handle(state, message)
      case next.running {
        True -> loop(next)
        False -> halt()
      }
    }
  }
}
