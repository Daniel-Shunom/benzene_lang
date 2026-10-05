#pragma once
#include <ether/parser/parser_types.hpp>

auto m_parse_chain_left(Parser<NDPtr>, Parser<Token>) -> Parser<NDPtr>;

auto m_parse_unary_expression() -> Parser<NDPtr>;

auto m_parse_additive_op() -> Parser<NDPtr>;

auto m_parse_multiplicative_op() -> Parser<NDPtr>;

auto m_parse_comparision_op() -> Parser<NDPtr>;

auto m_parser_equality_op() -> Parser<NDPtr>;

auto m_parse_logical_and() -> Parser<NDPtr>;

auto parse_binary_expression() -> Parser<NDPtr>;
