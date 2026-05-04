#pragma once
#include <ether/tables/keyword_table.hpp>
#include <ether/tables/literal_table.hpp>
#include <ether/tables/operator_table.hpp>

inline bool is_keyword(const Token& tok) {
  return KeywordTable.contains(tok.token_value);
}

inline bool is_operator(const Token& tok) {
  for (const auto& [_, type] : OperatorList) {
    if (tok.token_type == type) return true;
  }
  return false;
}

inline bool is_literal(const Token& tok) {
  return LiteralTable.contains(tok.token_type);
}
