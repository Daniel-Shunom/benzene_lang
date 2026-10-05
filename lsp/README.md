# ether-lsp — a language server for Benzene

> [!WARNING]
> **Highly experimental, and mostly AI-generated.**
>
> This whole directory (plus the `ether scan` subcommand that backs it) was
> written in a single automated session and has not been reviewed line by line.
> It works on the samples in this repository and under a real Neovim client —
> see [Status](#status) for exactly what was verified — but treat it as a
> starting point to read and rewrite, not as trusted code. Expect rough edges
> on anything outside the happy path.

A language server for Benzene, written in Gleam. It gives you live type
checking, hover types, go-to-definition, a document outline, and semantic
syntax highlighting in any LSP-capable editor.

## How it works

The server owns no lexer, parser or type checker of its own. Every answer comes
from the real compiler:

```
  Neovim  ──LSP/stdio──▶  ether-lsp (Gleam/BEAM)  ──stdin──▶  ether scan (C++)
          ◀─diagnostics─                          ◀──JSON───
```

`ether scan` is a subcommand added for this purpose. It runs the same pipeline
as `ether check` — lex, parse, resolve scopes, resolve symbols, type check,
unify — and prints the result as JSON: tokens for highlighting, diagnostics,
and an index of every identifier with its solved type and declaration site.

The buffer is passed to the compiler on **stdin**, length-prefixed, so what gets
checked is what you are looking at. You do not have to save the file.

The point of the split is that there is exactly one type checker. Reimplementing
inference in Gleam would mean the editor and the compiler could disagree about
your program, which is worse than having no editor support at all.

## Requirements

| Tool    | Why                                  | Verified with  |
| ------- | ------------------------------------ | -------------- |
| Erlang  | runs the server                      | OTP 28         |
| Gleam   | builds the server                    | 1.18.0         |
| Neovim  | the client these docs cover          | 0.11.1         |
| `ether` | does all the actual analysis         | this checkout  |

Neovim **0.11 or newer** is required: the config uses the native `vim.lsp.config`
/ `vim.lsp.enable` API that landed in that release.

## Setup

### 1. Build the compiler

From the repository root:

```sh
./build.bat          # Windows
cmake -S . -B build -G Ninja && cmake --build build    # POSIX
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
{ dir = "/path/to/benzene_lang/lsp/editors/nvim", ft = "benzene" }
```

### 4. Open a file

```sh
nvim tests/integration/samples/valid_program.bz
```

The buffer should highlight immediately, and errors should appear as you type.
`:BenzeneLspStatus` reports whether the server actually attached.

## What you get

| Feature                    | Notes                                                       |
| -------------------------- | ----------------------------------------------------------- |
| Diagnostics                | every compiler phase: lexer, parser, resolver, type checker  |
| Hover                      | the inferred type, e.g. `identity : Fn(Int) :> Int`          |
| Go to definition           | `gd` — resolver-backed, so it follows real bindings          |
| Document symbols           | functions, constants, bindings and types                     |
| Semantic highlighting      | identifiers coloured by what the compiler resolved them to   |

Highlighting is layered. `syntax/benzene.vim` colours the buffer the instant it
opens, using ordinary pattern rules. Once the server attaches, its semantic
tokens take over for anything it knows better — a call to a function and a local
binding are the same shape to a regex, but not to the resolver.

## Configuration

Both are optional; the defaults work for a normal checkout.

```lua
-- Use a different server launcher.
vim.g.benzene_lsp_cmd = { "/path/to/ether-lsp" }

-- Use a different compiler binary.
vim.g.benzene_compiler = "/path/to/ether"
```

The server also reads `ETHER_BIN` from the environment, and falls back to
finding `ether` on `PATH`.

## Troubleshooting

**Nothing highlights and no errors appear.** Run `:BenzeneLspStatus`. If no
client is attached, check `:LspLog` — the server logs the compiler path it
chose to stderr on startup, and Neovim collects that.

**`ether-lsp: not built yet`.** Step 2 has not been run, or was run before the
last `.gleam` change.

**`could not run the ether compiler`,** shown as a diagnostic on line 1. The
server started but could not execute `ether`. Check that step 1 produced a
binary, or set `vim.g.benzene_compiler`.

**Highlighting is right but hover and go-to-definition are not.** These read the
index from the last successful scan. If the file does not parse, the index is
whatever the parser recovered.

## Status

Verified in this session, against Neovim 0.11.1 driving the real server:

- filetype detection, attach, and `utf-8` position encoding negotiation
- diagnostics on open, and on edit **without saving**
- semantic tokens decoded back onto the source and checked span by span
- hover, go-to-definition and document symbols on the repository's samples
- clean `shutdown`/`exit`

Not done, and worth knowing before relying on this:

- **No completion.** There is no `textDocument/completion` handler at all.
- **No cross-file anything.** `Load` imports are not followed; every file is
  analysed alone. Go-to-definition cannot leave the current buffer.
- **A whole-file re-check on every keystroke.** Each edit spawns a compiler
  process. Fine for the file sizes in this repo, and not debounced.
- **Diagnostic ranges are one token wide.** The compiler reports a point, not a
  span, so the underline covers the token starting there and nothing more.
- **`utf-16` positions drift on non-ASCII lines.** The lexer counts bytes. The
  server negotiates `utf-8` when the client offers it — Neovim does — but under
  a client that insists on `utf-16`, columns after a multi-byte character will
  be wrong.
- **Comments produce no semantic tokens.** The lexer discards them, so comment
  highlighting comes from the syntax file alone.

## Layout

```
lsp/
  bin/            launcher scripts (what your editor runs)
  build.cmd       builds the shipment on Windows
  build.sh        builds the shipment on POSIX
  editors/nvim/   the Neovim plugin: ftdetect, syntax, ftplugin, lsp config
  ether_lsp/      the Gleam project
    src/
      ether_lsp.gleam      entry point and read loop
      ether_lsp_ffi.erl    raw stdio and subprocess control
      lsp/rpc.gleam        Content-Length framing
      lsp/scan.gleam       runs `ether scan`, decodes its JSON
      lsp/semantic.gleam   tokens to the semantic-token legend
      lsp/server.gleam     dispatch and the feature handlers
      lsp/uri.gleam        file:// URIs to paths
```

## Developing

```sh
cd lsp/ether_lsp
gleam test     # unit tests for the pure logic
gleam build
cd ../.. && lsp\build.cmd    # re-export the shipment your editor runs
```

`gleam run` is not useful on its own — the server expects an LSP client on
stdin and will sit waiting for a `Content-Length` header.
