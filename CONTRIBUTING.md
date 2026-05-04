# Contributing

Thanks for poking around. This is a small project; expectations are light.

## Before you open a PR

1. `./test.sh` (or `test.bat`) must pass.
2. New behavior gets a test. New surface tokens / syntax gets a grammar update
   in [`docs/grammar.md`](docs/grammar.md).
3. Keep diffs focused — separate refactors from features.

## Style

- C++26, two-space indent, snake_case methods/variables, PascalCase types.
- Prefer existing patterns: the visitor pattern for AST passes, the parser
  combinators in `core/include/ether/parser/parser_types.hpp`, and
  `DiagnosticEngine` for any user-facing error.
- No `goto`. Restructure with helpers, flags, or inner loops.
- Comments are for non-obvious *why*, not *what*.

## Reporting issues

Open a GitHub issue with:
- a minimal `.bz` snippet,
- what you expected,
- what `ether check` actually printed.

## Layout cheat sheet

```
core/include/ether/   public headers
core/src/             implementations
cli/src/              CLI commands
tests/unit/           per-component tests
tests/integration/    full-pipeline tests against samples/
docs/                 grammar and design notes
```
