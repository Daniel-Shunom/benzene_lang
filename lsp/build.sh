#!/usr/bin/env sh
# Compiles the language server into lsp/ether_lsp/build/erlang-shipment.
set -eu
cd "$(dirname -- "$0")/ether_lsp"
exec gleam export erlang-shipment
