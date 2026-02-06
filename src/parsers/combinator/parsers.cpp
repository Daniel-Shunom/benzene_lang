#include "parsers.hpp"
#include "../../tables/utils.hpp"
#include "parser_types.hpp"
#include <memory>
#include <optional>

PResult<Parent> run_parser(ParserState& state) {
  Parent parent;

  while(!state.is_at_end()) {
    PResult<NDPtr> ptr = run(parse_expression(), state);
    if (!ptr) { state.advance(); continue; }
    parent.children.push_back(std::move(ptr.value()));
  }

  return parent;
}

Parser<NDImportDirective> parse_import_stmt() {
  return [=](ParserState& state) -> PResult<NDImportDirective> {
    size_t start = state.pos;
    auto import_kwd = match(TokenType::ImportKeyword)(state);
    if (!import_kwd) return std::nullopt;
    auto import_module = match(TokenType::ImportModule)(state);
    if (!import_module) {
      state.reset_pos(start);
      return std::nullopt;
    }
    auto import = NDImportDirective();
    import.import_directive = import_module.value();

    state.log_success("Sucessfully parsed `ImportDirective`");
    return import;
  };
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

    state.log_success("Sucessfully parsed `FunctionCall`");
    return call;
  };
}

Parser<NDPtr> parse_call_exprs() {
  return [=](ParserState& state) -> PResult<NDPtr> {
    size_t start_pos = state.pos;
    auto func = parse_call_expression()(state);
    if (!func) {
      state.reset_pos(start_pos);
      return std::nullopt;
    }

    if (auto p = state.peek(); p->token_type != TokenType::PipeOp) {
      auto ptr = std::make_unique<NDCallExpr>(std::move(func.value()));
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
    }
    auto pipe_chain_ptr = std::make_unique<NDCallChain>(std::move(pipe_chain));
    state.log_success("Sucessfully parsed `ChainExpression`");
    state.log_captured_expr("`CallChain`");
    return pipe_chain_ptr;
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

    state.log_success("Sucessfully parsed `FunctionDeclaration`");
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

    state.log_success("Sucessfully parsed CaseExpression");
    return expr;
  };
}

Parser<NDPtr> parse_expression() {
  return [=](ParserState& state) -> PResult<NDPtr> {
    state.sym_table.new_scope(ScopeType::Function);
    // Try each expression parser in priority order
    // Okay so obviously this is super inefficient. 
    // A jump table (switch statment) is way better for this
    // So we will use that later, but for now, I just want the
    // damn thing to work :)
    if (auto import_directive = parse_import_stmt()(state)) {
      state.log_captured_expr("`ImportDirective`");
      return std::make_unique<NDImportDirective>(*import_directive);
    }

    if (auto let_expr = parse_let_expression()(state)) {
      state.sym_table.pop_scope();
      state.log_captured_expr("`LetExpression`");
      return std::make_unique<NDLetBindExpr>(std::move(*let_expr));
    }

    if (auto const_expr = parse_const_expression()(state)) {
      state.sym_table.pop_scope();
      state.log_captured_expr("`ConstExpression`");
      return std::make_unique<NDConstExpr>(std::move(*const_expr));
    }

    if (auto func_call_exprs = parse_call_exprs()(state)) {
      state.sym_table.pop_scope();
      state.log_captured_expr("`FuncCallExpression`");
      return func_call_exprs;
    }

    if (auto func_decl = parse_function_declaration()(state)) {
      state.sym_table.pop_scope();
      state.log_captured_expr("`FuncDeclExpression`");
      return std::make_unique<NDFuncDeclExpr>(std::move(*func_decl));
    }

    if (auto case_expr = parse_case_expression()(state)) {
      state.sym_table.pop_scope();
      state.log_captured_expr("`CaseExpression`");
      return std::make_unique<NDCaseExpr>(std::move(*case_expr));
    }

    if (auto literal = parse_literal()(state)) {
      state.sym_table.pop_scope();
      state.log_captured_expr("`LiteralExpression`");
      return std::make_unique<NDLiteral>(std::move(*literal));
    }

    if (auto ident = parse_identifier()(state)) {
      state.sym_table.pop_scope();
      state.log_captured_expr("`IdentifierExpr`");
      return std::make_unique<NDIdentifier>(std::move(*ident));
    }

    state.sym_table.pop_scope();
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
    state.log_success("Sucessfully parsed `LetExpression`");
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

    state.log_success("Sucessfully parsed `ConstExpression`");
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
