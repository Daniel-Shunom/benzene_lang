# benzene_lang

A small statically-typed language and its compiler front-end (`ether`).

Status: early. Lexer, parser, symbol resolver, and diagnostics are working.
Type checker and codegen are stubs.

## Build

Requires CMake (>=3.16), Ninja, and a C++26-capable compiler (GCC 14+ /
Clang 19+).

```sh
cmake -S . -B build -G Ninja
cmake --build build
```

The CLI lands at `bin/ether`.

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
