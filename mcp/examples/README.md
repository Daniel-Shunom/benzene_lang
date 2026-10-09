# Generated file/HTTP application

`file_http.request.json` is the complete `tools/call` request for
`generate_program`. `file_http.bz` is the source returned by the running MCP
server, validated by the Benzene lexer, parser, symbol resolver and type checker.

Run `python mcp/test/test_mcp.py` from the repository root to regenerate and
verify it. With the MCP server already running, the saved request can also be
POSTed to `/mcp` after initialization.

The program copies an input file, serves the output via `GET /`, and accepts
writes via `POST /output`. All effects are represented by typed `Reader`,
`Writer` and `Server` callback arguments to `app`. It does not claim native
file/network support that Benzene has not yet implemented.
