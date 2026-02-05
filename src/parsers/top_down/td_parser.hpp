#pragma once
#include <expected>
#include <optional>
#include <span>
#include <string>
#include "../../nodes/node.hpp"
#include "../../tokens/token_types.hpp"
#include "../../symbols/symtable.hpp"

struct ParseError {
  Token error_token{};
  std::string message{};
};

using Result = std::expected<ASTPtr, ParseError>;
using MResult = std::expected<Token, ParseError>;

template<typename T>
using Option = std::optional<T>;

class TDParser {
public:
  TDParser(std::span<Token> tokens, SymbolTable sym_table) {
    this->sym_table = sym_table;
  }

private:
  Result parse_additive_expr();

  Result parse_multiplicative_expr();

  Result parse_unary_expr();

  Result parse_statement_expr();

  Result parse_func_decl_expr();

  Result parse_const_expr();

  Result parse_let_bind_expr();

  Result parse_literal_expr();

  Result parse_identifier_expr();

  Result parse_general_expr();

  MResult m_match_token_type(TokenType);

  Option<Token> consume();

  Option<Token> peek();

  bool at_token_end();

  std::unexpected<ParseError> make_err(Token, std::string);

  std::unexpected<ParseError> default_eof_error();

  void reset_position(size_t);

  std::span<Token> tokens{};

  size_t current_position{};

  SymbolTable sym_table{};
};
