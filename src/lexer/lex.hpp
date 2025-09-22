#pragma once
#include "../syntax/syntax.hpp"
#include "../syntax/symbols.hpp"
#include <string>
#include <vector>

class Lex {
public:
  Lex(const std::string &stream):
  pos(0),
  line(0),
  col(0)
  {
    this->ref_stream = stream;
  }
private:
  std::string ref_stream;
  size_t pos;
  size_t line;
  size_t col;

  char peekChar();
  bool isOperator(char);
  bool isStringOpen(char);
  Token readNumber();
  Token readString();
  Token readSymbol();
  Token readOperator();
  void skipWhitespace();
public:
  TOKEN_TYPE peek();
  Token advance();
};
