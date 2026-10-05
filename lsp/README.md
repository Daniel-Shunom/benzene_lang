# ether-lsp — a language server for Benzene

> [!WARNING]
> **Highly experimental, and mostly AI-generated.**
>
> This whole directory (plus the `ether scan` subcommand that backs it) was
> written by Claude across two automated sessions and has not been reviewed line
> by line. It is tested — see [Status](#status) for exactly what is verified and
> how — but treat it as a starting point to read and rewrite, not as trusted
> code. Expect rough edges outside the paths the tests cover.

A language server for Benzene, written in Gleam. Live type checking, completion,
hover types, go-to-definition, references, rename, inlay hints, signature help,
folding, and semantic highlighting.

## How it works

The server owns no lexer, parser or type checker of its own. Every answer comes
from the real compiler:

```
            ┌──────────────┐   LSP/stdio   ┌───────────────────────┐
  Neovim ──▶│    reader    │──────────────▶│   server (main loop)  │
            │   process    │               │   state, dispatch     │
            └──────────────┘               └───────────┬───────────┘
                                                       │ length-prefixed stdin
                                                       ▼
                                           ┌───────────────────────┐
                                           │  ether scan  (C++)    │
                                           │  lex → parse →        │
                                           │  resolve → typecheck  │
                                           └───────────────────────┘
```

`ether scan` is a subcommand added for this purpose. It runs the same pipeline
as `ether check` and prints the result as JSON: tokens for highlighting,
diagnostics, and an index of every identifier with its solved type, declaration
site, enclosing scope, and whether the user annotated it.

The buffer is passed to the compiler on **stdin**, length-prefixed, so what gets
checked is what you are looking at. You do not have to save.

The point of the split is that there is exactly one type checker. Reimplementing
inference in Gleam would mean the editor and the compiler could disagree about
your program, which is worse than having no editor support at all.

### Nothing waits for the compiler

Checking shells out to `ether`, and that is not fast: about 0.7s on a
2000-line file, and 15s on a 10000-line one. Three things keep that off the
critical path.

**Reading is a separate process.** It drains stdin into the server's mailbox,
so input is accepted no matter what the server is doing.

**Checking is a separate process.** The server starts a check and goes back to
its loop; the result arrives later as a message. However slow a check is, the
server keeps reading, replying and publishing throughout.

**Edits debounce and collapse.** A `didChange` only marks the document dirty.
The check runs once the client has been quiet, so a burst of edits costs one
run rather than one per keystroke. The debounce follows how slow the compiler
actually is, between 120ms and 600ms.

A request does not wait for the debounce: it starts the check itself and waits
up to 250ms for it. On a small file that lands in time and the answer is
current; on a large one the budget runs out and the request is answered from
the previous analysis rather than stalling the editor.

Measured on this repository:

| Situation                                   | Result                        |
| ------------------------------------------- | ----------------------------- |
| 40 edits sent back to back                  | one compiler run, accepted in 3ms |
| hover during a burst of edits               | 12ms, with up-to-date types   |
| hover during a 15s check on a 10k-line file | under 300ms, from the previous analysis |
| opening a second file during that check     | immediate                     |

## Requirements

| Tool    | Why                          | Verified with |
| ------- | ---------------------------- | ------------- |
| Erlang  | runs the server              | OTP 28        |
| Gleam   | builds the server            | 1.18.0        |
| Neovim  | the client these docs cover  | 0.11.1        |
| `ether` | does all the actual analysis | this checkout |

Neovim **0.11 or newer** is required: the config uses the native
`vim.lsp.config` / `vim.lsp.enable` API that landed in that release.

## Setup

### 1. Build the compiler

From the repository root:

```sh
./build.bat                                           # Windows
cmake -S . -B build -G Ninja && cmake --build build   # POSIX
```

This produces `bin/ether.exe` (or `bin/ether`). The launcher scripts default to
this binary, so nothing needs to be on your `PATH`.

### 2. Build the language server

```sh
lsp\build.cmd        # Windows
./lsp/build.sh       # POSIX
```

This runs `gleam export erlang-shipment` and leaves a self-contained build in
`lsp/ether_lsp/build/erlang-shipment/`. That directory is gitignored, so this
step is needed on every fresh clone, and again after changing any `.gleam` file.

### 3. Point Neovim at the plugin directory

Add one line to your `init.lua`:

```lua
vim.opt.runtimepath:append("/path/to/benzene_lang/lsp/editors/nvim")
```

That is the whole installation. The directory ships the filetype detection, the
fallback syntax file, the LSP client config and the autocommand that enables it.

With `lazy.nvim`, point it at the same directory instead:

```lua
{ dir = "/path/to/benzene_lang/lsp/editors/nvim", lazy = false }
```

`lazy = false` matters: the rule that *defines* the `benzene` filetype ships
inside the plugin, so lazy-loading on `ft = "benzene"` would never fire.

### 4. Open a file

```sh
nvim tests/integration/samples/valid_program.bz
```

Highlighting, diagnostics and inferred-type hints should all appear.
`:BenzeneLspStatus` reports whether the server actually attached.

## What you get

| Feature               | Notes                                                            |
| --------------------- | ---------------------------------------------------------------- |
| Diagnostics           | every compiler phase: lexer, parser, resolver, type checker       |
| Completion            | names in scope, plus keywords; only types after `:` or `:>`        |
| Hover                 | the full signature, e.g. `identity(x: Int) :> Int`                 |
| Go to definition      | `gd` — resolver-backed, so it follows real bindings                |
| References            | `gr` — every occurrence of the same binding                        |
| Document highlight    | the other uses of whatever the cursor rests on                     |
| Rename                | `grn` — rewrites every occurrence; refuses illegal names           |
| Inlay hints           | the inferred type, shown only where you did not write one          |
| Code actions          | write the inferred type down — the one refactor the compiler can offer |
| Signature help        | the signature of the call you are inside, with the active argument |
| Document symbols      | nested: locals sit under the function that declares them           |
| Folding               | `func`/`case`/`{}` paired from the token stream, not indentation   |
| Semantic highlighting | identifiers coloured by what the compiler resolved them to         |
| Closing blocks        | `func`, `case` and `Fn` get their `end` as you open them           |

`refactor.rewrite` is the only code-action kind, so `vim.lsp.buf.code_action()`
on an unannotated binding offers to write its type down.

Completion works through whatever completion plugin you already use
(`nvim-cmp`, `blink.cmp`, or Neovim's built-in `vim.lsp.completion`).

Everything the compiler resolves, the editor resolves. Where a node's own type
was left open by unification, the index falls back to the checker itself: the
type environment, then the constructor table, then the alias table. So `Wrap` in
`Wrap(1)` reads as `Box`, and `type Count = Int` reads as `Int`. Imports are the
one thing deliberately left untyped, because the checker does not type them
either.

Opening a block writes its `end`. Pressing Enter after `func f()`, `case x:` or
a `Fn(...)` lambda adds the closing `end` at the opener's indent and leaves the
cursor inside. It does nothing when the block is already closed, on a `Cmt`
line, or on a `type` expression that merely mentions `Fn`. Turn it off with
`vim.g.benzene_auto_end = false`.

This one is in the plugin rather than the server: the line it has to judge is
the one just typed, which is by definition newer than anything the compiler has
seen. It also avoids mapping `<CR>`, since a buffer-local mapping would shadow
the one a completion plugin uses to accept a completion.

Highlighting is layered. `syntax/benzene.vim` colours the buffer the instant it
opens, using ordinary pattern rules. Once the server attaches, its semantic
tokens take over for anything it knows better — a call to a function and a local
binding are the same shape to a regex, but not to the resolver.

## Commands

| Command              | Does                                                     |
| -------------------- | -------------------------------------------------------- |
| `:BenzeneLspStatus`  | whether the server attached, and how it was launched      |
| `:BenzeneInlayHints` | toggle inferred-type hints in this buffer                 |
| `:BenzeneRestart`    | restart the server, to pick up a rebuilt one              |

## Configuration

All optional; the defaults work for a normal checkout. Set these before the
plugin loads.

```lua
vim.g.benzene_lsp_cmd     = { "/path/to/ether-lsp" }  -- different launcher
vim.g.benzene_compiler    = "/path/to/ether"          -- different compiler
vim.g.benzene_inlay_hints = false                     -- no inline types
vim.g.benzene_folding     = false                     -- leave 'foldexpr' alone
vim.g.benzene_highlight   = false                     -- no cursor-hold highlight
```

The server also reads `ETHER_BIN` from the environment, and falls back to
finding `ether` on `PATH`.

## Troubleshooting

**Nothing highlights and no errors appear.** Run `:BenzeneLspStatus`. If no
client is attached, check `:LspLog` — the server logs the compiler path it chose
to stderr on startup, and Neovim collects that.

**`ether-lsp: not built yet`.** Step 2 has not been run, or was run before the
last `.gleam` change.

**`could not run the ether compiler`,** shown as a diagnostic on line 1. The
server started but could not execute `ether`. Check that step 1 produced a
binary, or set `vim.g.benzene_compiler`. Once you have fixed it, `:w` retries —
a failed check is not repeated until the file changes or you save.

**Diagnostics lag on a big file.** The compiler, not the server, is the cost:
`ether scan` takes about 0.7s on a 2000-line file and 15s on a 10000-line one.
Checks run in the background, so the editor stays responsive and hover keeps
answering from the last completed analysis — but the red underlines will trail
the cursor. `:LspLog` records any check over 250ms.

**Highlighting is right but hover is not.** Hover reads the last successful
analysis. If the file does not parse, that is whatever the parser recovered.

## Testing

```sh
cd lsp/ether_lsp && gleam test   # pure logic, no server or compiler needed
python lsp/test/run.py           # the real server, over a real pipe
python lsp/test/run.py features  # or one suite: features, scheduling, resilience
```

Both builds must be current first (`build.bat`, then `lspuild.cmd`).

The Neovim half runs separately, since it needs an editor:

```sh
nvim --headless --cmd "set rtp+=$PWD/lsp/editors/nvim"      -c "edit lsp/test/sample.bz" -c "luafile lsp/test/nvim_check.lua"
```

`gleam test` covers the features as pure functions. The Python suites cover
what only appears once a real server is talking to a real compiler over a real
pipe: framing, scheduling, subprocess failure, and timing. The Neovim suite
covers the half that only exists inside the editor.

## Status

Verified by those tests:

- 162 compiler tests, 71 Gleam unit tests
- every feature above, driven over the wire by a scripted LSP client, including
  that the annotate action is withheld where the inferred type has no spelling
  the grammar accepts
- Neovim 0.11.1: attach, `utf-8` encoding negotiation, inlay hints on attach,
  foldexpr wiring, and each feature through `vim.lsp`
- block closing driven by real keystrokes, and comment highlighting checked
  against the actual syntax groups
- diagnostics on open and on edit **without saving**
- semantic tokens decoded back onto the source and checked span by span
- debouncing, coalescing, request freshness during a burst, and idle silence
- a 10000-line file whose check takes 15s: requests stay under 300ms
  throughout, a second file still opens, and the check lands and publishes
- fault injection: a compiler that exits non-zero, one that prints garbage, a
  malformed request, an unknown method, requests against unopened or closed
  documents, and closing a file while its check is still running — in every case
  the server replies and stays up
- degenerate inputs: empty, whitespace-only, CRLF, unterminated strings and
  blocks, stray delimiters, 2000-term lines, 50-deep nesting, and multi-byte
  text
- applying the annotate code action and confirming the result still type checks
  and that its inlay hint then disappears

Not done, and worth knowing before relying on this:

- **No cross-file anything.** `Load` imports are not followed; every file is
  analysed alone. Go-to-definition and rename cannot leave the current buffer,
  and there is no `workspace/symbol`.
- **Only one code action.** "Annotate with the inferred type" is the one refactor
  the compiler can supply on its own. It reports problems but suggests no fixes,
  so there are no quick fixes to offer.
- **No formatter.** The compiler has none.
- **Diagnostic ranges are one token wide.** The compiler reports a point, not a
  span. The point is now the right token -- a bad call underlines the callee --
  but a range covering a whole expression would need an end position on
  `SourceLocation`, which the editor would pick up for free.
- **Completion scope is a heuristic.** The index records points, not body
  extents, so "which function am I in" is derived from the nearest declaration
  above the cursor. It is right wherever anything is declared.
- **`utf-16` positions drift on non-ASCII lines.** The lexer counts bytes. The
  server negotiates `utf-8` when the client offers it — Neovim does — but under
  a client that insists on `utf-16`, columns after a multi-byte character will
  be wrong.
- **Comments produce no semantic tokens.** The lexer discards them, so comment
  highlighting comes from the syntax file alone.
- **A whole-file re-check per edit.** There is no incremental analysis; the
  debounce and the background worker are what keep that affordable. The
  compiler also scales badly — roughly quadratically — so very large files lag
  noticeably even though the editor itself stays responsive.

## Known compiler issues

Found while testing this, and left alone because they are the front-end's to
fix, not the editor's. The server reports what it is told.

- **Type aliases are never expanded when unifying.** A `type X = T` declaration
  creates the name, but the unifier compares `X` as an opaque constructor and
  refuses to match it against `T`:

  ```
  type Count = Int

  func f()
    let x: Count = 1   -- "These types are incompatible: expected Count,
    x                  --  but found Int"
  end
  ```

  Dropping the alias makes it compile. Sum types are unaffected -- they are
  nominal, so comparing them by name is right. `TypeAliasTable` now records the
  targets, and the index reads it so hovering `Count` says `Int`; what remains
  is for the unifier to consult it before comparing two types. This is what
  makes an aliased function type (`type Handler = Fn(Int) :> String`) unusable
  as an annotation.

- **A scoped expression containing a `let` does not propagate its type.**

  ```
  func f()
    { let b = 2
      b }
  end          -- infers Fn() :> 't1, should be Fn() :> Int
  ```

  `ether check -show-constraints` shows the block's constraint coming out as
  `'t1 ~ 't1`: the trailing identifier carries the scope's own type variable
  rather than the binding's. Without the `let` it is `'t0 ~ Int` and resolves.
  Let generalisation itself is fine -- a `let`-bound function is correctly used
  at two different types.

## Layout

```
lsp/
  bin/            launcher scripts (what your editor runs)
  build.cmd       builds the shipment on Windows
  build.sh        builds the shipment on POSIX
  editors/nvim/   the Neovim plugin: ftdetect, syntax, ftplugin, lsp config
  ether_lsp/      the Gleam project
    src/
      ether_lsp.gleam      entry point and the message loop
      ether_lsp_ffi.erl    stdio, the reader process, the scan worker
      lsp/rpc.gleam        Content-Length framing, reader plumbing
      lsp/scan.gleam       starts `ether scan`, decodes its JSON
      lsp/server.gleam     state, scheduling, dispatch
      lsp/feature.gleam    the features, as pure functions over one analysis
      lsp/encode.gleam     shared JSON shapes and position conversion
      lsp/semantic.gleam   tokens to the semantic-token legend
      lsp/text.gleam       reading the buffer where the cursor is ahead of it
      lsp/uri.gleam        file:// URIs to paths
```

`feature.gleam` holds no state and does no IO: each function takes one analysis
and returns the JSON to reply with, which is why the unit tests need neither a
client nor a compiler.

## Developing

```sh
cd lsp/ether_lsp
gleam test                   # unit tests for the pure logic
gleam format src test
cd ../.. && lsp\build.cmd    # re-export the shipment your editor runs
```

Then `:BenzeneRestart` in Neovim to pick it up.

`gleam run` is not useful on its own — the server expects an LSP client on
stdin and will sit waiting for a `Content-Length` header.
