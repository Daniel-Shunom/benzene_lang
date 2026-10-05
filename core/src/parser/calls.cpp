#include <ether/parser/parsers.hpp>

using std::nullopt;

auto parse_call_expression() -> Parser<NDCallExpr> {
  static Parser<NDCallExpr> parser = [](ParserState& state) -> PResult<NDCallExpr> {
    ParseCheckpoint checkpoint(state);

    auto ident = parse_identifier()(state);
    if (!ident) {
      return std::nullopt;
    }

    auto open_paren = match(TokenType::LParen)(state);
    if (!open_paren) {
      return std::nullopt;
    }

    std::vector<NDPtr> args;
    while (!state.is_at_end()) {
      if (auto close_paren = match(TokenType::RParen)(state)) {
        break;
      }

      auto expr = expect_wp(
        state,
        parse_primary_expression(),
        ParseErrorType::InvalidFuncCallExpr,
        "Function args require valid primary expression"
      );

      if (!expr) {
        return std::nullopt;
      }

      args.push_back(std::move(expr.value()));

      if (!match(TokenType::Delim)(state)) {
        auto close_paren = expect(
          state,
          TokenType::RParen,
          ParseErrorType::InvalidFuncCallExpr,
          "missing closing parenthsis `)`"
        );

        if (!close_paren) {
          return std::nullopt;
        }

        break;
      }
    }

    NDCallExpr call;
    call.identifier = std::make_unique<NDIdentifier>(ident.value());
    call.args = std::move(args);

    checkpoint.commit();
    return call;
  };
  return parser;
}

auto parse_call_exprs() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);
    auto func = parse_call_expression()(state);

    if (!func) {
      return std::nullopt;
    }

    auto next = state.peek();
    if (!next || next->token_type != TokenType::PipeOp) {
      // Not a pipe chain — let parse_value_expression's other arms handle it.
      return std::nullopt;
    }

    auto pipe_chain = NDCallChain();
    pipe_chain.start_token = func->identifier->identifier;
    pipe_chain.calls.push_back(
      std::make_unique<NDCallExpr>(std::move(func.value()))
    );

    while (!state.is_at_end()) {
      if (!match(TokenType::PipeOp)(state)) {
        break;
      }

      auto chain_func = expect_wp(
        state,
        parse_call_expression(),
        ParseErrorType::InvalidFuncCallExpr,
        "Expected a function call after `|=>`"
      );

      if (!chain_func) {
        return std::nullopt;
      }

      pipe_chain.calls.push_back(
        std::make_unique<NDCallExpr>(std::move(chain_func.value()))
      );
    }

    checkpoint.commit();
    return std::make_unique<NDCallChain>(std::move(pipe_chain));
  };
  return parser;
}
