#pragma once
#include "../tokens/token_types.hpp"
#include "scopes.hpp"
#include "symbol_types.hpp"
#include <string>
#include <vector>

class SymbolTable {
public:
  Symbol* declare(const Token&, SymbolKind);
  Symbol* lookup(const std::string&);

  void new_scope(ScopeType);
  void pop_scope();

private:
  std::vector<Scope> scopes;
};
