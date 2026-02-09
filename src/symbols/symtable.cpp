#include "symtable.hpp"
#include "scopes.hpp"
#include "symbol_types.hpp"
#include <memory>
#include <optional>

Symbol* SymbolTable::declare(const Token& token, SymbolKind kind) {
  std::string name = token.token_value;
  auto sym = std::make_unique<Symbol>(Symbol{
    .name = token.token_value,
    .symbol_kind = kind,
    .type_info = TypeInfo(),
    .symbol_token = token,
  });

  if (this->scopes.back().scope_sym_table.contains(name)) {
    return nullptr;
  }

  Symbol* ptr = sym.get();
  this->scopes.back().scope_sym_table[name] = std::move(sym);
  return ptr;
}

std::optional<ScopeType> SymbolTable::get_current_scope_type() const {
  if (this->scopes.empty()) return std::nullopt;
  return this->scopes.back().scope_type;
}

Symbol* SymbolTable::lookup(const std::string& name) {
  for (auto it = this->scopes.rbegin(); it != scopes.rend(); ++it) {
    if (it->scope_sym_table.contains(name)) {
      Symbol* ptr = it->scope_sym_table[name].get();
      return ptr;
    }
  }
  return nullptr;
}

void SymbolTable::new_scope(ScopeType scope_type) {
  this->scopes.emplace_back(Scope({
    .scope_type = scope_type,
    .scope_sym_table{}
  }));
}


void SymbolTable::pop_scope() {
  this->scopes.pop_back();
}
