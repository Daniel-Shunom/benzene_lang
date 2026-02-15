#include <cctype>
#include <format>
#include <iostream>
#include <string>
#include "lexer.hpp"
#include "../tokens/token_types.hpp"
#include "../tables/keyword_table.hpp"
#include "../tables/operator_table.hpp"


void Lexer::scan_tokens() {
  this->set_token_start();
  while(!this->is_file_end()) {
    char c = this->peek();

    if (this->is_whitespace_or_newline(c)) {
      this->advance();
      continue;
    }

    if (this->is_delim(c)) {
      this->make_token(TokenType::Delim, {','});
      this->advance();
      continue;
    }

    if (this->is_dot(c)) {
      this->make_token(TokenType::Dot, {'.'});
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

void Lexer::scan_keyword_or_identifier() {
  this->set_token_start();
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
    this->make_token(TokenType::Identifier, id);
  } else {
    switch (it->second) {
      case TokenType::CommentKeyword: return this->scan_comment();
      case TokenType::ImportKeyword: return this->scan_import_module();
      default: this->make_token(it->second, id);
    }
  }
}

void Lexer::scan_comment() {
  this->set_token_start();
  while(
    !this->is_file_end()
    && this->is_whitespace(this->peek())
  ) {
    this->advance();
  }

  if (this->peek() == '{') {
    this->advance();
    return this->scan_multi_line_comment();
  }

  // std::string comment{};

  while (
    !this->is_file_end()
    && !this->is_newline(this->peek())
  ) {
    // comment.push_back(this->peek());
    this->advance();
  }

  // this->make_token(TokenType::SLComment, comment);
}

void Lexer::scan_multi_line_comment() {
  this->set_token_start();
  // std::string ml_comment{};

  while (
    !this->is_file_end()
  ) {
    if (this->peek() == '}') {
      // this->make_token(TokenType::MLComment, ml_comment);
      this->advance();
      return;
    }

    if (this->peek() == '`') {
      // ml_comment.push_back(this->peek());
      this->advance();
      if (!this->is_file_end()) {
        // ml_comment.push_back(this->peek());
        this->advance();
        continue;
      }
    }

    // ml_comment.push_back(this->peek());
    this->advance();
  }

  // this->make_token(TokenType::UTComment, ml_comment);
}

void Lexer::scan_import_module() {
  this->set_token_start();
  std::string import_module{};

  while(!this->is_file_end()) {
    if (this->is_newline(this->peek())) {
      break;
    }

    if (this->is_whitespace(this->peek())) {
      this->advance();
      continue;
    }

    import_module.push_back(this->peek());
    this->advance();
  }

  this->make_token(TokenType::ImportModule, import_module);
}

void Lexer::scan_number() {
  this->set_token_start();
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
    this->make_token(TokenType::FloatLiteral, number);
  } else {
    this->make_token(TokenType::IntegerLiteral, number);
  }
}

bool Lexer::scan_operator() {
  this->set_token_start();
  for (const auto& [op, type]: OperatorList) {
    if (this->input.compare(this->position, op.size(), op) == 0) {
      for (const auto& _: op) this->advance();
      this->make_token(type, op);
      return true;
    }
  }

  return false;
}


bool Lexer::scan_other_symbol() {
  this->set_token_start();
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


void Lexer::scan_string() {
  if (!this->is_string_apo(this->peek())) {
    return;
  }

  this->set_token_start();
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
    } else if (this->is_newline(this->peek())) {
      this->make_token(TokenType::UTStringLiteral, value);
      this->advance();
      return;
    } else {
      value.push_back(this->peek());
      this->advance();
    }
  }

  this->make_token(TokenType::StringLiteral, value);
  // Skip closing apostrophe
  this->advance();
}


bool Lexer::match(const std::string& expected) {
  for (char c: expected) {
    if (this->peek() != c) return false;
    this->advance();
  }

  return true;
}

char Lexer::peek() {
  if (this->position >= this->input.size()) {
    return '\0';
  }

  return this->input[this->position];
}

char Lexer::advance() {
  if (this->position >= this->input.size()) {
    return '\0';
  }

  char c = this->input[this->position++];

  if (this->is_newline(c)) {
    this->line_number++;
    this->column_number=1;
  } else {
    this->column_number++;
  }

  return c;
}

bool Lexer::is_file_end() {
  return this->position >= this->input.size();
}

bool Lexer::is_identifier_char(const char& c) {
  return std::isalnum(c) || (c == '_');
}

bool Lexer::is_string_apo(const char& c) {
  return c == '\"';
}

bool Lexer::is_dot(const char& c) {
  return c == '.';
}

bool Lexer::is_whitespace_or_newline(const char& c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool Lexer::is_whitespace(const char& c) {
  return c == ' ' || c == '\t' || c == '\r';
}

bool Lexer::is_digit(const char& c) {
  return std::isdigit(c);
}

bool Lexer::is_delim(const char& c) {
  return c == ',';
}

bool Lexer::is_newline(const char& c) {
  return c == '\n';
}

void Lexer::make_token(TokenType type, std::string value) {
  this->tokens.emplace_back(Token({
    .token_type = type,
    .token_value = value,
    .line_number = this->token_start_line,
    .column_number = this->token_start_column,
  }));
}

std::vector<Token> Lexer::get_tokens() {
  return this->tokens;
}

void Lexer::print_tokens() {
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

size_t Lexer::get_line_number() {
  return this->line_number;
}

size_t Lexer::get_column_number() {
  return this->column_number;
}

void Lexer::set_token_start() {
  this->token_start_line = this->get_line_number();
  this->token_start_column = this->get_column_number();
}
