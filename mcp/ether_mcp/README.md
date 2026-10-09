# Structured Benzene generation

`construct.gleam` models Benzene declarations, types, expressions and patterns.
`generation.generate` checks their formation and returns either generated source
or a list of `GenerationError(path, message)` values suitable for tool responses.
The HTTP/MCP handler decodes tool requests into these same constructs and can
validate the generated source with the actual compiler.

```gleam
import construct as ct
import generation
import gleam/option.{None, Some}

pub fn program() {
  generation.generate(ct.Module([
    ct.TypeDeclaration("Result", ["a", "b"], None, Some([
      ct.AppliedType("Ok", [ct.NamedType("a")]),
      ct.AppliedType("Error", [ct.NamedType("b")]),
    ])),
    ct.TypeDeclaration("Handler", ["data"],
      Some(ct.FunctionType([ct.NamedType("data")], ct.NamedType("String"))), None),
    ct.Function("main", Some(ct.AppliedType("Result", [ct.NamedType("Int"), ct.NamedType("String")])), [], [ct.Call("Ok", [ct.Integer(42)])]),
  ]))
}
```

Formation validates scope, identifier spelling, duplicate parameters and field
labels, constant literals, case branch arity, wildcard placement, and pipeline
shape. It accepts individual fragments as well as complete modules. A fragment
such as a let binding still needs to be placed inside a function in a program.
Symbol existence, generic parameter consistency, constructor arity, alias
expansion, inference and type compatibility remain the compiler's responsibility.
A successful generation result is not a guarantee of semantic correctness.

## Supported constructs

| Category | Structured constructors |
| --- | --- |
| Declarations | `Module`, `Import`, `TypeDeclaration`, `Function`, `Let`, `Const` |
| Types | `NamedType`, `AppliedType`, `FunctionType`, `LabelledType`, `TypeField` |
| Functions | `FuncParam`, `Lambda`, `Call`, `Pipeline` |
| Control flow | `CaseExpr`, `CaseEval`, `ScopedExpr` |
| Values | `Identifier`, `Integer`, `Decimal`, `Text`, `Boolean`, `NilValue`, `ListExpr`, `TupleExpr` |
| Operations | `Binary`, `Unary` with structured operator enums |
| Patterns | Value/collection constructs, constructor `Call`, `Identifier`, `Wildcard` |
| Comments | `Comment(SingleLine, text)`, `Comment(MultiLine, text)` |

Use `None` for inferred annotations. Type declarations take either an alias
target or constructor members. `members: None` emits a bare declaration;
`Some([])` emits an explicit empty body. Generic constructor fields should refer
to the parent's declared parameters. `LabelledType` represents labelled fields,
including nested type applications. Nullary patterns use `Identifier("Name")`;
constructors with fields use `Call("Name", [...])`. Each `CaseEval` needs one
pattern per case condition; its result can be a scope with bindings.

The renderer emits `end` without a period, `Fn(...) :> ...` lambdas and function
types, `@{...}` tuples, `Load` imports, and two-space indentation. It escapes
strings/comments and expands scientific float notation to decimal literals.
Nested operations and non-primary call arguments use brace scopes to preserve
grouping, since Benzene has no parenthesized expression grouping. Constants use
unsigned literals, matching the parser. `%` is omitted because the current
expression parser does not support it. Pipeline steps retain explicit arguments:
the current type checker does not insert the previous call's result as an argument.

`construct.to_string(construct, indent)` is the unchecked rendering API. The old
`Literal(String)` escape hatch is retained there, but validated generation rejects
it. Migrate old string type annotations to `NamedType`/`AppliedType` and other
structured types. Prefer typed values and identifiers to raw source strings.

## Verification

From this directory:

```sh
gleam test
gleam run
```

`gleam run` prints a valid example using generic constructors, a function alias,
and pattern matching. From the repository root, with the compiler built:

```sh
python mcp/test/check_generated.py
```

An optional first argument selects another compiler executable. This check
generates five complete programs and feeds them through `ether scan` using its
stdin protocol, checking lexer, parser, resolution and type diagnostics, symbol
index output, and string escape round-tripping. It exercises aliases, cases,
comments, nested operations, lambdas, scopes, collections and pipelines.

## MCP server

From this directory, run:

```sh
gleam run -m mcp/server
```

The endpoint is `http://127.0.0.1:4000/mcp`. Set `BENZENE_MCP_PORT` to change the
port. Set `ETHER_BIN` to your compiler executable; the default searches the
checkout's `../../bin/ether[.exe]`, then PATH. Each compiler call has a 15-second
deadline. Source is sent using the compiler's length-prefixed stdin protocol;
validation creates no source files and never executes a program.

This server uses the [MCP 2025-11-25](https://modelcontextprotocol.io/specification/2025-11-25/basic/transports)
Streamable HTTP transport with JSON responses, stateless operation, and no SSE
stream. `GET /mcp` returns 405. `initialize`, `ping`, `tools/list`, `tools/call`,
and notifications are supported. Initialization returns the supported protocol
version; use it in subsequent `MCP-Protocol-Version` headers. POST requests use
`Content-Type: application/json` and `Accept: application/json, text/event-stream`.
Notifications return 202 with an empty body. The server binds to loopback and
rejects nonlocal Origin headers. It is intended for local use, without a public
authentication layer.

| Tool | Arguments | Result |
| --- | --- | --- |
| `generate_program` | `construct`, optional `validate` (default true), optional `path` | Generated source, formation errors, and compiler report when validated |
| `check_program` | `source`, optional `path` | Compiler diagnostics, tokens and inferred symbol types |

Tool discovery includes a recursive JSON Schema for every supported construct
and type. Construct/type objects use `kind` equal to the constructor name and
fields matching the Gleam model. Enum values such as operators are strings.
Annotations can be omitted or null. For example:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "tools/call",
  "params": {
    "name": "generate_program",
    "arguments": {
      "construct": {
        "kind": "Function",
        "identifier": "main",
        "return_type": {"kind": "NamedType", "name": "Int"},
        "body": [{"kind": "Integer", "value": 42}]
      }
    }
  }
}
```

Each tool result includes `structuredContent` and matching JSON text content.
Compiler-checked results have `validated: true` and a separate `valid` flag;
diagnostics and symbol types are in `analysis`. `isError` is true for failed
decoding, formation, compiler invocation, or compilation. For fragments, set
`validate: false`: formation still runs, but the result has `validated: false`.
`path` is a logical filename used in compiler reporting, not an output location.

Run the real HTTP integration test from the repository root:

```sh
python mcp/test/test_mcp.py
```

It starts the server on an available loopback port, initializes it, discovers
tools, and asks it to generate a file/HTTP application without providing raw
Benzene syntax. It checks the source with both tools and exercises undefined
symbols, type mismatches, invalid constructs, notifications and transport errors.
The test saves `mcp/examples/file_http.bz` and `file_http.request.json`, then stops
the server. It requires Gleam, Erlang and the built compiler.

The generated application copies `input.txt` to `output.txt`, handles `GET /`
by reading the output file, handles `POST /output` by writing the request body,
and returns 404/500 responses where appropriate. Its `app` function takes typed
`Reader`, `Writer` and `Server` callbacks and asks `Server` to listen on port 8080.
Benzene currently has no native file/HTTP APIs or host callback integration, so
this is validated application logic for hypothetical runtime adapters, not an
executable file/HTTP server. The MCP HTTP server itself runs in Gleam/Erlang.
