#include "symtable.hpp"
#include "scopes.hpp"
#include "symbol_types.hpp"
#include <optional>

void SymbolTable::scan_symbols() {
  using t = TokenType;
  this->scope_stack.emplace_back(Scope());

  while (auto tok = this->peek()) {
    if (!tok.has_value()) {
      break;
    }

    switch (tok->token_type) {
      case t::ImportKeyword: {}

      case t::ConstantKeyword: {
        this->scan_const_expr(); 
        this->advance();
        continue;
      }

      case t::FuncStart: {
        this->scan_func_decl();
        this->advance();
        continue;
      };

      case t::LetKeyword: {
        this->scan_let_bind(); 
        this->advance();
        continue;
      }

      case t::Identifier: {
        // At this level, we should ignore not be expecting identifiers,
        // so we are definitely emitting scope errors.
      }

      default: {}
    }
  }
}

void SymbolTable::scan_func_decl() {
  this->scope_stack.emplace_back(Scope({
    .scope_type = ScopeType::Function,
    .scope_sym_table = SymTable{}
  }));
  this->advance();

  auto tok = this->next();
  if (tok->token_type != t::Identifier) {
    // emit error here.
    return;
  }

  FuncDeclInfo fn = {
    .params = {},
    .rtn_type_info = TypeInfo()
  };

  // scan body with helper function.

  this->scope_stack.pop_back();
}

void SymbolTable::scan_case_expr() {
  this->scope_stack.emplace_back(Scope());
  this->scope_stack.pop_back();
}

std::optional<Token> SymbolTable::next() {
  if (this->position+1 <= this->tokens.size()) {
    return this->tokens[this->position+1];
  }
  return std::nullopt;
}

std::optional<Token> SymbolTable::peek() {
  if (this->is_tokens_end()) {
    return std::nullopt;
  };

  return this->tokens[this->position];
}

bool SymbolTable::is_tokens_end() {
  return this->position >= this->tokens.size()
    || this->tokens[position].token_type == TokenType::EoF;
}
