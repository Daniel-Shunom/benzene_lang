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

## Layout

```
core/   library — lexer, parser, AST, passes, diagnostics
cli/    ether executable
tests/  unit + integration tests (doctest)
docs/   grammar specification
```

See [`docs/grammar.md`](docs/grammar.md) for the language grammar and
[`CONTRIBUTING.md`](CONTRIBUTING.md) for development notes.

## License

MIT. See [`LICENSE`](LICENSE).
