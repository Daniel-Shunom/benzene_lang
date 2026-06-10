#pragma once
#include <ether/parser/parser_types.hpp>
#include <ether/nodes/node_expr.hpp>

auto parse_literal() -> Parser<NDLiteral>;

auto parse_identifier() -> Parser<NDIdentifier>;

auto parse_const_expression() -> Parser<NDConstExpr>;

auto parse_let_expression() -> Parser<NDLetBindExpr>;

auto parse_scoped_expression() -> Parser<NDScopeExpr>;

auto parse_list_expression() -> Parser<NDListExpr>;

auto parse_tuple_expression() -> Parser<NDTupleExpr>;

auto parse_import_stmt() -> Parser<NDImportDirective>;

auto parse_function_declaration() -> Parser<NDFuncDeclExpr>;

auto parse_lambda_expression() -> Parser<NDLambdaExpr>;

auto parse_call_exprs() -> Parser<NDPtr>;

auto parse_call_expression() -> Parser<NDCallExpr>;

auto parse_case_expression() -> Parser<NDCaseExpr>;

auto parse_expression() -> Parser<NDPtr>;

auto parse_top_level_expressions() -> Parser<NDPtr>;

auto parse_body_expressions() -> Parser<NDPtr>;

auto parse_primary_expression() -> Parser<NDPtr>;

auto parse_value_expression() -> Parser<NDPtr>;

auto m_parse_chain_left(Parser<NDPtr>, Parser<Token>) -> Parser<NDPtr>;

auto m_parse_unary_expression() -> Parser<NDPtr>;

auto m_parse_additive_op() -> Parser<NDPtr>;

auto m_parse_multiplicative_op() -> Parser<NDPtr>;

auto m_parse_comparision_op() -> Parser<NDPtr>;

auto m_parser_equality_op() -> Parser<NDPtr>;

auto m_parse_logical_and() -> Parser<NDPtr>;

auto parse_binary_expression() -> Parser<NDPtr>;

auto match(TokenType) -> Parser<Token>;

auto parse_type_annotation() -> Parser<Token>;

auto run_parser(ParserState& state) -> PResult<Parent>;
