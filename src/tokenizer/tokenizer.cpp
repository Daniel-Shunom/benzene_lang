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

    if (this->is_whitespace_or_newline(c)) {
      if (c != '\n') {
        this->advance();
        continue;
      }
      this->increment_line_number();
      this->reset_column_number();
      this->advance();
      continue;
    }

    if (std::isalpha(c)) {
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

    if (this->scan_other_symbol()) {
      continue;
    }

    this->make_token(TokenType::Unknown, {c});
    continue;
  }

  this->make_token(TokenType::EoF, "");
}

void Tokenizer::scan_keyword_or_identifier() {
  this->token_start_line = this->get_line_number();
  this->token_start_column = this->get_column_number();

  std::string id{};

  while(
    !this->is_file_end() 
    && this->is_identifier_char(this->peek())
  ) {
    id.push_back(this->peek());
    this->advance();
  }

  if(this->peek() == '.') {
    id.push_back(this->peek());
    this->advance();
  }

  auto it = KeywordTable.find(id);

  if (it == KeywordTable.end()) {
    this->make_token(TokenType::Identifier, id);
  } else {
    this->make_token(it->second, id);
  }
}

void Tokenizer::scan_number() {
  this->token_start_line = this->get_line_number();
  this->token_start_column = this->get_column_number();

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
    this->make_token(TokenType::Float, number);
  } else {
    this->make_token(TokenType::Integer, number);
  }
}

bool Tokenizer::scan_operator() {
  this->token_start_line = this->get_line_number();
  this->token_start_column = this->get_column_number();

  for (const auto& [op, type]: OperatorList) {
    if (this->input.compare(this->position, op.size(), op) == 0) {
      for (const auto& _: op) this->advance();
      this->make_token(type, op);
      return true;
    }
  }

  return false;
}


bool Tokenizer::scan_other_symbol() {
  this->token_start_line = this->get_line_number();
  this->token_start_column = this->get_column_number();

  if (this->is_file_end()) return false;

  char c = this->peek();

  switch (c) {
    case '(':
      this->advance();
      this->make_token(TokenType::LParen, "(");
      return true;

    case ')':
      this->advance();
      this->make_token(TokenType::RParen, ")");
      return true;

    case '{':
      this->advance();
      this->make_token(TokenType::LBrace, "{");
      return true;

    case '}':
      this->advance();
      this->make_token(TokenType::RBrace, "}");
      return true;

    case '[':
      this->advance();
      this->make_token(TokenType::LBrac, "[");
      return true;

    case ']':
      this->advance();
      this->make_token(TokenType::RBrac, "]");
      return true;

    case ':':
      this->advance();
      this->make_token(TokenType::Colon, ":");
      return true;

    default:
      this->advance();
      this->make_token(TokenType::Unknown, std::string(1, c));
      return true;
  }
}


void Tokenizer::scan_string() {
  if (!this->is_string_apo(this->peek())) {
    return;
  }

  this->token_start_line = this->get_line_number();
  this->token_start_column = this->get_column_number();

  std::string value{};
  value.push_back(this->peek());
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
    } else if (this->peek() == '\n') {
      this->make_token(TokenType::Unknown, value);
      this->increment_line_number();
      this->reset_column_number();
      this->advance();
      return;
    } else {
      value.push_back(this->peek());
      this->advance();
    }
  }

  value.push_back(this->peek());
  this->make_token(TokenType::String, value);
  this->advance();
}


bool Tokenizer::match(const std::string& expected) {
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

  this->increment_column_number();
  return this->input[this->position++];
}

bool Tokenizer::is_file_end() {
  return this->position >= this->input.size();
}

bool Tokenizer::is_identifier_char(const char& c) {
  return std::isalnum(c) || (c == '_');
}

bool Tokenizer::is_string_apo(const char& c) {
  return c == '\"';
}

bool Tokenizer::is_whitespace_or_newline(const char& c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool Tokenizer::is_digit(const char& c) {
  return std::isdigit(c);
}

void Tokenizer::make_token(TokenType type, std::string value) {
  this->tokens.emplace_back(Token({
    .token_type = type,
    .token_value = value,
    .line_number = this->token_start_line,
    .column_number = this->token_start_column,
  }));
}

std::vector<Token> Tokenizer::get_tokens() {
  return this->tokens;
}

void Tokenizer::print_tokens() {
  for (const auto& token : this->tokens) {
    std::cout << std::format(
      "[TOKEN]  {:<20} | {:<25} | Ln {:>4}, Cn {:>4}\n",
      typeToStr(token.token_type),
      token.token_value,
      token.line_number,
      token.column_number
    );
  }
}

void Tokenizer::increment_line_number() {
  this->line_number++;
}

void Tokenizer::increment_column_number() {
  this->column_number++;
}

void Tokenizer::reset_column_number() {
  this->column_number = 1;
}

size_t Tokenizer::get_line_number() {
  return this->line_number;
}

size_t Tokenizer::get_column_number() {
  return this->column_number;
}
