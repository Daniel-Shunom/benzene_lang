#include "parsers.hpp"
#include <memory>
#include <format>
#include <print>
#include <utility>

ASTParser parse_let_bind_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::LetKeyword),
    p_parse_match(TokenType::Identifier),
    parse_expr()
  });
}

ASTParser parse_literal_expr() {
  return p_parse_options({
    p_parse_match(TokenType::IntegerLiteral),
    p_parse_match(TokenType::FloatLiteral),
    p_parse_match(TokenType::StringLiteral),
    p_parse_match(TokenType::UTStringLiteral),
    p_parse_match(TokenType::NilLiteral),
    p_parse_match(TokenType::TrueLiteral),
    p_parse_match(TokenType::FalseLiteral),
  });
}

ASTParser parse_identifier_expr() {
  return p_parse_match(TokenType::Identifier);
}

ASTParser parse_constant_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::ConstantKeyword),
    p_parse_match(TokenType::Identifier),
    p_parse_options({
      parse_literal_expr(),
      parse_expr()
    })
  });
}

ASTParser parse_func_invoc_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::Identifier),
    p_parse_match(TokenType::RParen),
    p_parse_options({
      p_parse_match(TokenType::Identifier),
      parse_expr(),
    }),
    p_parse_match(TokenType::LParen)
  });
}

// incomplete, but will come back later.
ASTParser parse_func_params() {
  return p_parse_options({
    p_parse_seq({
      p_parse_match(TokenType::Identifier), 
      p_parse_match(TokenType::Colon),
      p_parse_match(TokenType::Identifier)
    }),
    p_parse_match(TokenType::Identifier)
  });
}

ASTParser parse_func_decl_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::FuncStart),
    p_parse_match(TokenType::Identifier),
    p_parse_match(TokenType::RParen),
    parse_func_params(),
    p_parse_match(TokenType::LParen),
    parse_func_decl_body(),
    p_parse_match(TokenType::EndStmt)
  });
}

ASTParser m_parse_unaryop_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::MinusOp),
    m_parse_primary()
  });
}

ASTParser m_parse_mulplicative() {
  return m_parse_chain_left(
    m_parse_unaryop_expr(), 
    p_parse_options({
      p_parse_match(TokenType::MultiplyOp),
      p_parse_match(TokenType::DivideOp)
    })
  );
}

ASTParser m_parse_additive() {
  return m_parse_chain_left(
    m_parse_mulplicative(),
    p_parse_options({
      p_parse_match(TokenType::PlusOp),
      p_parse_match(TokenType::MinusOp)
    })
  );
}

ASTParser m_parse_primary() {
  return p_parse_options({
    parse_literal_expr(),
    parse_identifier_expr(),
    p_parse_seq({
      p_parse_match(TokenType::LParen),
      parse_expr(),
      p_parse_match(TokenType::RParen)
    })
  });
}


ASTParser parse_expr() {
  return p_parse_options({
    parse_let_bind_expr(),
    parse_constant_expr(),
    parse_func_decl_expr(),
    m_parse_additive()
  });
}

ASTResult run_parser(ASTParser parser, TokenState& state) {
  return parser.apply(state);
}

ASTParser m_parse_chain_left(ASTParser term, ASTParser op) {
  return ASTParser{
    [&term, &op](TokenState& state) mutable -> ASTResult {
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
}

ASTParser p_parse_bind(ASTParser parser, ASTFunc next) {
  return ASTParser{ 
    [&parser, &next](TokenState& state) mutable -> ASTResult {
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

ASTParser p_parse_match(TokenType type) {
  return ASTParser{
    [&type](TokenState& state) mutable -> ASTResult {
      auto current = state.current();
      if (!current.has_value()) {
        std::string message("Error: toke not found for matching");
        std::println(message);
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

      std::println(error_msg);
      return std::unexpected(ParseError<Token>{
        .error_value = current.value(),
        .message = error_msg
      });
    }
  };
}

ASTParser p_parse_options(std::vector<ASTParser> parsers) {
  return ASTParser{
    [&parsers](TokenState& state) mutable -> ASTResult {
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

ASTParser parse_func_decl_body() {
  return ASTParser{
    [&](TokenState& state) mutable -> ASTResult {
      std::vector<ASTPtr> statements;

      if (state.is_end()) {
        std::string message("Error: unpexpected token reached");
        std::println(message);
        return std::unexpected(ParseError<Token>{
          .error_value = Token{},
          .message = "Error: unexpected EoF token reached."
        });
      }

      while (state.current()->token_type != TokenType::EndStmt) {
        auto expr = parse_expr().apply(state);
        // Just to see for now. Will handle later;
        if(!expr) continue;
        statements.push_back(std::move(expr->value));
      }

      auto func_body = std::make_unique<FunctionDeclBody>();
      func_body->statements = std::move(statements);

      return ParseSuccess<ASTPtr, Token>{
        .value = std::move(func_body),
        .state = state
      };
    }
  };
}

ASTParser p_parse_seq(std::vector<ASTParser> parsers) {
  return ASTParser{
    [&parsers](TokenState& state) mutable -> ASTResult {
      auto start_pos = state.current_position;
      std::vector<ASTPtr> nodes{};

      for (const auto& parser : parsers) {
        if (state.is_end()) {
          std::string message("Error: unexpected EoF Token reached.");
          std::println(message);

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

ASTParser p_parse_optional(ASTParser parser) {
  return ASTParser {
    [&parser](TokenState& state) mutable -> ASTResult {
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
