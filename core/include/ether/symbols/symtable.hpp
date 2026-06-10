#pragma once
#include <optional>
#include <ether/tokens/token_types.hpp>
#include <ether/symbols/scopes.hpp>
#include <ether/symbols/symbol_types.hpp>
#include <string>
#include <vector>

class SymbolTable {
public:
  explicit SymbolTable(SymbolStorage& arena) : arena(arena) {
    new_scope(ScopeType::Module);
  }

  [[nodiscard]]
  auto declare(const Token&, SymbolKind) -> SymbolAttr*;

  [[nodiscard]]
  auto lookup(const std::string&) -> SymbolAttr*;

  [[nodiscard]] auto get_current_scope_type() const -> std::optional<ScopeType> ;
  void new_scope(ScopeType);
  void pop_scope();

private:
  SymbolStorage& arena;
  std::vector<Scope> scopes;
};

struct ScopeGuard {
  SymbolTable& table;
  ScopeGuard(SymbolTable& tbl, ScopeType stype= ScopeType::Module): table(tbl) {
    table.new_scope(stype);
  }

  ~ScopeGuard() {
    table.pop_scope();
  }
};
