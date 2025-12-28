#pragma once
#include <memory>
#include <vector>
#include "../ast/expr_l.hpp"
#include "../types/syntax_t.hpp"
#include "../scopes/env.hpp"
#include "../scopes/params.hpp"

class Parser {
public:
  Parser(std::vector<Token> tokens) {
    this->tokens = std::move(tokens);
  }

  ExprKind peek();
  std::unique_ptr<Token> peekNextTok();
  std::unique_ptr<Token> tokFromCurrPos(size_t);
  Token advance();
  Token match();
 
  void parseTokens();
  void advanceNext() {
    this->pos++;
  }

  Expr parseExpression();
  Expr parseBinaryOp(float = 0);

  Expr parsePrefixExpression();
  Expr parseSuffixExpression();

  Expr parseCaseExpression();

  Expr parseBooleanOp();
  Expr parseFuncExpression();

  Expr parseVarExpression();
  Expr parseConstExpression();
  
  std::pair<float, float> getBinOpPrecedence(std::string&);
  std::pair<float, float> getBoolOpPrecedence(std::string&);

private:
  Env scope;
  FunctionRegistry function_registry;
  size_t pos = 0;
  std::vector<Token> tokens;
  std::vector<Expr>  expressions;
};
