#pragma once
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/symbols/symbol_types.hpp>
#include <string>
#include <unordered_map>

// A Module owns the AST, symbol storage, and diagnostics for one source unit.
// It is filesystem-agnostic: callers (CLI, LSP, tests) read the source bytes
// however they like and hand them in. `path` is the canonical identity used
// for diagnostic rendering and registry keys; it is not opened by Module.
class Module {
public:
  Module(std::string path, std::string source)
  : module_path(std::move(path)),
    source_text(std::move(source)) {}

  void attach_visitor(Visitor&);
  void generate_ast();
  void apply_visitors();
  void print_errors();
  Parent get_ast();

  SymbolStorage& get_symbol_storage() { return arena; }
  DiagnosticEngine& get_diag_engine() { return diag; }
  const std::string& get_path() const { return module_path; }

  const std::unordered_map<std::string, SymbolAttr*>& get_exported_symbols() const {
    return exported_symbols;
  }
  void set_exports(std::unordered_map<std::string, SymbolAttr*> syms) {
    exported_symbols = std::move(syms);
  }

private:
  std::string module_path;
  std::string source_text;
  DiagnosticEngine diag;

  SymbolStorage arena;
  std::unordered_map<std::string, SymbolAttr*> exported_symbols;

  Parent module_root;

  void make_module_ast();
};
