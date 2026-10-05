#include "ether/parser/types.hpp"
#include <ether/parser/parsers.hpp>

using std::nullopt;

namespace {
auto parse_parameters(ParserState& state) -> PResult<std::vector<NDFuncParam>> {
  std::vector<NDFuncParam> params;
  while (!match(TokenType::RParen)(state)) {
    auto identifier = expect(state, TokenType::Identifier,
                             ParseErrorType::InvalidFuncDeclExpr,
                             "Function args require valid identifiers");
    if (!identifier) {
      return std::nullopt;
    }
    NDFuncParam parameter;
    parameter.identifier.identifier = *identifier;
    if (state.peek() && state.peek()->token_type == TokenType::Colon) {
      auto annotation = parse_type_annotation()(state);
      if (!annotation) {
        return std::nullopt;
      }
      parameter.param_type = std::move(*annotation);
    }
    params.push_back(std::move(parameter));
    if (!match(TokenType::Delim)(state)) {
      if (!expect(state, TokenType::RParen, ParseErrorType::InvalidFuncDeclExpr,
                  "Missing closing parenthesis ')'")) {
        return std::nullopt;
      }
      break;
    }
  }
  return params;
}
} // namespace

auto parse_lambda_expression() -> Parser<NDLambdaExpr> {
  static Parser<NDLambdaExpr> parser = [](ParserState& state) -> PResult<NDLambdaExpr> {
    ParseCheckpoint checkpoint(state);

    auto lambda_start = match(TokenType::LambdaKeyword)(state);

    if (!lambda_start) {
      return std::nullopt;
    }

    auto left_paren = expect(
      state,
      TokenType::LParen,
      ParseErrorType::InvalidFuncDeclExpr,
      "`Missng open parenthesis `(`"
    );

    if (!left_paren) {
      return std::nullopt;
    }

    auto params = parse_parameters(state);
    if (!params) {
      return std::nullopt;
    }



    std::optional<NDTypeExpr> func_rtn_type;
    if (auto rtnop = match(TokenType::RtnTypeOp)(state)) {
      func_rtn_type = expect_wp(
        state,
        parse_type_expression(),
        ParseErrorType::InvalidFuncDeclExpr,
        "Function is missing the indicated return type"
      );
    }

    ScopeStackGuard stack_guard(state, ScopeStackType::Lambda);

    std::vector<NDPtr> body{};
    while (true) {
      if (match(TokenType::EndStmt)(state)) {
        break;
      }

      auto expr = parse_body_expression()(state);
      if (!expr) {
        state.skip_until(TokenType::EndStmt);
        break;
      };


      body.push_back(std::move(expr.value()));


      if (auto eof = match(TokenType::EoF)(state)) {
        Diagnostic diag;
        diag.level = DiagnosticLevel::Fail;
        diag.location.column = eof->column_number;
        diag.location.line = eof->line_number;
        diag.phase = DiagnosticPhase::Parser;
        diag.message = "Expected `end` keyword after lambda declaration";
        state.diag_eng.report(diag);
        return std::nullopt;
      }
    }

    NDLambdaExpr lambda;
    lambda.lambda_start = lambda_start.value();
    lambda.return_type = func_rtn_type;
    lambda.func_params = std::move(*params);
    lambda.func_body = std::move(body);

    checkpoint.commit();
    return lambda;
  };

  return parser;
}

auto parse_function_declaration() -> Parser<NDFuncDeclExpr> {
  static Parser<NDFuncDeclExpr> parser = [](ParserState& state) -> PResult<NDFuncDeclExpr> {
    ParseCheckpoint checkpoint(state);

    if (!match(TokenType::FuncStart)(state)) {
      return std::nullopt;
    }

    auto ident = expect_wp(
      state,
      parse_identifier(),
      ParseErrorType::InvalidFuncDeclExpr,
      "Identifier required after function declaration start"
    );

    if (!ident) {
      return std::nullopt;
    }

    auto left_paren = expect(
      state,
      TokenType::LParen,
      ParseErrorType::InvalidFuncDeclExpr,
      "`Missng open parenthesis `(`"
    );

    if (!left_paren) {
      return std::nullopt;
    }

    auto params = parse_parameters(state);
    if (!params) {
      return std::nullopt;
    }

    std::optional<NDTypeExpr> func_rtn_type;
    if (auto rtnop = match(TokenType::RtnTypeOp)(state)) {
      func_rtn_type = expect_wp(
        state,
        parse_type_expression(),
        ParseErrorType::InvalidFuncDeclExpr,
        "Function is missing the indicated return type"
      );
    }

    ScopeStackGuard stack_guard(state, ScopeStackType::Function);

    // Parse function body (at least one expression)
    std::vector<NDPtr> body{};
    while (true) {
      auto expr = parse_body_expression()(state);
      if (!expr) {
        state.skip_until(TokenType::EndStmt);
        break;
      };

      body.push_back(std::move(expr.value()));
      auto next = state.peek();
      if (!next || next->token_type == TokenType::EndStmt || next->token_type == TokenType::EoF) { break; }
    }

    auto end = match(TokenType::EndStmt)(state);

    if (!end) {
      Diagnostic diag;
      diag.level = DiagnosticLevel::Fail;
      auto location = state.peek().value_or(ident->identifier);
      diag.location.column = location.column_number;
      diag.location.line = location.line_number;
      diag.phase = DiagnosticPhase::Parser;
      diag.message = "Expected an end keyword after function declaration";
      state.diag_eng.report(diag);
      return std::nullopt;
    }

    NDFuncDeclExpr func;
    func.return_type = func_rtn_type;
    func.func_identifier = ident->identifier;
    func.func_params = std::move(*params);
    func.func_body = std::move(body);

    checkpoint.commit();
    return func;
  };
  return parser;
}
