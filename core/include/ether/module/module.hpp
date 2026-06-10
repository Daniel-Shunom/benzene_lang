#pragma once
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/symbols/symbol_types.hpp>
#include <iosfwd>
#include <iostream>
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
  void print_errors(std::ostream& out = std::cout);
  auto get_ast() -> Parent;

  auto get_symbol_storage() -> SymbolStorage& { return arena; }
  auto get_diag_engine() -> DiagnosticEngine& { return diag; }
  [[nodiscard]] auto get_path() const -> const std::string& { return module_path; }

  [[nodiscard]] auto get_exported_symbols() const -> const std::unordered_map<std::string, SymbolAttr*>& {
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
