#include <format>
#include <memory>
#include <optional>
#include "td_parser.hpp"
#include "../../tables/utils.hpp"

Result TDParser::parse_let_bind_expr() {
  size_t start = this->current_position;
  MResult let_token = this->m_match_token_type(TokenType::LetKeyword);
  
  if (!let_token) {
    this->reset_position(start);
    return std::unexpected(let_token.error());
  }

  Result identifier = this->parse_identifier_expr();
  if (!identifier) {
    this->reset_position(start);
    return std::unexpected(identifier.error());
  }

  Result bound_expr = this->parse_general_expr();
  if (!bound_expr) {
    this->reset_position(start);
    return std::unexpected(bound_expr.error());
  }
  
  auto* ident = static_cast<IdentifierExpr*>(identifier.value().get());
  LetBindExpr expr = {
    .symbol = ident->identifier_sym,
    .identifier = std::move(identifier.value()),
    .expr = std::move(bound_expr.value())
  };

  return std::make_unique<ASTNode>(expr);
}

Result TDParser::parse_identifier_expr() {
  size_t start = this->current_position;
  auto token = this->consume();
  if (!token) {
    this->reset_position(start);
    return this->default_eof_error();
  }

  if (token->token_type != TokenType::Identifier) {
    std::string gotten = typeToStr(token->token_type);
    std::string message = std::format("Expected Identifier token but instead got {}", gotten);
    this->reset_position(start);
    return this->make_err(token.value(), message);
  }

  Symbol* sym = this->sym_table.lookup(token->token_value);
  if (sym == nullptr) {
    Symbol* inserted_sym = sym_table.declare(token.value(), SymbolKind::UnResolved);
    IdentifierExpr expr = {.identifier_sym = inserted_sym};
    return ASTPtr(std::make_unique<ASTNode>(expr));
  }

  IdentifierExpr expr = {.identifier_sym=sym};
  return std::make_unique<ASTNode>(expr);
}

Result TDParser::parse_literal_expr() {
  size_t start = this->current_position;
  auto token = this->consume();

  if (!token) {
    this->reset_position(start);
    return this->default_eof_error();
  }

  if (!is_literal(token.value())) {
    std::string gotten = typeToStr(token->token_type);
    std::string message = std::format("Error: expected `Literal` token but instead got {}", gotten);
    this->reset_position(start);
    return this->make_err(token.value(), message);
  }

  Symbol* sym = this->sym_table.lookup(token->token_value);
  if (sym == nullptr) {
    Symbol* inserted_sym = sym_table.declare(token.value(), SymbolKind::UnResolved);
    IdentifierExpr expr = {
      .identifier_sym = inserted_sym
    };
    return std::make_unique<ASTNode>(expr);
  }

  IdentifierExpr expr = {.identifier_sym=sym};
  return std::make_unique<ASTNode>(expr);
}

MResult TDParser::m_match_token_type(TokenType type) {
  size_t start = this->current_position;
  auto token = this->consume();
  if (token && token->token_type == type) {
    return token.value();
  }

  this->reset_position(start);
  std::string expected(typeToStr(type));
  std::string gotten(typeToStr(token->token_type));
  std::string message = std::format(
    "Expected `{}` token, but instead got `{}`.", 
    expected, gotten
  );
  ParseError error = {
    .error_token = token.value(),
    .message = message
  };

  return std::unexpected(error);
}

Option<Token> TDParser::peek() {
  if (this->at_token_end()) return std::nullopt;
  return this->tokens[this->current_position];
}

Option<Token> TDParser::consume() {
  if (this->at_token_end()) return std::nullopt;
  return this->tokens[this->current_position++];
}

void TDParser::reset_position(size_t pos) {
  if (pos < this->tokens.size()) {
    this->current_position = pos;
  }
}

bool TDParser::at_token_end() {
  return this->current_position >= this->tokens.size();
}

std::unexpected<ParseError> TDParser::make_err(Token tok, std::string message) {
  auto error = ParseError{
    .error_token = tok,
    .message = message
  };

  return std::unexpected(error);
}

std::unexpected<ParseError> TDParser::default_eof_error() {
  auto error = ParseError{
    .error_token = Token{},
    .message = "Error: reached end of token stream"
  };

  return std::unexpected(error);
}
