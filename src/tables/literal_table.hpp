#pragma once
#include <unordered_set>
#include "../tokens/token_types.hpp"

static const std::unordered_set<TokenType> LiteralTable = {
  TokenType::NilLiteral,
  TokenType::TrueLiteral,
  TokenType::FalseLiteral,
  TokenType::IntegerLiteral,
  TokenType::FloatLiteral,
  TokenType::StringLiteral,
  TokenType::UTStringLiteral,
};
