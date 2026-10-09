import gleam/dynamic/decode
import gleam/json
import gleam/option.{None, Some}

/// Preserve JSON values (including string/integer request IDs) without an
/// unchecked cast between Dynamic and Json.
pub fn decoder(depth: Int) -> decode.Decoder(json.Json) {
  use _ <- decode.then(decode.dynamic)
  case depth >= 80 {
    True -> decode.failure(json.null(), "JSON nesting below 80 levels")
    False ->
      decode.one_of(decode.map(decode.int, json.int), [
        decode.map(decode.float, json.float),
        decode.map(decode.bool, json.bool),
        decode.map(decode.string, json.string),
        decode.map(decode.list(decoder(depth + 1)), json.preprocessed_array),
        decode.map(decode.dict(decode.string, decoder(depth + 1)), fn(values) {
          json.dict(values, fn(key) { key }, fn(value) { value })
        }),
        decode.then(decode.optional(decode.string), fn(value) {
          case value {
            None -> decode.success(json.null())
            Some(_) -> decode.failure(json.null(), "null")
          }
        }),
      ])
  }
}
