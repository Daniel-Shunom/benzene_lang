//// The LSP base protocol: `Content-Length` framed JSON over stdin/stdout.
////
//// Reading happens in a separate process that forwards each decoded message to
//// the server. That split is what keeps the server responsive: analysis shells
//// out to the compiler, and while it does, the reader keeps draining input so a
//// burst of keystrokes arrives as one backlog to collapse rather than a queue
//// of checks to run one after another.
////
//// stdout carries the protocol and nothing else. Anything the server wants to
//// say to a human goes to stderr via `log`, which Neovim collects into
//// `:LspLog`.

import gleam/bit_array
import gleam/dynamic.{type Dynamic}
import gleam/dynamic/decode
import gleam/int
import gleam/json
import gleam/result
import gleam/string
import lsp/scan

/// An Erlang process identifier. Opaque here; it is only ever handed back to
/// the FFI that produced it.
pub type Pid

/// What the server's mailbox produced.
pub type Incoming {
  /// A decoded message from the client.
  Frame(Dynamic)
  /// The connection ended. Normal shutdown looks like this.
  Closed(String)
  /// A background scan finished, carrying the tag it was started with.
  Scanned(scan.Ref, Result(BitArray, String))
  /// The timeout elapsed with nothing waiting, so queued work can be flushed.
  Idle
}

/// Why a read did not produce a message.
type ReadError {
  /// The stream ended or broke. Nothing more will arrive.
  StreamClosed(String)
  /// This message was unusable, but the stream is still good.
  Malformed(String)
}

@external(erlang, "ether_lsp_ffi", "configure_stdio")
pub fn configure_stdio() -> Nil

@external(erlang, "ether_lsp_ffi", "log")
pub fn log(message: String) -> Nil

@external(erlang, "ether_lsp_ffi", "monotonic_ms")
pub fn now_ms() -> Int

@external(erlang, "ether_lsp_ffi", "read_line")
fn read_line() -> Result(BitArray, String)

@external(erlang, "ether_lsp_ffi", "read_bytes")
fn read_bytes(count: Int) -> Result(BitArray, String)

@external(erlang, "ether_lsp_ffi", "write_stdout")
fn write_stdout(data: BitArray) -> Nil

@external(erlang, "ether_lsp_ffi", "spawn_reader")
fn spawn_reader(run: fn() -> Nil) -> Nil

@external(erlang, "ether_lsp_ffi", "self_pid")
fn self_pid() -> Pid

@external(erlang, "ether_lsp_ffi", "send_frame")
fn send_frame(to: Pid, message: Dynamic) -> Nil

@external(erlang, "ether_lsp_ffi", "send_closed")
fn send_closed(to: Pid, reason: String) -> Nil

/// Waits for the next message. A negative timeout waits indefinitely.
@external(erlang, "ether_lsp_ffi", "receive_frame")
pub fn receive_frame(timeout_ms: Int) -> Incoming

/// Waits only for a finished scan, leaving client messages queued.
@external(erlang, "ether_lsp_ffi", "await_scan")
pub fn await_scan(timeout_ms: Int) -> Incoming

/// Starts the reader. Call once, from the process that will consume messages.
pub fn start_reader() -> Nil {
  let server = self_pid()
  spawn_reader(fn() { read_forever(server) })
}

fn read_forever(server: Pid) -> Nil {
  case read_message() {
    Ok(message) -> {
      send_frame(server, message)
      read_forever(server)
    }

    // A message we could not parse is this message's problem, not the
    // stream's: the framing told us exactly how many bytes to skip, so the
    // next one is still readable.
    Error(Malformed(reason)) -> {
      log("ether-lsp: ignoring unreadable message (" <> reason <> ")")
      read_forever(server)
    }

    Error(StreamClosed(reason)) -> send_closed(server, reason)
  }
}

fn read_message() -> Result(Dynamic, ReadError) {
  use length <- result.try(read_headers(0))
  use body <- result.try(read_bytes(length) |> result.map_error(StreamClosed))
  use text <- result.try(
    bit_array.to_string(body)
    |> result.replace_error(Malformed("body was not valid utf-8")),
  )

  json.parse(text, decode.dynamic)
  |> result.replace_error(Malformed("body was not valid json"))
}

/// Consumes header lines up to the blank separator, returning Content-Length.
/// Unknown headers are ignored, as the spec requires.
fn read_headers(length: Int) -> Result(Int, ReadError) {
  use line <- result.try(read_line() |> result.map_error(StreamClosed))
  use text <- result.try(
    bit_array.to_string(line)
    |> result.replace_error(Malformed("header was not valid utf-8")),
  )

  case string.trim(text) {
    "" ->
      case length {
        // Without a length there is no way to know where this message ends,
        // so the stream can no longer be resynchronised.
        0 -> Error(StreamClosed("message had no Content-Length header"))
        found -> Ok(found)
      }
    header ->
      case content_length(header) {
        Ok(found) -> read_headers(found)
        Error(_) -> read_headers(length)
      }
  }
}

fn content_length(header: String) -> Result(Int, Nil) {
  case string.split_once(header, ":") {
    Ok(#(name, value)) ->
      case string.lowercase(string.trim(name)) {
        "content-length" -> int.parse(string.trim(value))
        _ -> Error(Nil)
      }
    Error(_) -> Error(Nil)
  }
}

/// Frames and writes one message. Length is in bytes, not characters, which is
/// why the body is measured after encoding.
pub fn write_message(payload: json.Json) -> Nil {
  let body = bit_array.from_string(json.to_string(payload))
  let header =
    "Content-Length: " <> int.to_string(bit_array.byte_size(body)) <> "\r\n\r\n"

  write_stdout(bit_array.append(bit_array.from_string(header), body))
}
