#pragma once
#include "cmd.hpp"

// `ether scan` — machine-readable front-end output for editor tooling.
//
// Runs the same pipeline as `check` (lex, parse, resolve scopes, resolve
// symbols, type check, unify) and serialises the result as a single JSON
// object on stdout: tokens for semantic highlighting, diagnostics, and an
// index of identifier occurrences with their solved types.
//
// This exists so the language server never has to reimplement the front-end.
int HandleScan(const ArgScan&);
