#pragma once
#include <optional>
#include "../tokens/token_types.hpp"
#include "scopes.hpp"
#include "symbol_types.hpp"
#include <string>
#include <vector>

class SymbolTable {
public:
  SymbolTable() {
    new_scope(ScopeType::Module);
  }
  Symbol* declare(const Token&, SymbolKind);
  Symbol* lookup(const std::string&);
  std::optional<ScopeType> get_current_scope_type() const ;
  void new_scope(ScopeType);
  void pop_scope();

private:
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
