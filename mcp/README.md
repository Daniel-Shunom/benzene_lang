# Benzene construct generation

The active generation prototype lives in `ether_mcp/` (Gleam). It turns
structured constructs into source accepted by the current Benzene front end.
Agents can describe a program as constructs instead of assembling language
syntax themselves. See [the generation API](ether_mcp/README.md).

`include/` and `src/` contain earlier, unfinished C++ builder experiments. They
are not used by the Gleam generator or included by the repository's root build.

The HTTP server now exposes `POST /mcp` with initialization, tool discovery,
structured generation and compiler-backed validation. See the generation API
documentation for startup and test commands. It binds to loopback and runs
`ether scan` over stdin; it does not write or execute generated programs.

[examples/file_http.bz](examples/file_http.bz) is a generated and type-checked
application that describes file copying and HTTP routes through typed host
callbacks. The corresponding JSON MCP request is saved alongside it.
