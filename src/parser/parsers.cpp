#include "parsers.hpp"
#include "parser_err.hpp"
#include "parser_types.hpp"
#include "../tables/utils.hpp"
#include "parser_types.hpp"
#include <cstdio>
#include <memory>
#include <optional>
#include <utility>

PResult<Parent> run_parser(ParserState& state) {
  Parent parent;
  while(!state.is_at_end()) {
    PResult<NDPtr> ptr = run(parse_expression(), state);
    if (!ptr) { state.advance(); continue; }
    parent.children.push_back(std::move(ptr.value()));
  }

  return parent;
}

Parser<NDPtr> parse_expression() {
  return [=](ParserState& state) -> PResult<NDPtr> {

    if (auto import_directive = parse_import_stmt()(state)) {
      return std::make_unique<NDImportDirective>(import_directive.value());
    }

    if (auto let_expr = parse_let_expression()(state)) {
      return std::make_unique<NDLetBindExpr>(std::move(let_expr.value()));
    }

    if (auto const_expr = parse_const_expression()(state)) {
      return std::make_unique<NDConstExpr>(std::move(const_expr.value()));
    }

    if (auto func_decl = parse_function_declaration()(state)) {
      return std::make_unique<NDFuncDeclExpr>(std::move(func_decl.value()));
    }

    if (auto value_expr = parse_value_expression()(state)) {
      return value_expr;
    }

    return std::nullopt;
  };
}

Parser<NDPtr> parse_value_expression() {
  return [=](ParserState& state) -> PResult<NDPtr> {
    if (auto bin_expr = parse_binary_expression()(state)) {
      return bin_expr;
    }

    if (auto scoped_expr = parse_scoped_expression()(state)) {
      return std::make_unique<NDScopeExpr>(std::move(scoped_expr.value()));
    }

    if (auto case_expr = parse_case_expression()(state)) {
      return std::make_unique<NDCaseExpr>(std::move(case_expr.value()));
    }

    if (auto func_call_exprs = parse_call_exprs()(state)) {
      return func_call_exprs;
    }

    if (auto literal = parse_literal()(state)) {
      return std::make_unique<NDLiteral>(std::move(literal.value()));
    }

    if (auto ident = parse_identifier()(state)) {
      return std::make_unique<NDIdentifier>(std::move(ident.value()));
    }

    return std::nullopt;
  };
}

Parser<NDPtr> parse_primary_expression() {
  return [=](ParserState& state) -> PResult<NDPtr> {

    if (auto func_call = parse_call_expression()(state)) {
      return std::make_unique<NDCallExpr>(std::move(func_call.value()));
    }

    if (auto scoped_expr = parse_scoped_expression()(state)) {
      return std::make_unique<NDScopeExpr>(std::move(scoped_expr.value()));
    }

    if (auto literal = parse_literal()(state)) {
      return std::make_unique<NDLiteral>(std::move(literal.value()));
    }

    if (auto ident = parse_identifier()(state)) {
      return std::make_unique<NDIdentifier>(std::move(ident.value()));
    }

    return std::nullopt;
  };
}

Parser<NDImportDirective> parse_import_stmt() {
  return [=](ParserState& state) -> PResult<NDImportDirective> {
    ParseCheckpoint checkpoint(state);

    auto import_kwd = match(TokenType::ImportKeyword)(state);
    if (!import_kwd) return std::nullopt;

    auto import_module = expect(
      state,
      TokenType::ImportModule,
      ParseErrorType::InvalidImportExpr,
      "Expected a valid module name after import directive"
    );

    if (!import_module) return std::nullopt;

    auto import = NDImportDirective();
    import.import_directive = import_module.value();


    checkpoint.commit();
    return import;
  };
}

Parser<NDPtr> m_parse_unary_expression() {
  return [=](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);
    auto op = optional(choice<Token>({
      match(TokenType::MinusOp),
      match(TokenType::NotOp)
    }), state);

    auto expression = parse_primary_expression()(state);
    if (!expression) return std::nullopt;

    if (op) {
      NDUnaryExpr expr;
      expr.op = op;
      expr.rhs = std::move(expression.value());
      checkpoint.commit();
      return std::make_unique<NDUnaryExpr>(std::move(expr));
    }

    checkpoint.commit();
    return expression;
  };
}

Parser<NDPtr> m_parse_chain_left(Parser<NDPtr> term, Parser<Token> op) {
  return [&, term, op](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);

    auto left_res = term(state);

    if (!left_res) return std::nullopt;

    NDPtr left = std::move(left_res.value());

    while(!state.is_at_end()) {
      size_t start = state.pos;

      auto op_res = op(state);
      if (!op_res) break;

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

Parser<NDPtr> m_parse_multiplicative_op() {
  return m_parse_chain_left(
    m_parse_unary_expression(),
    choice<Token>({
      match(TokenType::MultiplyOp),
      match(TokenType::DivideOp)
    })
  );
}

Parser<NDPtr> m_parse_additive_op() {
  return m_parse_chain_left(
    m_parse_multiplicative_op(),
    choice<Token>({
      match(TokenType::PlusOp), 
      match(TokenType::MinusOp)
    })
  );
}

Parser<NDPtr> m_parse_comparision_op() {
  return m_parse_chain_left(
    m_parse_additive_op(),
    choice<Token>({
      match(TokenType::Lt),
      match(TokenType::Le),
      match(TokenType::Gt),
      match(TokenType::Ge)
    })
  );
}

Parser<NDPtr> m_parse_equality() {
  return m_parse_chain_left(
    m_parse_comparision_op(),
    choice<Token>({
      match(TokenType::EqEq),
      match(TokenType::NtEq)
    })
  );
}

Parser<NDPtr> m_parse_logical_and() {
  return m_parse_chain_left(
    m_parse_equality(), 
    match(TokenType::AndOp)
  );
}

Parser<NDPtr> m_parse_logical_or() {
  return m_parse_chain_left(
    m_parse_logical_and(), 
    match(TokenType::OrOp)
  );
}

Parser<NDPtr> parse_binary_expression() {
  return [=](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);
    auto bin_expr = m_parse_logical_or()(state);

    if (!bin_expr) return std::nullopt;

    checkpoint.commit();
    return bin_expr;
  };
}

Parser<NDCallExpr> parse_call_expression() {
  return [=](ParserState& state) -> PResult<NDCallExpr> {
    ParseCheckpoint checkpoint(state);

    auto ident = parse_identifier()(state);
    if (!ident) return std::nullopt;

    auto open_paren = match(TokenType::LParen)(state);
    if (!open_paren) return std::nullopt;

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

      if (!expr) return std::nullopt;

      args.push_back(std::move(expr.value()));

      if (!match(TokenType::Delim)(state)) {
        auto close_paren = expect(
          state,
          TokenType::RParen,
          ParseErrorType::InvalidFuncCallExpr,
          "missing closing parenthsis `)`"
        );

        if (!close_paren) return std::nullopt;
        break;
      }
    }

    NDCallExpr call;
    call.identifier = std::make_unique<NDIdentifier>(ident.value());
    call.args = std::move(args);

    checkpoint.commit();
    return call;
  };
}

Parser<NDPtr> parse_call_exprs() {
  return [=](ParserState& state) -> PResult<NDPtr> {
    ParseCheckpoint checkpoint(state);
    auto func = parse_call_expression()(state);

    if (!func) return std::nullopt;

    if (auto p = state.peek(); p->token_type != TokenType::PipeOp) {
      auto ptr = std::make_unique<NDCallExpr>(std::move(func.value()));

      checkpoint.commit();
      return ptr;
    }

    auto pipe_chain = NDCallChain();
    auto ptr = std::make_unique<NDCallExpr>(std::move(func.value()));
    pipe_chain.calls.push_back(std::move(ptr));
    while(!state.is_at_end()) {
      auto pipe = match(TokenType::PipeOp)(state);
      if (!pipe) break;
      size_t chain_start = state.pos;
      auto chain_func = parse_call_expression()(state);
      if(!chain_func) {
        state.reset_pos(chain_start);
        break;
      }
      auto chain_ptr = std::make_unique<NDCallExpr>(std::move(chain_func.value()));
      pipe_chain.calls.push_back(std::move(chain_ptr));
    }

    auto pipe_chain_ptr = std::make_unique<NDCallChain>(std::move(pipe_chain));

    checkpoint.commit();
    return pipe_chain_ptr;
  };
}

Parser<NDFuncDeclExpr> parse_function_declaration() {
  return [=](ParserState& state) -> PResult<NDFuncDeclExpr> {
    ParseCheckpoint checkpoint(state);

    if (!match(TokenType::FuncStart)(state)) return std::nullopt;

    auto ident = expect_wp(
      state,
      parse_identifier(),
      ParseErrorType::InvalidFuncDeclExpr,
      "Identifier required after function declaration start"
    );

    if (!ident) return std::nullopt;

    auto left_paren = expect(
      state,
      TokenType::LParen,
      ParseErrorType::InvalidFuncDeclExpr,
      "`Missng open parenthesis `(`"
    );

    if (!left_paren) return std::nullopt;
    std::vector<FuncParam> params;

    while (true) {
      if (match(TokenType::RParen)(state)) break;

      auto param_token = expect(
        state,
        TokenType::Identifier,
        ParseErrorType::InvalidFuncDeclExpr,
        "Function args require valid identifiers"
      );

      if (!param_token) return std::nullopt;

      FuncParam param;
      auto param_type = optional(parse_type_annotation(), state);
      param.param_token = param_token.value();
      param.param_type = param_type;
      params.push_back(param);

      if (!match(TokenType::Delim)(state)) {
        auto right_paren = expect(
          state,
          TokenType::RParen,
          ParseErrorType::InvalidFuncDeclExpr,
          "Missing closing parenthesis `)`"
        );
        if (!right_paren) return std::nullopt;
        break;
      }
    }

    std::optional<Token> func_rtn_type;
    if (auto a = match(TokenType::RtnTypeOp)(state)) {
      func_rtn_type = expect(
        state,
        TokenType::Identifier,
        ParseErrorType::InvalidFuncDeclExpr,
        "Function is missing the indicated return type"
      );
    }

    // Parse function body (at least one expression)
    std::vector<NDPtr> body{};
    while (!match(TokenType::EndStmt)(state)) {
      auto expr = parse_expression()(state);
      if (!expr) { 
        state.skip_until(TokenType::EndStmt);
        break;
      };
      body.push_back(std::move(expr.value()));
    }

    NDFuncDeclExpr func;
    func.type = func_rtn_type;
    func.return_type = func_rtn_type;
    func.func_identifier = ident->identifier;
    func.func_params = std::move(params);
    func.func_body = std::move(body);

    checkpoint.commit();
    return func;
  };
}

Parser<NDCaseExpr> parse_case_expression() {
  return [=](ParserState& state) -> PResult<NDCaseExpr> {
    ParseCheckpoint checkpoint(state);

    if (!match(TokenType::Case)(state)) return std::nullopt;

    // The main condition to evaluate
    std::vector<NDPtr> conditions;
    auto main_expr = expect_wp(
      state,
      parse_value_expression(),
      ParseErrorType::InvalidCaseExpr,
      "Expected a value expression here"
    );

    if (!main_expr) return std::nullopt;
    conditions.push_back(std::move(main_expr.value()));

    auto colon = expect(
      state,
      TokenType::Colon,
      ParseErrorType::InvalidCaseExpr,
      "Expected `:` after case precondition"
    );

    if(!colon) return std::nullopt;

    std::vector<NDCaseExpr::Branch> branches;
    while (!match(TokenType::EndStmt)(state)) {
      auto pattern = parse_value_expression()(state);

      // This will change later.
      if (!pattern) return std::nullopt;

      auto rtn_op = expect(
        state,
        TokenType::RtnTypeOp,
        ParseErrorType::InvalidCaseExpr,
        "Expected `:>` after case condition"
      );

      if (!rtn_op) return std::nullopt;

      auto result = expect_wp(
        state,
        parse_value_expression(),
        ParseErrorType::InvalidCaseExpr,
        "Expected a valid value expression"
      );

      if (!result) return std::nullopt;

      auto branch = std::vector<NDPtr>();
      branch.push_back(std::move(pattern.value()));

      branches.push_back(NDCaseExpr::Branch{
        .pattern = std::move(branch),
        .result = std::move(result.value())
      });
    }

    NDCaseExpr expr;
    expr.conditions = std::move(conditions);
    expr.branches = std::move(branches);

    checkpoint.commit();
    return expr;
  };
}

Parser<NDScopeExpr> parse_scoped_expression() {
  return [=](ParserState& state) -> PResult<NDScopeExpr>{
    ParseCheckpoint checkpoint(state);

    if (!match(TokenType::LBrace)(state)) {
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
    scope_expr.expressions = std::move(exprs);


    checkpoint.commit();
    return scope_expr;
  };
}

Parser<NDLetBindExpr> parse_let_expression() {
  return [=](ParserState& state) -> PResult<NDLetBindExpr> {
    ParseCheckpoint checkpoint(state);

    auto let_tok = match(TokenType::LetKeyword)(state);
    if (!let_tok) return std::nullopt;

    auto ident = expect_wp(
      state,
      parse_identifier(),
      ParseErrorType::InvalidLetExpr,
      "Expected an identifier here"
    );

    if (!ident) return std::nullopt;
    auto let_type = optional(parse_type_annotation(), state);

    auto eq = expect(
      state,
      TokenType::Eq,
      ParseErrorType::InvalidLetExpr,
      "Expected `=` after identifier"
    );

    if (!eq) return std::nullopt;

    auto value = expect_wp(
      state,
      parse_value_expression(),
      ParseErrorType::InvalidLetExpr,
      "Expected a valid expression"
    );

    if (!value) return std::nullopt;

    NDLetBindExpr expr;
    expr.identifier = std::make_unique<NDIdentifier>(ident.value());
    expr.identifier->type = let_type;
    expr.type = let_type;
    expr.bound_value = std::move(value.value());

    checkpoint.commit();
    return expr;
  };
}

Parser<NDConstExpr> parse_const_expression() {
  return [=](ParserState& state) -> PResult<NDConstExpr> {
    ParseCheckpoint checkpoint(state);

    auto let_tok = match(TokenType::ConstantKeyword)(state);
    if (!let_tok) return std::nullopt;

    auto ident = expect_wp(
      state,
      parse_identifier(),
      ParseErrorType::InvalidConstExpr,
      "Expected a valid const identifier"
    );

    if (!ident) return std::nullopt;

    auto const_type = optional(parse_type_annotation(), state);

    auto eq = expect(
      state,
      TokenType::Eq,
      ParseErrorType::InvalidLetExpr,
      "Expected `=` after identifier"
    );

    if (!eq) return std::nullopt;

    auto literal = expect_wp(
      state,
      parse_literal(),
      ParseErrorType::InvalidConstExpr,
      "Const values can only hold `literal` types"
    );

    if (!literal) return std::nullopt;

    NDConstExpr expr;
    expr.identifier = std::make_unique<NDIdentifier>(ident.value());
    expr.identifier->type = const_type;
    expr.type = const_type;
    expr.literal = std::move(literal.value());


    checkpoint.commit();
    return expr;
  };
}

Parser<NDIdentifier> parse_identifier() {
  return [=](ParserState& state) -> PResult<NDIdentifier> {
    auto token = match(TokenType::Identifier)(state);
    if (!token) return std::nullopt;

    NDIdentifier id;
    id.identifier = token.value();
    return id;
  };
}

Parser<NDLiteral> parse_literal() {
  return [=](ParserState& state) -> PResult<NDLiteral> {
    auto token = state.peek();
    if (!token) return std::nullopt;
    if (!is_literal(token.value())) return std::nullopt;
    auto literal = NDLiteral();
    literal.literal = token.value();
    state.advance();
    return literal;
  };
}

Parser<Token> match(TokenType type) {
  return [=](ParserState& state) -> PResult<Token> {
    auto t = typeToStr(type);
    auto tok = state.peek();
    if (!tok) return std::nullopt;
    if (tok->token_type == type) {
      state.advance();
      return tok;
    }

    return std::nullopt;
  };
}

Parser<Token> parse_type_annotation() {
  return [=](ParserState& state) -> PResult<Token> {
    size_t start = state.pos;
    auto c = match(TokenType::Colon)(state);
    if (!c) return std::nullopt;
    auto type = expect(
      state,
      TokenType::Identifier,
      ParseErrorType::InvalidTypeAnnotation,
      "Missing indicated type"
    );
    if (!type) {
      state.reset_pos(start);
      return std::nullopt;
    }
    return type.value();
  };
}
