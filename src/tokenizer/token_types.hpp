#pragma once
#include <string>

enum class TokenType {
  Identifier, 

  ConstantKeyword, LetKeyword,

  Integer, Float, String,

  TrueLiteral, FalseLiteral, NilLiteral,

  Plus, Minus, Divide, Multiply, Percent,

  Gt, Ge, Lt, Le, Eq, EqEq,

  FuncStart, FuncEnd, RtnTypeOp, And, Or, Not,

  Case, Default,

  LParen, RParen, LBrac, RBrac, LBrace, RBrace, Pipe, Colon,

  EoF, Unknown
};


struct Token {
  TokenType token_type;
  std::string token_value;
  size_t line_number;
  size_t column_number;
};


inline static std::string typeToStr(TokenType type) {
  using t = TokenType;
  switch (type) {
    case t::Identifier: return "Identifier";
    case t::LetKeyword: return "LetKeyword";
    case t::ConstantKeyword: return "ConstantKeyword";
    case t::Integer: return "Integer";
    case t::Float: return "Float";
    case t::String: return "String";
    case t::TrueLiteral: return "TrueLiteral";
    case t::FalseLiteral: return "FalseLiteral";
    case t::NilLiteral: return "NilLiteral";
    case t::Plus: return "Plus";
    case t::Minus: return "Minus";
    case t::Divide: return "Divide";
    case t::Multiply: return "Multiply";
    case t::Percent: return "Percent";
    case t::Gt: return "Gt";
    case t::Ge: return "Ge";
    case t::Lt: return "Lt";
    case t::Le: return "Le";
    case t::Eq: return "Eq";
    case t::EqEq: return "EqEq";
    case t::FuncStart: return "FuncStart";
    case t::FuncEnd: return "FuncEnd";
    case t::RtnTypeOp: return "RtnTypeOp";
    case t::And: return "And";
    case t::Or: return "Or";
    case t::Not: return "Not";
    case t::Case: return "Case";
    case t::Default: return "Default";
    case t::LParen: return "LParen";
    case t::RParen: return "RParen";
    case t::LBrac: return "LBrac";
    case t::RBrac: return "RBrac";
    case t::LBrace: return "LBrace";
    case t::RBrace: return "RBrace";
    case t::Pipe: return "Pipe";
    case t::Colon: return "Colon";
    case t::EoF: return "EoF";
    case t::Unknown: return "Unknown";
  }

  return "Unknown";
}
