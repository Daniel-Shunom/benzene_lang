#pragma once
#include <unordered_map>
#include <string>
#include "../tokens/token_types.hpp"

static const std::unordered_map<std::string, TokenType> KeywordTable({
  {"Load", TokenType::ImportKeyword},

  {"Cmt", TokenType::CommentKeyword},

  {"const", TokenType::ConstantKeyword},

  {"let", TokenType::LetKeyword},

  {"Nil", TokenType::NilLiteral},

  {"True", TokenType::TrueLiteral},

  {"False", TokenType::FalseLiteral},

  {"func", TokenType::FuncStart},

  {"end", TokenType::EndStmt},

  {"case", TokenType::Case},

  {"default", TokenType::Default},
});
