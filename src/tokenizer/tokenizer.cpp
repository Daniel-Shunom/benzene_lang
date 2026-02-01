#include "tokenizer.hpp"
#include "token_types.hpp"
#include <cctype>
#include <format>
#include <iostream>
#include <string>
#include "tables/keyword_table.hpp"
#include "tables/operator_table.hpp"


void Tokenizer::scan_tokens() {
  while(!this->is_file_end()) {
    char c = this->peek();

    if (this->is_identifier_char(c)) {
      this->scan_keyword_or_identifier();
      continue;
    }

    if (this->is_string_apo(c)) {
      this->scan_string();
      continue;
    }

    if (this->is_digit(c)) {
      this->scan_number();
      continue;
    }

    if (this->scan_operator()) {
      continue;
    }

    switch (c) {
      case '(': this->advance(); this->make_token(TokenType::LParen, "("); continue;
      case ')': this->advance(); this->make_token(TokenType::RParen, ")"); continue;
      case '{': this->advance(); this->make_token(TokenType::LBrace, "{"); continue;
      case '}': this->advance(); this->make_token(TokenType::RBrace, "}"); continue;
      case '[': this->advance(); this->make_token(TokenType::LBrac, "["); continue;
      case ']': this->advance(); this->make_token(TokenType::RBrac, "]"); continue;
      default: this->advance(); this->make_token(TokenType::Unknown, ""); continue;
    }
  }

  this->tokens.emplace_back(Token({
    .token_type = TokenType::EoF,
    .token_value = std::string{}
  }));
}

void Tokenizer::scan_keyword_or_identifier() {
  std::string id{};

  while(
    !this->is_file_end() 
    && this->is_identifier_char(this->peek())
  ) {
    id.push_back(this->peek());
    this->advance();
  }

  auto it = KeywordTable.find(id);

  if (it == KeywordTable.end()) {
    this->tokens.emplace_back(Token({
      .token_type = TokenType::Identifier,
      .token_value = id
    }));
    return;
  }

  this->tokens.emplace_back(Token({
    .token_type = it->second,
    .token_value = id
  }));
}

void Tokenizer::scan_number() {
  std::string number("");

  while (!this->is_file_end() && std::isdigit(this->peek())) {
    number.push_back(this->peek());
    this->advance();
  }

  bool is_decimal = false;

  if (!this->is_file_end() && this->peek() == '.') {
    is_decimal = true;
    number.push_back(this->peek());
    this->advance();

    while (!this->is_file_end() && std::isdigit(this->peek())) {
      number.push_back(this->peek());
      this->advance();
    }
  }

  if (is_decimal) {
    this->tokens.emplace_back(Token({
      .token_type = TokenType::Float,
      .token_value = number
    }));
  } else {
    this->tokens.emplace_back(Token({
      .token_type = TokenType::Integer,
      .token_value = number
    }));
  }
}

bool Tokenizer::scan_operator() {
  for (const auto& [op, type]: OperatorList) {
    if (this->input.compare(this->position, op.size(), op) == 0) {
      this->position += op.size();
      this->make_token(type, op);
      return true;
    }
  }

  return false;
}

void Tokenizer::scan_string() {
  if (!this->is_string_apo(this->peek())) {
    return;
  }

  std::string value{};
  this->advance();

  while (
    !this->is_file_end()
    && !this->is_string_apo(this->peek())
  ) {
    if (this->peek() == '\\') {
      this->advance();
      if (!this->is_file_end()) {
        switch (this->peek()) {
          case '\\': value.push_back('\\'); this->advance(); break;
          case '\"': value.push_back('"'); this->advance(); break;
          case 'n' : value.push_back('\n'); this->advance(); break;
          case 't' : value.push_back('\t'); this->advance(); break;
          case 'r' : value.push_back('\r'); this->advance(); break;
          case '0' : value.push_back('\0'); this->advance(); break;
          default: value.push_back(this->peek()); this->advance(); break;
        }
      }
    } else {
      value.push_back(this->peek());
      this->advance();
    }
  }

  this->advance();
  this->tokens.emplace_back(Token({
    .token_type = TokenType::String,
    .token_value = value
  }));
}


bool Tokenizer::match(std::string& expected) {
  for (char c: expected) {
    if (this->peek() != c) return false;
    this->advance();
  }

  return true;
}

char Tokenizer::peek() {
  if (this->position >= this->input.size()) {
    return '\0';
  }

  return this->input[this->position];
}

char Tokenizer::advance() {
  if (this->position >= this->input.size()) {
    return '\0';
  }

  return this->input[this->position++];
}

bool Tokenizer::is_file_end() {
  return this->position >= this->input.size();
}

bool Tokenizer::is_identifier_char(char c) {
  return std::isalnum(c) || (c == '_');
}

bool Tokenizer::is_string_apo(char c) {
  return c == '\"';
}

bool Tokenizer::is_digit(char c) {
  return std::isdigit(c);
}

void Tokenizer::make_token(TokenType type, std::string value) {
  this->tokens.emplace_back(Token({
    .token_type = type,
    .token_value = value
  }));
}

std::vector<Token> Tokenizer::get_tokens() {
  return this->tokens;
}

void Tokenizer::print_tokens() {
  for (const auto& token: this->tokens) {
    std::cout << std::format("[TOKEN]\t Type: {}, value: {}", typeToStr(token.token_type), token.token_value);
  }
}
