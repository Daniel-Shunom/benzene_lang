#pragma once
#include <unordered_map>
#include <string>
#include "../tokens/token_types.hpp"

static const std::unordered_map<std::string, TokenType> LiteralTable({
  {"Nil", TokenType::NilLiteral},

  {"True", TokenType::TrueLiteral},

  {"False", TokenType::TrueLiteral},

  {"Int", TokenType::IntegerLiteral},

  {"Float", TokenType::FloatLiteral},
});
