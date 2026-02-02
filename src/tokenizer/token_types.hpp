#pragma once
#include <string>

enum class TokenType {
  ImportKeyword,
  ImportModule,
  Identifier,

  CommentKeyword,
  MLComment,
  SLComment,
  UTComment,

  ConstantKeyword,
  LetKeyword,

  IntegerLiteral,
  FloatLiteral,
  StringLiteral,
  UTStringLiteral,
  TrueLiteral,
  FalseLiteral,
  NilLiteral,

  PlusOp,
  MinusOp,
  DivideOp,
  MultiplyOp,
  PercentOp,

  Gt,
  Ge,
  Lt,
  Le,
  Eq,
  EqEq,

  FuncStart,
  EndStmt,
  RtnTypeOp,

  AndOp,
  OrOp,
  NotOp,

  Case,
  Default,

  LParen,
  RParen,
  LBrac,
  RBrac,
  LBrace,
  RBrace,

  Pipe,
  Colon,
  Delim,
  Dot,

  EoF,
  Unknown
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
    case t::ImportKeyword: return "ImportKeyword";
    case t::ImportModule: return "ImportModule";
    case t::Identifier: return "Identifier";
    case t::LetKeyword: return "LetKeyword";
    case t::ConstantKeyword: return "ConstantKeyword";
    case t::IntegerLiteral: return "IntegerLiteral";
    case t::FloatLiteral: return "FloatLiteral";
    case t::StringLiteral: return "StringLiteral";
    case t::UTStringLiteral: return "UT StringLiteral";
    case t::TrueLiteral: return "TrueLiteral";
    case t::FalseLiteral: return "FalseLiteral";
    case t::NilLiteral: return "NilLiteral";
    case t::PlusOp: return "Plus";
    case t::MinusOp: return "Minus";
    case t::DivideOp: return "Divide";
    case t::MultiplyOp: return "Multiply";
    case t::PercentOp: return "Percent";
    case t::Gt: return "Gt";
    case t::Ge: return "Ge";
    case t::Lt: return "Lt";
    case t::Le: return "Le";
    case t::Eq: return "Eq";
    case t::EqEq: return "EqEq";
    case t::FuncStart: return "FuncStart";
    case t::EndStmt: return "EndStmt";
    case t::RtnTypeOp: return "RtnTypeOp";
    case t::AndOp: return "And";
    case t::OrOp: return "Or";
    case t::NotOp: return "Not";
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
    case t::Delim: return "Delimitter";
    case t::Dot: return "Dot";
    case t::CommentKeyword: return "CommentKeyword";
    case t::SLComment: return "Comment";
    case t::MLComment: return "Multi-Line Comment";
    case t::UTComment: return "UT Comment";
    case t::EoF: return "EoF";
    case t::Unknown: return "Unknown";
  }

  return "Unknown";
}
