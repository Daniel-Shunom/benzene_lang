#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_function_declaration() -> Parser<NDFuncDeclExpr>;

auto parse_lambda_expression() -> Parser<NDLambdaExpr>;
