#include <vector>
#include <string>
#include "../tokens/token_types.hpp"

static const std::vector<std::pair<std::string, TokenType>> OperatorList = {
  {"|=> ", TokenType::PipeOp},

  {">=", TokenType::Ge},

  {"<=", TokenType::Le},

  {"~=", TokenType::NtEq},

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

