#pragma once
#include <string>

enum class TokenType {
  Identifier, 

  ConstantKeyword,

  Integer, Float, String,

  TrueLiteral, FalseLiteral, NilLiteral,

  Plus, Minus, Divide, Multiply, Percent,

  Gt, Ge, Lt, Le, Eq, EqEq,

  FuncStart, FuncEnd, RtnTypeOp, And, Or, Not,

  Case, Default,

  LParen, RParen, LBrac, RBrac, LBrace, RBrace, Pipe,

  EoF, Unknown
};


using Token = struct {
  TokenType token_type;
  std::string token_value;
};
