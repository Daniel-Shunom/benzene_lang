#pragma once
#include <unordered_map>
#include <string>
#include "..\token_types.hpp"

static const std::unordered_map<std::string, TokenType> KeywordTable({
  {"const", TokenType::ConstantKeyword},

  {"Nil", TokenType::NilLiteral},

  {"True", TokenType::TrueLiteral},

  {"False", TokenType::FalseLiteral},

  {"func", TokenType::FuncStart},

  {"end.", TokenType::FuncEnd},

  {"case", TokenType::Case},

  {"default", TokenType::Default},
});
