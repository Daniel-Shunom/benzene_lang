#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "token_types.hpp"

class Tokenizer {
public:
  Tokenizer(std::string_view input) {
    this->input = input;
    this->line_number = 1;
    this->column_number = 1;
    this->token_start_line = 1;
  };

  void scan_tokens();

  void print_tokens();

  std::vector<Token> get_tokens();

private: 
  size_t position{};

  size_t line_number{};

  size_t column_number{};

  size_t token_start_line{};

  size_t token_start_column{};

  void scan_string();

  void scan_number();

  bool scan_operator();

  bool scan_other_symbol();

  void scan_keyword_or_identifier();

  void scan_comment();

  void scan_import_module();

  void scan_multi_line_comment();

  bool is_newline(const char&);

  bool is_file_end();

  bool is_whitespace_or_newline(const char&);

  bool is_whitespace(const char&);

  bool is_identifier_char(const char&);

  bool is_string_apo(const char&);

  bool is_dot(const char&);

  bool is_digit(const char&);

  bool is_delim(const char&);

  bool match(const std::string& expected);

  char peek();

  char advance();

  size_t get_line_number();

  size_t get_column_number();

  void set_token_start();

  void make_token(TokenType, std::string);

  std::string_view input{};

  std::vector<Token> tokens{};
};
