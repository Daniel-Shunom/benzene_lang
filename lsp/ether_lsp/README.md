# ether_lsp

The Gleam half of the Benzene language server. It speaks LSP on stdin/stdout and
delegates all analysis to the `ether` compiler.

See [`../README.md`](../README.md) for what it does, how it is wired together,
and how to install it in an editor. This package is not published to Hex and is
not meant to be depended on.

```sh
gleam test    # unit tests for the pure logic
gleam build
```

Building the artefact your editor actually runs is a level up:

```sh
cd .. && ./build.sh      # or build.cmd on Windows
```
