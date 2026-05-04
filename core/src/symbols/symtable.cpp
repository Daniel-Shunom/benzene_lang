#include <ether/symbols/symtable.hpp>
#include <ether/symbols/scopes.hpp>
#include <ether/symbols/symbol_types.hpp>
#include <optional>

SymbolAttr* SymbolTable::declare(const Token& token, SymbolKind kind) {
  const std::string& name = token.token_value;

  if (this->scopes.back().scope_sym_table.contains(name)) {
    return nullptr;
  }

  SymbolAttr* ptr = this->arena.allocate(SymbolAttr{
    .name = name,
    .symbol_kind = kind,
    .type_info = TypeInfo(),
    .symbol_token = token,
  });

  this->scopes.back().scope_sym_table[name] = ptr;
  return ptr;
}

std::optional<ScopeType> SymbolTable::get_current_scope_type() const {
  if (this->scopes.empty()) return std::nullopt;
  return this->scopes.back().scope_type;
}

SymbolAttr* SymbolTable::lookup(const std::string& name) {
  for (auto it = this->scopes.rbegin(); it != scopes.rend(); ++it) {
    if (auto found = it->scope_sym_table.find(name); found != it->scope_sym_table.end()) {
      return found->second;
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
