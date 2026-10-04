#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_scoped_expression() -> Parser<NDScopeExpr>;

auto parse_case_expression() -> Parser<NDCaseExpr>;
