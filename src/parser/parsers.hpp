#pragma once
#include "parser_types.hpp"
#include "../nodes/node.hpp"
#include "../lexer/token_types.hpp"
#include <format>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

using TokenState = ParserState<Token>;
using ASTParser = Parser<Token, ASTPtr>;
using ASTResult = ParseResult<ASTPtr, Token>;
using ASTFunc = std::function<ASTParser (ASTPtr)>;

ASTParser parse_expr();

ASTParser parse_literal_expr();

ASTParser parse_constant_expr();

ASTParser parse_identifier_expr();

// need a helper with this one
ASTParser m_parse_binaryop_expr();

ASTParser m_parse_unaryop_expr();

ASTParser m_parse_primary();

ASTParser m_parse_mulplicative();

ASTParser m_parser_additive();

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

inline ASTParser p_parse_optional(ASTParser parser) {
  return ASTParser {
    [parser](TokenState& state) -> ASTResult {
      if (state.is_end()) {
        return std::unexpected(ParseError<Token>{
          .error_value = Token{},
          .message = "Error: reached unexpected EoF token"
        });
      }

      auto res = parser.apply(state);
      if (!res) {
        return ParseSuccess<ASTPtr, Token>{
          .value = nullptr,
          .state = state
        };
      }

      return res;
    }
  };
}

inline ASTParser p_parse_options(std::vector<ASTParser> parsers) {
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
        .message = "Error: no matching parser found"
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

inline ASTParser p_parse_bind(ASTParser parser, ASTFunc next) {
  return ASTParser{ 
    [parser, next](TokenState& state) -> ASTResult {
      if (state.is_end()) {
        return std::unexpected(ParseError<Token>{
          .error_value = Token{},
          .message = "Error: unexpected EoF token reached."
        });
      }

      auto res = parser.apply(state);
      if (!res) return res;

      ASTParser next_parser = next(std::move(res.value().value));
      return next_parser.apply(state);
    }
  };
}

inline ASTParser m_parse_chain_left(ASTParser term, ASTParser op) {
  return ASTParser{
    [term, op](TokenState& state) -> ASTResult {
      if(state.is_end()) {
        return std::unexpected(ParseError<Token>{
          .error_value = Token{},
          .message = "Error: unexpected EoF token reached."
        });
      }

      auto left_term = term.apply(state);
      if (!left_term) return left_term;

      while(true) {
        size_t start_pos = state.current_position;

        auto oper = op.apply(state);
        if (!oper) {
          state.current_position = start_pos;
          break;
        }

        auto right_term = term.apply(state);
        if (!right_term) {
          state.current_position = start_pos;
          break;
        }

        std::string op_str = static_cast<TokNode*>(oper->value.get())->token.token_value;

        auto binary_operation = std::make_unique<BinOpExpr>(
          std::move(left_term.value().value),
          op_str,
          std::move(right_term.value().value)
        );

        left_term->value = std::move(binary_operation);
      }

      return ParseSuccess<ASTPtr, Token>{
        .value = std::move(left_term.value().value),
        .state = state
      };
    }
  };
};
