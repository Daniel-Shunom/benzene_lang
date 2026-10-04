#include <ether/parser/parsers.hpp>

using std::nullopt;

auto parse_list_expression() -> Parser<NDListExpr> {
  static Parser<NDListExpr> parser = [](ParserState& state) -> PResult<NDListExpr> {
    ParseCheckpoint checkpoint(state);

    auto open_brac = match(TokenType::LBrac)(state);
    if (!open_brac) {
      return std::nullopt;
    }

    std::vector<NDPtr> exprs;

    if (!match(TokenType::RBrac)(state)) {
      while (true) {
        auto expr = parse_expression()(state);
        if (!expr) {
          return std::nullopt;
        }

        exprs.push_back(std::move(expr.value()));

        if (match(TokenType::RBrac)(state)) {
          break;
        }

        if (!match(TokenType::Delim)(state)) {
          return std::nullopt;
        }
      }
    }

    NDListExpr list_expr;
    list_expr.open_brac = open_brac.value();
    list_expr.values = std::move(exprs);

    checkpoint.commit();
    return list_expr;
  };
  return parser;
}

auto parse_tuple_expression() -> Parser<NDTupleExpr> {
  static Parser<NDTupleExpr> parser = [](ParserState& state) -> PResult<NDTupleExpr> {
    ParseCheckpoint checkpoint(state);

    auto tuple_start = match(TokenType::TupleStart)(state);
    if (!tuple_start) {
      return std::nullopt;
    }

    auto open_brace = match(TokenType::LBrace)(state);
    if (!open_brace) {
      return std::nullopt;
    }

    std::vector<NDPtr> exprs;

    if (!match(TokenType::RBrace)(state)) {
      while (true) {
        auto expr = parse_expression()(state);
        if (!expr) {
          return std::nullopt;
        }

        exprs.push_back(std::move(expr.value()));

        if (match(TokenType::RBrace)(state)) {
          break;
        }

        if (!match(TokenType::Delim)(state)) {
          return std::nullopt;
        }
      }
    }

    NDTupleExpr tuple_expr;
    tuple_expr.at_sym = tuple_start.value();
    tuple_expr.values = std::move(exprs);

    checkpoint.commit();
    return tuple_expr;
  };
  return parser;
}
