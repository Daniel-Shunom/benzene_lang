#include <ether/parser/parsers.hpp>

using std::nullopt;

auto parse_case_expression() -> Parser<NDCaseExpr> {
  static Parser<NDCaseExpr> parser = [](ParserState& state) -> PResult<NDCaseExpr> {
    ParseCheckpoint checkpoint(state);

    auto case_tok = match(TokenType::Case)(state);
    if (!case_tok) {
      return std::nullopt;
    }

    // The main condition to evaluate
    // Right now, the entire expression would only parse if
    // all control conditions and case expressions are syntactically
    // correct. Proper error handling grandularity needs to be introduced
    // in order to provide more conveniently triageable error messages
    // to the user.
    std::vector<NDPtr> conditions;
    while (state.peek() && state.peek().value().token_type != TokenType::Colon) {
      auto cond = expect_wp(
        state,
        parse_value_expression(),
        ParseErrorType::InvalidCaseExpr,
        "Expected a valid expression here"
      );

      if (!cond) {
        break;
      }

      conditions.push_back(std::move(cond.value()));

      auto delim = match(TokenType::Delim)(state);

      if (delim) {
        continue;
      }

      break;
    }

    auto colon = expect(
      state,
      TokenType::Colon,
      ParseErrorType::InvalidCaseExpr,
      "Expected `:` after case precondition"
    );

    if(!colon) {
      return std::nullopt;
    }

    ScopeStackGuard stack_guard(state, ScopeStackType::CaseExpr);

    std::vector<NDCaseExpr::Branch> branches;
    bool invalid_branch_arity = false;
    while (true) {
      if (match(TokenType::EndStmt)(state)) {
        break;
      }

      if (auto eof = match(TokenType::EoF)(state)) {
        Diagnostic diag;
        diag.level = DiagnosticLevel::Fail;
        diag.location.column = eof->column_number;
        diag.location.line = eof->line_number;
        diag.phase = DiagnosticPhase::Parser;
        diag.message = "Expected `end` keyword after case expression";
        state.diag_eng.report(diag);
        return std::nullopt;
      }

      const auto branch_start = state.peek();
      std::vector<NDPtr> patterns;
      auto pattern = parse_value_expression()(state);
      if (!pattern) {
        return std::nullopt;
      }
      patterns.push_back(std::move(pattern.value()));

      while (match(TokenType::Delim)(state)) {
        auto next_pattern = expect_wp(
          state,
          parse_value_expression(),
          ParseErrorType::InvalidCaseExpr,
          "Expected a pattern expression after `,`"
        );
        if (!next_pattern) {
          return std::nullopt;
        }
        patterns.push_back(std::move(next_pattern.value()));
      }

      if (patterns.size() != conditions.size()) {
        Diagnostic diag;
        diag.level = DiagnosticLevel::Fail;
        diag.phase = DiagnosticPhase::Parser;
        if (branch_start) {
          diag.location.line = branch_start->line_number;
          diag.location.column = branch_start->column_number;
        } else {
          diag.location.line = case_tok->line_number;
          diag.location.column = case_tok->column_number;
        }
        diag.message = "Each case branch must have one pattern for every condition";
        state.diag_eng.report(diag);
        invalid_branch_arity = true;
      }

      auto rtn_op = expect(
        state,
        TokenType::RtnTypeOp,
        ParseErrorType::InvalidCaseExpr,
        "Expected `:>` after condition"
      );

      if (!rtn_op) {
        return std::nullopt;
      }

      auto result = expect_wp(
        state,
        parse_body_expression(),
        ParseErrorType::InvalidCaseExpr,
        "Expected an expression"
      );

      if (!result) {
        return std::nullopt;
      }

      branches.push_back(NDCaseExpr::Branch{
        .pattern = std::move(patterns),
        .result = std::move(result.value())
      });
    }

    NDCaseExpr expr;
    expr.case_keyword = case_tok.value();
    expr.conditions = std::move(conditions);
    expr.branches = std::move(branches);
    expr.is_poisoned = invalid_branch_arity;

    checkpoint.commit();
    return expr;
  };
  return parser;
}

auto parse_scoped_expression() -> Parser<NDScopeExpr> {
  static Parser<NDScopeExpr> parser = [](ParserState& state) -> PResult<NDScopeExpr>{
    ParseCheckpoint checkpoint(state);

    auto open_brace = match(TokenType::LBrace)(state);
    if (!open_brace) {
      return std::nullopt;
    }

    std::vector<NDPtr> exprs{};
    while (!match(TokenType::RBrace)(state)) {
      auto expr = parse_expression()(state);
      if (!expr) {
        state.skip_until(TokenType::RBrace);
        break;
      }
      exprs.push_back(std::move(expr.value()));
    }

    NDScopeExpr scope_expr;
    scope_expr.open_brace = open_brace.value();
    scope_expr.expressions = std::move(exprs);


    checkpoint.commit();
    return scope_expr;
  };
  return parser;
}
