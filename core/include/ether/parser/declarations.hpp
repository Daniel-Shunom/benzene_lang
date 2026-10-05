#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_const_expression() -> Parser<NDConstExpr>;

auto parse_let_expression() -> Parser<NDLetBindExpr>;

auto parse_import_stmt() -> Parser<NDImportDirective>;
