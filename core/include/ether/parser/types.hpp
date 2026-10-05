#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_explicit_type_annotation() -> Parser<NDExplicitTypeAnot>;

auto parse_type_annotation() -> Parser<NDTypeExpr>;

auto parse_type_declaration() -> Parser<NDTypeDecl>;

auto parse_type_expression() -> Parser<NDTypeExpr>;

auto h_parse_type_expr_ident() -> Parser<TypePtr>;

auto h_parse_type_expr_ident_wtagged_params(std::string) -> Parser<TypePtr>;

auto h_parse_type_expr_lambda() -> Parser<TypePtr>;

auto is_allowed_type(std::string type) -> bool;

