#pragma once
#include "parser_types.hpp"
#include "../nodes/node.hpp"
#include "../lexer/token_types.hpp"
#include <format>
#include <memory>
#include <vector>

using TokenState = ParserState<Token>;
using ASTParser = Parser<Token, ASTPtr>;
using ASTResult = ParseResult<ASTPtr, Token>;

ASTParser parse_expr();

ASTParser parse_literal_expr();

ASTParser parse_constant_expr();

ASTParser parse_identifier_expr();

// need a helper with this one
ASTParser parse_binaryop_expr();

ASTParser parse_unaryop_expr();

ASTParser parse_let_bind_expr();

ASTParser parse_func_invoc_expr();

ASTParser parse_case_expr();

ASTParser parse_func_decl_expr();

ASTParser parse_func_params();

ASTParser parse_func_decl_body();

ASTResult run_parser(ASTParser, TokenState&);

inline ASTParser p_parse_seq(std::vector<ASTParser> parsers) {
  return ASTParser{
    [parsers](TokenState& state) -> ASTResult {
      auto start_pos = state.current_position;
      std::vector<ASTPtr> nodes{};

      for (const auto& parser : parsers) {
        if (state.is_end()) {
          return std::unexpected(ParseError<Token>{
            .error_value = Token{},
            .message = "Error: reached unexpected EoF token"
          });
        }

        auto res = parser.apply(state);
        if (!res) {
          state.current_position = start_pos;
          return res;
        }
        nodes.push_back(std::move(res.value().value));
      }

      SeqNode seqn(std::move(nodes));

      return ParseSuccess<ASTPtr, Token>{
        .value = std::make_unique<SeqNode>(std::move(seqn)),
        .state = state
      };
    }
  };
}

inline ASTParser p_parse_first_match(std::vector<ASTParser> parsers) {
  return ASTParser{
    [parsers](TokenState& state) -> ASTResult {
      auto start_pos = state.current_position;
      for (const auto& parser : parsers) {
        state.current_position = start_pos;
        if (state.is_end()) {
          return std::unexpected(ParseError<Token>{
            .error_value = Token{},
            .message = "Error: reached unexpected EoF token"
          });
        }

        auto res = parser.apply(state);
        if (res) return res;
      }

      return std::unexpected(ParseError<Token>{
        .error_value = Token{},
        .message = ""
      });
    }
  };
}

inline ASTParser p_parse_match(TokenType type) {
  return ASTParser{
    [type](TokenState& state) -> ASTResult {
      auto current = state.current();
      if (!current.has_value()) {
        return std::unexpected(ParseError<Token>{
          .error_value = Token{},
          .message = "Error: token not found for matching"
        });
      }

      if (current->token_type==type) {
        state.advance();
        return ParseSuccess<ASTPtr, Token> {
          .value = std::make_unique<TokNode>(current.value()),
          .state = state,
        };
      }

      std::string expected = typeToStr(type);
      std::string gotten = typeToStr(current->token_type);
      std::string error_msg = std::format(
        "Error: expected {} but instead got {}",
        expected, gotten
      );

      return std::unexpected(ParseError<Token>{
        .error_value = current.value(),
        .message = error_msg
      });
    }
  };
}

inline ASTParser p_parse_option_of(std::vector<ASTParser> parsers) {

}
