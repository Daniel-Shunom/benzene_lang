#pragma once
#include "parser_types.hpp"
#include "../../nodes/node_expr.hpp"

Parser<NDLiteral> parse_literal();

Parser<NDIdentifier> parse_identifier();

Parser<NDLetBindExpr> parse_let_expression();

Parser<NDConstExpr> parse_const_expression();

Parser<NDFuncDeclExpr> parse_function_declaration();

Parser<NDCallExpr> parse_call_expression();

Parser<NDCaseExpr> parse_case_expression();

Parser<NDPtr> parse_expression();

Parser<Token> match(TokenType);

PResult<NDPtr> run_parser(ParserState& state);
