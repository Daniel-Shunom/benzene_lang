#include "parsers.hpp"
#include "../../tables/utils.hpp"
#include "parser_types.hpp"
#include <optional>

PResult<NDPtr> run_parser(ParserState& state) {
  return run(parse_expression(), state);
}

Parser<NDCallExpr> parse_call_expression() {
  return [=](ParserState& state) -> PResult<NDCallExpr> {
    size_t start = state.pos;

    auto ident = parse_identifier()(state);
    if (!ident) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto open_paren = match(TokenType::LParen)(state);
    if (!open_paren) {
      state.reset_pos(start);
      return std::nullopt;
    }

    std::vector<NDPtr> args;
    while (true) {
      if (auto close_paren = match(TokenType::RParen)(state)) {
        break;
      }

      auto expr = parse_expression()(state);
      if (!expr) {
        state.reset_pos(start);
        return std::nullopt;
      }

      args.push_back(std::move(*expr));

      // Consume comma if present, otherwise expect CloseParen next iteration
      if (!match(TokenType::Delim)(state)) {
        auto close_paren = match(TokenType::RParen)(state);
        if (!close_paren) {
          state.reset_pos(start);
          return std::nullopt;
        }
        break;
      }
    }

    NDCallExpr call;
    call.identifier = std::make_unique<NDIdentifier>(*ident);
    call.args = std::move(args);

    return call;
  };
}

Parser<NDFuncDeclExpr> parse_function_declaration() {
  return [=](ParserState& state) -> PResult<NDFuncDeclExpr> {
    size_t start = state.pos;

    if (!match(TokenType::FuncStart)(state)) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto ident = parse_identifier()(state);
    if (!ident) {
      state.reset_pos(start);
      return std::nullopt;
    }

    if (!match(TokenType::LParen)(state)) {
      state.reset_pos(start);
      return std::nullopt;
    }

    std::vector<FuncParam> params;
    while (true) {
      if (match(TokenType::RParen)(state)) break;

      auto param_token = parse_identifier()(state);
      if (!param_token) {
        state.reset_pos(start);
        return std::nullopt;
      }

      // TODO: optional typed params
      FuncParam param;
      param.param_name = param_token->identifier.token_value;
      param.param_type = Type{};
      params.push_back(std::move(param));

      if (!match(TokenType::Delim)(state)) {
        if (!match(TokenType::RParen)(state)) {
          state.reset_pos(start);
          return std::nullopt;
        }
        break;
      }
    }

    // Parse function body (at least one expression)
    std::vector<NDPtr> body;
    while (!match(TokenType::EndStmt)(state)) {
      auto expr = parse_expression()(state);
      if (!expr) {
        state.reset_pos(start);
        return std::nullopt;
      }
      body.push_back(std::move(*expr));
    }

    NDFuncDeclExpr func;
    func.func_identifier = ident->identifier;
    func.func_params = std::move(params);
    func.func_body = std::move(body);

    return func;
  };
}

Parser<NDCaseExpr> parse_case_expression() {
  return [=](ParserState& state) -> PResult<NDCaseExpr> {
    size_t start = state.pos;

    if (!match(TokenType::Case)(state)) {
      state.reset_pos(start);
      return std::nullopt;
    }

    // The main condition to evaluate
    std::vector<NDPtr> conditions;
    auto main_expr = parse_expression()(state);
    if (!main_expr) {
      state.reset_pos(start);
      return std::nullopt;
    }
    conditions.push_back(std::move(*main_expr));

    std::vector<NDCaseExpr::Branch> branches;
    while (!match(TokenType::EndStmt)(state)) {
      auto pattern = parse_expression()(state);
      if (!pattern) {
        state.reset_pos(start);
        return std::nullopt;
      }

      if (!match(TokenType::RtnTypeOp)(state)) {
        state.reset_pos(start);
        return std::nullopt;
      }

      auto result = parse_expression()(state);
      if (!result) {
        state.reset_pos(start);
        return std::nullopt;
      }

      auto branch = std::vector<NDPtr>();
      branch.push_back(std::move(*pattern));

      branches.push_back(NDCaseExpr::Branch{
        .pattern = std::move(branch),
        .result = std::move(*result)
      });
    }

    NDCaseExpr expr;
    expr.conditions = std::move(conditions);
    expr.branches = std::move(branches);

    return expr;
  };
}

Parser<NDPtr> parse_expression() {
  return [=](ParserState& state) -> PResult<NDPtr> {
    // Try each expression parser in priority order
    if (auto let_expr = parse_let_expression()(state)) {
      return std::make_unique<NDLetBindExpr>(std::move(*let_expr));
    }

    if (auto const_expr = parse_const_expression()(state)) {
      return std::make_unique<NDConstExpr>(std::move(*const_expr));
    }

    if (auto func_call = parse_call_expression()(state)) {
      return std::make_unique<NDCallExpr>(std::move(*func_call));
    }

    if (auto func_decl = parse_function_declaration()(state)) {
      return std::make_unique<NDFuncDeclExpr>(std::move(*func_decl));
    }

    if (auto case_expr = parse_case_expression()(state)) {
      return std::make_unique<NDCaseExpr>(std::move(*case_expr));
    }

    if (auto literal = parse_literal()(state)) {
      return std::make_unique<NDLiteral>(std::move(*literal));
    }

    if (auto ident = parse_identifier()(state)) {
      return std::make_unique<NDIdentifier>(std::move(*ident));
    }

    return std::nullopt;
  };
}

Parser<NDLetBindExpr> parse_let_expression() {
  return [=](ParserState& state) -> PResult<NDLetBindExpr> {
    size_t start = state.pos;

    auto let_tok = match(TokenType::LetKeyword)(state);
    if (!let_tok) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto ident = parse_identifier()(state);
    if (!ident) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto eq = match(TokenType::Eq)(state);
    if (!eq) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto value = parse_expression()(state);
    if (!value) {
      state.reset_pos(start);
      return std::nullopt;
    }

    NDLetBindExpr expr;
    expr.identifier = std::make_unique<NDIdentifier>(*ident);
    expr.bound_value = std::move(*value);

    return expr;
  };
}

Parser<NDConstExpr> parse_const_expression() {
  return [=](ParserState& state) -> PResult<NDConstExpr> {
    size_t start = state.pos;

    auto let_tok = match(TokenType::ConstantKeyword)(state);
    if (!let_tok) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto ident = parse_identifier()(state);
    if (!ident) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto eq = match(TokenType::Eq)(state);
    if (!eq) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto literal = parse_literal()(state);
    if (!literal) {
      state.reset_pos(start);
      return std::nullopt;
    }

    NDConstExpr expr;
    expr.identifier = std::make_unique<NDIdentifier>(*ident);
    expr.literal = std::move(literal.value());

    return expr;
  };
}

Parser<NDIdentifier> parse_identifier() {
  return [=](ParserState& state) -> PResult<NDIdentifier> {
    auto token = state.peek();
    if(!token) return std::nullopt;
    if(token->token_type != TokenType::Identifier) return std::nullopt;
    Symbol* sym = state.sym_table.lookup(token->token_value);
    if (sym == nullptr) {
       sym = state.sym_table.declare(token.value(), SymbolKind::UnResolved);
    }
    auto identifier = NDIdentifier();
    identifier.identifier = token.value();
    identifier.identifier_symbol = sym;
    state.advance();
    return identifier;
  };
}

Parser<NDLiteral> parse_literal() {
  return [=](ParserState& state) -> PResult<NDLiteral> {
    auto token = state.peek();
    if (!token) return std::nullopt;
    if (!is_literal(token.value())) return std::nullopt;
    state.advance();
    auto literal = NDLiteral();
    literal.literal = token.value();
    return literal;
  };
}

Parser<Token> match(TokenType type) {
  return [=](ParserState& state) -> PResult<Token> {
    if (auto tok = state.peek(); tok && tok->token_type == type) {
      state.advance();
      return tok;
    }

    return std::nullopt;
  };
}
