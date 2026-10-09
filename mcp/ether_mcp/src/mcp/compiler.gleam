import gleam/dynamic/decode
import gleam/json
import gleam/list
import gleam/result
import mcp/json_value

pub type Report {
  Report(valid: Bool, analysis: json.Json)
}

@external(erlang, "ether_mcp_ffi", "scan")
fn scan(path: String, source: String) -> Result(String, String)

pub fn check(path: String, source: String) -> Result(Report, String) {
  use output <- result.try(scan(path, source))
  use analysis <- result.try(
    json.parse(output, json_value.decoder(0))
    |> result.replace_error("Compiler returned malformed JSON"),
  )
  let decoder = {
    use severities <- decode.field(
      "diagnostics",
      decode.list({
        use severity <- decode.field("severity", decode.string)
        decode.success(severity)
      }),
    )
    decode.success(!list.contains(severities, "error"))
  }
  use valid <- result.try(
    json.parse(output, decoder)
    |> result.replace_error("Compiler report is missing diagnostics"),
  )
  Ok(Report(valid, analysis))
}
