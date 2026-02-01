#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "token_types.hpp"

class Tokenizer {
public:
  Tokenizer(std::string input) {
    this->input = input;
  };

  void scan_tokens();

  void scan_string();

  void scan_number();

  bool scan_operator();

  void scan_keyword_or_identifier();

  std::vector<Token> get_tokens();

  void print_tokens();

private: 
  uint64_t position{0};

  bool is_file_end();

  bool is_identifier_char(char);

  bool is_string_apo(char);

  bool is_digit(char);

  bool match(std::string& expected);

  char peek();

  char advance();

  void make_token(TokenType, std::string);

  std::string input{};

  std::vector<Token> tokens{};
};
