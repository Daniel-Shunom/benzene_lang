#include <ether/parser/parsers.hpp>
#include <ether/tables/utils.hpp>
#include <unordered_map>

using std::nullopt;

auto parse_identifier() -> Parser<NDIdentifier> {
  static Parser<NDIdentifier> parser = [](ParserState& state) -> PResult<NDIdentifier> {
    auto token = match(TokenType::Identifier)(state);
    if (!token) {
      return std::nullopt;
    }

    NDIdentifier idt;
    idt.identifier = token.value();
    return idt;
  };
  return parser;
}

auto parse_literal() -> Parser<NDLiteral> {
  static Parser<NDLiteral> parser = [](ParserState& state) -> PResult<NDLiteral> {
    auto token = state.peek();
    if (!token) {
      return std::nullopt;
    }
    if (!is_literal(token.value())) {
      return std::nullopt;
    }
    auto literal = NDLiteral();
    literal.literal = token.value();
    state.advance();
    return literal;
  };
  return parser;
}

auto match(TokenType type) -> Parser<Token> {
  static std::unordered_map<TokenType, Parser<Token>> cache;
  if (auto itr = cache.find(type); itr != cache.end()) {
    return itr->second;
  }

  Parser<Token> parser = [type](ParserState& state) -> PResult<Token> {
    auto tok = state.peek();
    if (!tok) {
      return std::nullopt;
    }
    if (tok->token_type == type) {
      state.advance();
      return tok;
    }
    return std::nullopt;
  };
  cache.emplace(type, parser);
  return parser;
}
