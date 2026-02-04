#pragma once
#include "parser_types.hpp"
#include "../nodes/node.hpp"
#include "../lexer/token_types.hpp"
#include <functional>
#include <vector>

using TokenState = ParserState<Token>;
using ASTParser = Parser<Token, ASTPtr>;
using ASTResult = ParseResult<ASTPtr, Token>;
using ASTFunc = std::function<ASTParser (ASTPtr)>;

ASTParser parse_expr();

ASTParser parse_literal_expr();

ASTParser parse_constant_expr();

ASTParser parse_identifier_expr();

ASTParser m_parse_binaryop_expr();

ASTParser m_parse_unaryop_expr();

ASTParser m_parse_primary();

ASTParser m_parse_mulplicative();

ASTParser m_parser_additive();

ASTParser parse_let_bind_expr();

ASTParser parse_func_invoc_expr();

// ASTParser parse_case_expr();

ASTParser parse_func_decl_expr();

ASTParser parse_func_params();

ASTParser parse_func_decl_body();

ASTParser p_parse_seq(std::vector<ASTParser> parsers);

ASTParser p_parse_options(std::vector<ASTParser> parsers);

ASTParser p_parse_optional(ASTParser parser);

ASTParser p_parse_match(TokenType type);

ASTParser p_parse_bind(ASTParser parser, ASTFunc next);

ASTParser m_parse_chain_left(ASTParser term, ASTParser op); ;

ASTResult run_parser(ASTParser parser, TokenState& state);
