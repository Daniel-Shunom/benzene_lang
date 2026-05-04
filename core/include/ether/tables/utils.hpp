#pragma once
#include <ether/tables/keyword_table.hpp>
#include <ether/tables/literal_table.hpp>
#include <ether/tables/operator_table.hpp>

static inline bool is_keyword(Token& type) {
  return KeywordTable.contains(type.token_value);
}

static inline bool is_operator(Token& token) {
  for (const auto& [_, type]: OperatorList) {
    if (token.token_type == type) return true;
  }
  return false;
}

static inline bool is_literal(Token& token) {
  return LiteralTable.contains(token.token_type);
}
