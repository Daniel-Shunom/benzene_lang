# benzene_lang

A small statically-typed language and its compiler front-end (`ether`).

Status: early. Lexer, parser, symbol resolver, and diagnostics are working.
Type inference/checking is implemented. Executable code generation is still a stub.

## Install

On Windows, install the compiler, LSP and MCP for your user account:

```powershell
.\install.ps1 -AddToPath
```

The default location is `%LOCALAPPDATA%\Programs\Benzene`. Open a new terminal
and run `ether --version`, `ether new my_project`, or `ether-mcp`. Erlang/OTP is
required for the servers; Gleam is only needed when building them. POSIX systems
can use `sh install.sh`. See [installation](docs/installation.md) for prerequisites,
custom prefixes, portable packages and editor configuration.

## Build

Requires CMake (>=3.16), Ninja, and a C++26-capable compiler (GCC 14+ /
Clang 19+).

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

Builds default to one compiler/linker worker to limit memory use. To opt into
parallel compilation with Ninja, set the pool size during configuration:

```sh
cmake -S . -B build -G Ninja -DETHER_BUILD_JOBS=2
cmake --build build --parallel 2
```

On Windows, the build script defaults to release mode using one worker:

```bat
build.bat
build.bat debug
build.bat release 2
build.bat debug 1
```

The script requires CMake and Ninja and keeps separate incremental build
directories under `build/native-Debug` and `build/native-Release`. The worker
count limits simultaneous compilation and linking jobs. Increase it gradually
only if you have sufficient free RAM. `build.bat debug all` explicitly opts into
one worker per logical processor, which can exhaust memory. For Ninja, the
`ETHER_BUILD_JOBS` pool also limits plain `cmake --build build` and direct Ninja
invocations; `--parallel` alone cannot exceed the configured pool size. Other
generators should use `cmake --build build --parallel 1` for the same safe limit.

The CLI lands at `bin/ether` (`bin/ether.exe` on Windows).

## Test

```sh
./test.sh        # POSIX shells
test.bat         # Windows
```

Both configure on first run, build the test binary, and invoke `ctest`.
Extra arguments pass through to ctest, e.g. `./test.sh -R lexer`.

## Try it

```sh
./bin/ether check tests/integration/samples/valid_program.bz -show-ast
./bin/ether help
```

Create a Benzene project with a `Nil`-returning starter and an agent MCP entrypoint:

```sh
./bin/ether new my_project
cd my_project
ether check src/main.bz
```

See [project generation](docs/project-generation.md) for the generated layout,
optional Git initialization, and agent configuration.

## Editor support

> [!WARNING]
> Highly experimental and largely AI-assisted. See
> [`lsp/README.md`](lsp/README.md) before relying on any of it.

A language server lives in [`lsp/`](lsp/README.md), written in Gleam and driven
entirely by this front-end — it owns no lexer, parser or type checker of its
own, so it cannot disagree with the compiler about your program.

Live type checking as you type, completion, hover types, go-to-definition,
references, rename, inlay hints showing inferred types, signature help, folding,
semantic highlighting, and a code action that writes an inferred type down. The
Neovim plugin that ships with it also closes `func`/`case`/`Fn` blocks as you
open them.

## Layout

```
core/   library — lexer, parser, AST, passes, diagnostics
cli/    ether executable
lsp/    language server (Gleam) + Neovim plugin — experimental
tests/  unit + integration tests (doctest)
docs/   grammar specification
```

See [`docs/grammar.md`](docs/grammar.md) for the language grammar and
[`CONTRIBUTING.md`](CONTRIBUTING.md) for development notes.

## License

MIT. See [`LICENSE`](LICENSE).
