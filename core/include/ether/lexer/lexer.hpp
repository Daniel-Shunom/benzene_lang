#pragma once
#include <cstddef>
#include <iosfwd>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <ether/tokens/token_types.hpp>
#include <ether/lexer/lexer_diag.hpp>

class Lexer {
public:
  Lexer(std::string_view input, DiagnosticEngine& eng)
  : lex_diag(eng) {
    this->input = input;
    this->line_number = 1;
    this->column_number = 1;
    this->token_start_line = 1;
  };

  void scan_tokens();

  void print_tokens(std::ostream& out = std::cout) const;

  auto get_tokens() -> std::vector<Token>;

private:
  LexerDiagnostics lex_diag;

  size_t position{};

  size_t line_number{};

  size_t column_number{};

  size_t token_start_line{};

  size_t token_start_column{};

  void scan_string();

  void scan_number();

  auto scan_operator() -> bool;

  auto scan_other_symbol() -> bool;

  void scan_keyword_or_identifier();

  void scan_comment();

  void scan_import_module();

  void scan_multi_line_comment();

  auto is_newline(const char&) -> bool;

  auto is_file_end() -> bool;

  auto is_whitespace_or_newline(const char&) -> bool;

  auto is_whitespace(const char&) -> bool;

  auto is_identifier_char(const char&) -> bool;

  auto is_string_apo(const char&) -> bool;

  auto is_dot(const char&) -> bool;

  auto is_digit(const char&) -> bool;

  auto is_delim(const char&) -> bool;

  auto match(const std::string& expected) -> bool;

  auto peek() -> char;

  auto advance() -> char;

  auto get_line_number() -> size_t;

  auto get_column_number() -> size_t;

  void set_token_start();

  auto make_token(TokenType, std::string) -> Token;

  std::string_view input;

  std::vector<Token> tokens;
};
