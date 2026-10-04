#pragma once
#include <ether/parser/parser_types.hpp>

auto parse_literal() -> Parser<NDLiteral>;

auto parse_identifier() -> Parser<NDIdentifier>;

auto match(TokenType) -> Parser<Token>;
