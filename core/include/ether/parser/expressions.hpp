#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_expression() -> Parser<NDPtr>;

auto parse_top_level_expressions() -> Parser<NDPtr>;

auto parse_body_expressions() -> Parser<NDPtr>;

auto parse_body_expression() -> Parser<NDPtr>;

auto parse_primary_expression() -> Parser<NDPtr>;

auto parse_value_expression() -> Parser<NDPtr>;
