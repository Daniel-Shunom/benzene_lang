#include <ether/parser/parsers.hpp>
#include <ether/tables/utils.hpp>

using std::nullopt;

auto m_parse_unary_expression() -> Parser<NDPtr> {
  static Parser<Token> unary_op = choice<Token>({
    match(TokenType::MinusOp),
    match(TokenType::NotOp)
  });
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);
    auto opt = optional(unary_op, state);

    auto expression = parse_primary_expression()(state);
    if (!expression) {
      return std::nullopt;
    }

    if (opt) {
      NDUnaryExpr expr;
      expr.op = opt;
      expr.rhs = std::move(expression.value());
      checkpoint.commit();
      return std::make_unique<NDUnaryExpr>(std::move(expr));
    }

    checkpoint.commit();
    return expression;
  };
  return parser;
}

auto m_parse_chain_left(Parser<NDPtr> term, Parser<Token> opr) -> Parser<NDPtr> {
  return [&, term, opr](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);

    auto left_res = term(state);

    if (!left_res) {
      return nullopt;
    }

    NDPtr left = std::move(left_res.value());

    while(!state.is_at_end()) {
      size_t start = state.pos;

      auto op_res = opr(state);
      if (!op_res) {
        break;
      }

      auto right_res = term(state);

      if (!right_res) {
        state.reset_pos(start);
        break;
      }

      auto bin = std::make_unique<NDBinaryExpr>();

      bin->lhs = std::move(left);
      bin->op  = std::move(op_res.value());
      bin->rhs = std::move(right_res.value());

      left = std::move(bin);
    }

    checkpoint.commit();
    return left;
  };
}

auto m_parse_multiplicative_op() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = m_parse_chain_left(
    m_parse_unary_expression(),
    choice<Token>({
      match(TokenType::MultiplyOp),
      match(TokenType::DivideOp)
    })
  );
  return parser;
}

auto m_parse_additive_op() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = m_parse_chain_left(
    m_parse_multiplicative_op(),
    choice<Token>({
      match(TokenType::PlusOp),
      match(TokenType::MinusOp)
    })
  );
  return parser;
}

auto m_parse_comparision_op() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = m_parse_chain_left(
    m_parse_additive_op(),
    choice<Token>({
      match(TokenType::Lt),
      match(TokenType::Le),
      match(TokenType::Gt),
      match(TokenType::Ge)
    })
  );
  return parser;
}

auto m_parse_equality() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = m_parse_chain_left(
    m_parse_comparision_op(),
    choice<Token>({
      match(TokenType::EqEq),
      match(TokenType::NtEq)
    })
  );
  return parser;
}

auto m_parse_logical_and() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = m_parse_chain_left(
    m_parse_equality(),
    match(TokenType::AndOp)
  );
  return parser;
}

auto m_parse_logical_or() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = m_parse_chain_left(
    m_parse_logical_and(),
    match(TokenType::OrOp)
  );
  return parser;
}

auto parse_binary_expression() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);
    auto bin_expr = m_parse_logical_or()(state);

    if (!bin_expr) {
      return std::nullopt;
    }

    checkpoint.commit();
    return bin_expr;
  };
  return parser;
}
