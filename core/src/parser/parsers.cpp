#include <ether/parser/parsers.hpp>

using std::nullopt;

static auto is_top_level_sync_token(TokenType tokt) -> bool {
  return tokt == TokenType::ImportKeyword
      || tokt == TokenType::TypeKeyword
      || tokt == TokenType::LetKeyword
      || tokt == TokenType::ConstantKeyword
      || tokt == TokenType::FuncStart
      || tokt == TokenType::Case;
}

auto run_parser(ParserState& state) -> PResult<Parent> {
  Parent parent;
  while(!state.is_at_end()) {
    size_t before = state.pos;
    PResult<NDPtr> ptr = run(parse_expression(), state);
    if (ptr) {
      parent.children.push_back(std::move(ptr.value()));
      continue;
    }

    // Recovery: advance at least one token, then skip to the next plausible
    // top-level boundary so a single bad expression doesn't cascade.
    if (state.pos == before) {
      state.advance();
    }

    while (!state.is_at_end()) {
      auto tok = state.peek();
      if (!tok || is_top_level_sync_token(tok->token_type)) {
        break;
      }
      state.advance();
    }
  }

  return parent;
}


// Benzene has no assignment. A binding is introduced with `let` inside a body,
// or `const` at module scope, and is never rebound.
//
// Without this, `a = 5` parses as the bare expression `a`, the `=` is left
// over, and the next round of body parsing fails on it -- taking the rest of
// the function down silently. Recognising the shape here turns that into one
// diagnostic that names the missing keyword.
//
// An annotation is picked up too, because `a: Count = 5` is the same mistake
// wearing a type: annotations belong to declarations and parameters, never to
// an expression.
static auto parse_assignment_without_binder() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {
    size_t start = state.pos;

    auto name = match(TokenType::Identifier)(state);
    if (!name) {
      state.reset_pos(start);
      return std::nullopt;
    }

    bool annotated = false;
    if (match(TokenType::Colon)(state)) {
      annotated = true;
      // The annotation's type, if one was written. A missing one is still this
      // same mistake, so it is not required here.
      match(TokenType::Identifier)(state);
    }

    if (!match(TokenType::Eq)(state)) {
      state.reset_pos(start);
      return std::nullopt;
    }

    Diagnostic diag;
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Parser;
    diag.location.line = name->line_number;
    diag.location.column = name->column_number;
    diag.message = "`" + name->token_value
      + "` is assigned without a binder. Benzene has no assignment: write `let "
      + name->token_value + (annotated ? ": <type>" : "")
      + " = ...` to introduce a binding, or `const` at module scope.";
    state.diag_eng.report(diag);

    // Consume the value that was being assigned, so the statements after this
    // one still parse and still resolve. Returning it keeps the body
    // well-formed; the diagnostic above is what reports the mistake.
    if (auto value = parse_value_expression()(state)) {
      return value;
    }

    state.reset_pos(start);
    return std::nullopt;
  };
  return parser;
}

auto parse_expression() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {

    if (auto import_directive = parse_import_stmt()(state)) {
      return std::make_unique<NDImportDirective>(std::move(import_directive.value()));
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

    if (auto type_decl = parse_type_declaration()(state)) {
      return std::make_unique<NDTypeDecl>(std::move(type_decl.value()));
    }

    if (auto assigned = parse_assignment_without_binder()(state)) {
      return assigned;
    }

    if (auto value_expr = parse_value_expression()(state)) {
      return value_expr;
    }

    return std::nullopt;
  };
  return parser;
}

auto parse_body_expression() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {
    if (auto const_expr = parse_const_expression()(state)) {
      return std::make_unique<NDConstExpr>(std::move(const_expr.value()));
    }

    if (auto let_expr = parse_let_expression()(state)) {
      return std::make_unique<NDLetBindExpr>(std::move(let_expr.value()));
    }

    if (auto assigned = parse_assignment_without_binder()(state)) {
      return assigned;
    }

    if (auto chain = parse_call_exprs()(state)) {
      return chain;
    }

    if (auto bin_expr = parse_binary_expression()(state)) {
      return bin_expr;
    }

    if (auto case_expr = parse_case_expression()(state)) {
      return std::make_unique<NDCaseExpr>(std::move(case_expr.value()));
    }

    if (auto lambda = parse_lambda_expression()(state)) {
      return std::make_unique<NDLambdaExpr>(std::move(lambda.value()));
    }

    return parse_primary_expression()(state);
  };
  return parser;
}

auto parse_value_expression() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {
    // Pipe chains must be tried first: a chain begins with a call expression,
    // which `parse_binary_expression` would otherwise consume as a primary,
    // leaving the trailing `|=>` orphaned.
    if (auto chain = parse_call_exprs()(state)) {
      return chain;
    }

    if (auto bin_expr = parse_binary_expression()(state)) {
      return bin_expr;
    }

    if (auto case_expr = parse_case_expression()(state)) {
      return std::make_unique<NDCaseExpr>(std::move(case_expr.value()));
    }

    if (auto lambda = parse_lambda_expression()(state)) {
      return std::make_unique<NDLambdaExpr>(std::move(lambda.value()));
    }

    return std::nullopt;
  };
  return parser;
}

auto parse_primary_expression() -> Parser<NDPtr> {
  static Parser<NDPtr> parser = [](ParserState& state) -> PResult<NDPtr> {

    if (auto func_call = parse_call_expression()(state)) {
      return std::make_unique<NDCallExpr>(std::move(func_call.value()));
    }

    if (auto scoped_expr = parse_scoped_expression()(state)) {
      return std::make_unique<NDScopeExpr>(std::move(scoped_expr.value()));
    }

    if (auto list_expr = parse_list_expression()(state)) {
      return std::make_unique<NDListExpr>(std::move(list_expr.value()));
    }

    if (auto tuple_expr = parse_tuple_expression()(state)) {
      return std::make_unique<NDTupleExpr>(std::move(tuple_expr.value()));
    }

    if (auto literal = parse_literal()(state)) {
      return std::make_unique<NDLiteral>(std::move(literal.value()));
    }

    if (auto ident = parse_identifier()(state)) {
      return std::make_unique<NDIdentifier>(std::move(ident.value()));
    }

    return std::nullopt;
  };
  return parser;
}
