# Project generation

```sh
ether new my_project
cd my_project
ether check src/main.bz
```

`ether create my_project` is an alias. Names may contain ASCII letters, digits,
underscores and hyphens; a leading hyphen and Windows device names are rejected.
The command creates one directory in the current working directory. It refuses
an existing destination, including a file or symlink, without overwriting it.

The generated layout is:

```text
my_project/
  README.md
  AGENTS.md
  .mcp.json
  .gitignore
  src/
    main.bz
  .git/             # when Git is available
```

The README starts with the project name and a standard Benzene introduction.
The starter has no runtime dependencies:

```benzene
func main() :> Nil
  Nil
end
```

If Git is available, `ether` runs `git init` in the new directory. It does not
stage files, make an initial commit, or configure a remote. If Git is missing,
project generation still succeeds and explains that initialization was skipped.
If Git is found but initialization fails, the command returns a failure status
and leaves the generated files for inspection; retry `git init` in the project.
No files are removed on a creation failure.

## Agents and MCP

`AGENTS.md` is the project-root entrypoint for agents. It explains how to connect,
initialize, discover tools with `tools/list`, use their recursive schemas, and
generate Benzene constructs through `generate_program`. It requires compiler
validation before accepting or writing generated code, followed by
`check_program` after source edits. It explains formation-only fragments and
how to handle diagnostics or an unavailable compiler/server.

`.mcp.json` points to the local HTTP MCP server at
`http://127.0.0.1:4000/mcp`. Whether that file is loaded automatically depends on
the agent client; otherwise register the endpoint in its MCP settings. Edit the
URL when using another port. The server is a separate service and is not started
by `ether new`. With the [toolchain installed](installation.md), start it with:

```sh
ether-mcp
```

From a compiler checkout, use:

```sh
cd mcp/ether_mcp
gleam run -m mcp/server
```

Set `ETHER_BIN` on the server to the desired compiler executable when needed.
The project can still be checked locally with `ether scan src/main.bz`; this
returns JSON diagnostics and inferred symbol types. Always inspect diagnostics,
rather than relying solely on the command's exit status.

Printing, native file/network operations and executable code generation remain
future language work. The entrypoint tells agents to use typed callbacks for
hypothetical effects and to avoid claiming those effects can already run.

## Verification

With the CLI built, run from the repository root:

```sh
python tests/integration/test_project_generation.py
```

An optional argument selects another compiler executable. This test exercises
generation in a path containing spaces, Git initialization/absence/failure,
existing files/directories, invalid names and arguments, the `create` alias,
and actual syntax, symbol resolution and type checking of the starter. CTest
also registers it as `cli/project-generation` when Python is available.
