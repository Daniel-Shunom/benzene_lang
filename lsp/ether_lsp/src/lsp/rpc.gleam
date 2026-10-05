//// The LSP base protocol: `Content-Length` framed JSON over stdin/stdout.
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

@external(erlang, "ether_lsp_ffi", "configure_stdio")
pub fn configure_stdio() -> Nil

@external(erlang, "ether_lsp_ffi", "log")
pub fn log(message: String) -> Nil

@external(erlang, "ether_lsp_ffi", "read_line")
fn read_line() -> Result(BitArray, String)

@external(erlang, "ether_lsp_ffi", "read_bytes")
fn read_bytes(count: Int) -> Result(BitArray, String)

@external(erlang, "ether_lsp_ffi", "write_stdout")
fn write_stdout(data: BitArray) -> Nil

/// Blocks until one complete message arrives. An `Error` here means the
/// connection is unusable -- the caller should stop, not retry.
pub fn read_message() -> Result(Dynamic, String) {
  use length <- result.try(read_headers(0))
  use body <- result.try(read_bytes(length))
  use text <- result.try(
    bit_array.to_string(body)
    |> result.replace_error("message body was not valid utf-8"),
  )
  json.parse(text, decode.dynamic)
  |> result.replace_error("message body was not valid json")
}

/// Consumes header lines up to the blank separator, returning Content-Length.
/// Unknown headers are ignored, as the spec requires.
fn read_headers(length: Int) -> Result(Int, String) {
  use line <- result.try(read_line())
  use text <- result.try(
    bit_array.to_string(line)
    |> result.replace_error("header was not valid utf-8"),
  )

  case string.trim(text) {
    "" ->
      case length {
        0 -> Error("message had no Content-Length header")
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
