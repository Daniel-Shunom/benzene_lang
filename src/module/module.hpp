#pragma once
#include "../diagnostics/diagnostic_eng.hpp"
#include "../nodes/node_expr.hpp"
#include "../symbols/symbol_types.hpp"
#include <string>
#include <unordered_map>

// Each module contains some code. And each module will have
// its own AST, know imports, and other module specific stuff
//
// It works under the assumption that a Module takes in the
// correct and validated path
//
// Each module object contains functions for lexing, parsing,
// and generating the file, into its own AST
class Module {
public:
  explicit Module(std::string module_path)
  : module_path(module_path),
    diag(DiagnosticEngine())
  { };

  void attach_visitor(Visitor&);
  void generate_ast();
  void apply_visitors();
  void print_errors();
  Parent get_ast();

  SymbolStorage& get_symbol_storage() { return arena; }
  DiagnosticEngine& get_diag_engine() { return diag; }

  // Module-level synthesized attribute: the symbols this module makes
  // available to other modules (top-level constants and top-level functions).
  // Keyed by identifier name so cross-module imports can look up by name in
  // O(1). Populated by an external pass (the symbol resolver) after analysis.
  const std::unordered_map<std::string, SymbolAttr*>& get_exported_symbols() const {
    return exported_symbols;
  }
  void set_exports(std::unordered_map<std::string, SymbolAttr*> syms) {
    exported_symbols = std::move(syms);
  }

private:
  std::string module_path;
  DiagnosticEngine diag;

  SymbolStorage arena;
  std::unordered_map<std::string, SymbolAttr*> exported_symbols;

  std::string extract_contents_from_path();
  Parent module_root;

  void make_module_ast();
};
