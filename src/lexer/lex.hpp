#pragma once
#include <vector>
#include <string>
#include "../types/syntax_t.hpp"

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
  std::vector<Token> tokens;
  std::string ref_stream;
  size_t pos;
  size_t line;
  size_t col;

  char peekChar();
  bool isOperator(const std::string&);
  bool isStringOpen(char);
  bool isSymbol(char);
  Token readNumber();
  Token readString();
  Token readSymbol();
  Token readOperator();
  void skipWhitespace();
public:
  TOKEN_TYPE peek();
  Token advance();
  void extractAllTokens();
  std::vector<Token> getTokenList();
};
