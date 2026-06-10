#pragma once
#include <ether/tables/keyword_table.hpp>
#include <ether/tables/literal_table.hpp>
#include <ether/tables/operator_table.hpp>

inline auto is_keyword(const Token& tok) -> bool {
  return KeywordTable.contains(tok.token_value);
}

inline auto is_operator(const Token& tok) -> bool {
  for (const auto& [var/*unused*/, type] : OperatorList) {
    if (tok.token_type == type) {
      return true;
    }
  }
  return false;
}

inline auto is_literal(const Token& tok) -> bool {
  return LiteralTable.contains(tok.token_type);
}
