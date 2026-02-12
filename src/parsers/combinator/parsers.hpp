#pragma once
#include "parser_types.hpp"
#include "../../nodes/node_expr.hpp"

Parser<NDLiteral> parse_literal();

Parser<NDIdentifier> parse_identifier();

Parser<NDConstExpr> parse_const_expression();

Parser<NDLetBindExpr> parse_let_expression();

Parser<NDScopeExpr> parse_scoped_expression();

Parser<NDImportDirective> parse_import_stmt();

Parser<NDFuncDeclExpr> parse_function_declaration();

Parser<NDPtr> parse_call_exprs();

Parser<NDCallExpr> parse_call_expression();

Parser<NDCaseExpr> parse_case_expression();

Parser<NDPtr> parse_expression();

Parser<NDPtr> parse_primary_expression();

Parser<NDPtr> parse_value_expression();

Parser<NDPtr> m_parse_chain_left(Parser<NDPtr>, Parser<Token>);

Parser<NDPtr> m_parse_unary_expression();

Parser<NDPtr> m_parse_additive_op();

Parser<NDPtr> m_parse_multiplicative_op();

Parser<NDPtr> m_parse_comparision_op();

Parser<NDPtr> m_parser_equality_op();

Parser<NDPtr> m_parse_logical_and();

Parser<NDPtr> parse_binary_expression();

Parser<Token> match(TokenType);

Parser<Token> parse_type_annotation();

PResult<Parent> run_parser(ParserState& state);
