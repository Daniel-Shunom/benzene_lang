#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_call_exprs() -> Parser<NDPtr>;

auto parse_call_expression() -> Parser<NDCallExpr>;
