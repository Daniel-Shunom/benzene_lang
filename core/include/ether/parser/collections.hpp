#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_list_expression() -> Parser<NDListExpr>;

auto parse_tuple_expression() -> Parser<NDTupleExpr>;
