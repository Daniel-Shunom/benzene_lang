#include "..\token_types.hpp"
#include <vector>
#include <string>

static const std::vector<std::pair<std::string, TokenType>> OperatorList = {
  {">=", TokenType::Ge},

  {"<=", TokenType::Le},

  {"==", TokenType::EqEq},

  {"&&", TokenType::AndOp},

  {"||", TokenType::OrOp},

  {":>", TokenType::RtnTypeOp},

  {"+", TokenType::PlusOp},

  {"-", TokenType::MinusOp},

  {"*", TokenType::MultiplyOp},

  {"/", TokenType::DivideOp},

  {"%", TokenType::PercentOp},

  {">", TokenType::Gt},

  {"<", TokenType::Lt},

  {"=", TokenType::Eq},

  {"~", TokenType::NotOp},
};

