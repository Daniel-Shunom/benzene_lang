#pragma once
#include "../lexer/token_types.hpp"
#include "scopes.hpp"
#include "symbol_types.hpp"
#include <optional>
#include <span>
#include <string>
#include <vector>

class SymbolTable {
public:
  SymbolTable(std::span<Token> tokens) {
    this->tokens = tokens;
  }

  void scan_symbols();

private:
  using t = TokenType;
  std::optional<Token> peek();

  std::optional<Token> next();

  void advance();

  void scan_func_decl();

  void scan_func_body();

  void scan_case_expr();
 
  void scan_const_expr();

  void scan_let_bind();
 
  void scan_import();

  bool is_tokens_end();

  void make_symbol(const std::string&, SymbolInfo);

  std::vector<Scope> scope_stack;

  std::span<Token> tokens{};

  size_t position{};
};
