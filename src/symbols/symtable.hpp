#pragma once
#include <optional>
#include "../tokens/token_types.hpp"
#include "scopes.hpp"
#include "symbol_types.hpp"
#include <string>
#include <vector>

class SymbolTable {
public:
  explicit SymbolTable(SymbolStorage& arena) : arena(arena) {
    new_scope(ScopeType::Module);
  }
  SymbolAttr* declare(const Token&, SymbolKind);
  SymbolAttr* lookup(const std::string&);
  std::optional<ScopeType> get_current_scope_type() const ;
  void new_scope(ScopeType);
  void pop_scope();

private:
  SymbolStorage& arena;
  std::vector<Scope> scopes;
};

struct ScopeGuard {
  SymbolTable& table;
  ScopeGuard(SymbolTable& t, ScopeType s = ScopeType::Module): table(t) {
    table.new_scope(s);
  }

  ~ScopeGuard() {
    table.pop_scope();
  }
};
