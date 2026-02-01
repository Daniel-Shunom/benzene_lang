#include "..\token_types.hpp"
#include <vector>
#include <string>

static const std::vector<std::pair<std::string, TokenType>> OperatorList = {
  {">=", TokenType::Ge},

  {"<=", TokenType::Le},

  {"==", TokenType::EqEq},

  {"&&", TokenType::And},

  {"||", TokenType::Or},

  {":>", TokenType::RtnTypeOp},

  {"+", TokenType::Plus},

  {"-", TokenType::Minus},

  {"*", TokenType::Multiply},

  {"/", TokenType::Divide},

  {"%", TokenType::Percent},

  {">", TokenType::Gt},

  {"<", TokenType::Lt},

  {"=", TokenType::Eq},

  {"~", TokenType::Not},
};

