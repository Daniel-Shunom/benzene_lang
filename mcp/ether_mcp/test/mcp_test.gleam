import construct as ct
import gleam/dynamic/decode
import gleam/http
import gleam/http/request
import gleam/json
import gleam/option.{Some}
import mcp/decode as constructs
import mcp/json_value
import mcp/rpc
import router/router
import wisp/simulate

fn error_code(body: String) -> Int {
  let decoder = {
    use code <- decode.subfield(["error", "code"], decode.int)
    decode.success(code)
  }
  let assert Some(reply) = rpc.handle(body).body
  let assert Ok(code) = json.parse(json.to_string(reply), decoder)
  code
}

pub fn malformed_json_and_rpc_test() {
  assert error_code("{") == -32_700
  assert error_code("[]") == -32_600
  assert error_code("{\"jsonrpc\":\"1.0\",\"method\":\"ping\",\"id\":1}")
    == -32_600
  assert error_code("{\"jsonrpc\":\"2.0\",\"method\":\"missing\",\"id\":1}")
    == -32_601
}

pub fn initialization_parameters_test() {
  assert error_code(
      "{\"jsonrpc\":\"2.0\",\"method\":\"initialize\",\"params\":{},\"id\":1}",
    )
    == -32_602
}

pub fn numeric_request_id_test() {
  let assert Some(reply) =
    rpc.handle("{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":42}").body
  let assert Ok(id) =
    json.parse(json.to_string(reply), {
      use id <- decode.field("id", decode.int)
      decode.success(id)
    })
  assert id == 42
}

pub fn json_values_roundtrip_test() {
  let assert Ok(value) =
    json.parse(
      "[null,true,1,1.25,\"text\",{\"nested\":[]}]",
      json_value.decoder(0),
    )
  let assert Ok(again) =
    json.parse(json.to_string(value), json_value.decoder(0))
  assert json.to_string(value) == json.to_string(again)
}

pub fn nesting_limit_test() {
  assert json.parse("{\"kind\":\"NilValue\"}", constructs.construct_decoder(64))
    |> is_error
}

pub fn collection_and_operator_decoding_test() {
  let source =
    "{\"kind\":\"TupleExpr\",\"values\":[{\"kind\":\"Decimal\",\"value\":1.25},{\"kind\":\"ListExpr\",\"values\":[{\"kind\":\"Boolean\",\"value\":true}]},{\"kind\":\"Binary\",\"operator\":\"Add\",\"left\":{\"kind\":\"Integer\",\"value\":1},\"right\":{\"kind\":\"Unary\",\"operator\":\"Negate\",\"value\":{\"kind\":\"Integer\",\"value\":2}}}]}"
  assert json.parse(source, constructs.construct_decoder(0))
    == Ok(
      ct.TupleExpr([
        ct.Decimal(1.25),
        ct.ListExpr([ct.Boolean(True)]),
        ct.Binary(ct.Add, ct.Integer(1), ct.Unary(ct.Negate, ct.Integer(2))),
      ]),
    )
}

pub fn invalid_nested_field_reports_path_test() {
  let assert Ok(value) =
    json.parse(
      "{\"kind\":\"Call\",\"identifier\":\"f\",\"arguments\":[{\"kind\":\"Integer\",\"value\":\"wrong\"}]}",
      decode.dynamic,
    )
  let assert Error(errors) = decode.run(value, constructs.construct_decoder(0))
  assert errors
    == [decode.DecodeError("Int", "String", ["arguments", "0", "value"])]
}

fn is_error(value: Result(a, b)) -> Bool {
  case value {
    Error(_) -> True
    Ok(_) -> False
  }
}

pub fn handler_roundtrip_test() {
  let reply =
    simulate.request(http.Post, "/mcp")
    |> simulate.string_body(
      "{\"jsonrpc\":\"2.0\",\"method\":\"ping\",\"id\":\"client\"}",
    )
    |> request.set_header("content-type", "application/json; charset=utf-8")
    |> router.router
  assert reply.status == 200
  let assert Ok(id) =
    json.parse(simulate.read_body(reply), {
      use id <- decode.field("id", decode.string)
      decode.success(id)
    })
  assert id == "client"
}

pub fn notification_has_empty_body_test() {
  let reply =
    simulate.request(http.Post, "/mcp")
    |> simulate.string_body(
      "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}",
    )
    |> request.set_header("content-type", "application/json")
    |> router.router
  assert reply.status == 202
  assert simulate.read_body(reply) == ""
}

pub fn media_type_and_origin_test() {
  let req =
    simulate.request(http.Post, "/mcp")
    |> simulate.string_body("{}")
  assert router.router(req).status == 415
  assert router.router(
      req |> request.set_header("content-type", "application/json-invalid"),
    ).status
    == 415
  assert router.router(
      req |> request.set_header("origin", "http://127.0.0.1.evil.example"),
    ).status
    == 403
}
