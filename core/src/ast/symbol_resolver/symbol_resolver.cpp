#include <ether/ast/symbol_resolver/symbol_resolver.hpp>

void SymbolResolver::visit(NDImportDirective& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  // todo: source files from include
  if ( cscope_type && (cscope_type != ScopeType::Module)) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Warn;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.import_directive.column_number;
    diag.location.line = expr.import_directive.line_number;
    diag.message = "Import statements are only allowed in the top Module scope";

    this->diag_eng.report(diag);
    return;
  }
}

void SymbolResolver::visit(NDLiteral& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Warn;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.literal.column_number;
    diag.location.line = expr.literal.line_number;
    diag.message = std::format(
      "Literal value `{}` not in allowed scope",
      expr.literal.token_value
    );

    this->diag_eng.report(diag);
    return;
  }
}

void SymbolResolver::visit(NDIdentifier& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier.column_number;
    diag.location.line = expr.identifier.line_number;
    diag.message = std::format(
      "Identifier `{}` not in allowed scope",
      expr.identifier.token_value
    );

    this->diag_eng.report(diag);
    return;
  }

  auto ident_sym = this->sym_table.lookup(expr.identifier.token_value);
  if (!ident_sym) return;
  expr.identifier_symbol = ident_sym;
}

void SymbolResolver::visit(NDLetBindExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::ScopedExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = "`Let` expression is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  auto expr_sym = this->sym_table.declare(expr.identifier->identifier, SymbolKind::Binding);
  if (!expr_sym) {
    expr.is_poisoned = true;
    auto ident_sym = this->sym_table.lookup(expr.identifier->identifier.token_value);
    if (!ident_sym) return;

    auto dup_msg = std::format(
      "Duplicate declaration of `{}` (see Ln {}, Col {} for previous declaration)",
      expr.identifier->identifier.token_value,
      ident_sym->symbol_token.line_number,
      ident_sym->symbol_token.column_number
    );
    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = dup_msg;

    this->diag_eng.report(diag);
    return;
  }

  expr_sym->symbol_kind = SymbolKind::Binding;
  expr.identifier->identifier_symbol = expr_sym;

  BindingData binding_data;

  if (expr.type) {
    binding_data.binding_type.type_name = expr.type->token_value;
  }

  expr.bound_value->accept(*this);
  return;
}

void SymbolResolver::visit(NDConstExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();

  if (
    cscope_type
    && cscope_type != ScopeType::Module
    && cscope_type != ScopeType::Application
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = "`Const` expression is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  auto const_sym = this->sym_table.declare(expr.identifier->identifier, SymbolKind::Constant);
  if (!const_sym) {
    expr.is_poisoned = true;
    auto ident_sym = this->sym_table.lookup(expr.identifier->identifier.token_value);
    if (!ident_sym) return;

    auto dup_msg = std::format(
      "Duplicate `const` declaration of `{}` (see Ln {}, Col {} for previous declaration)",
      expr.identifier->identifier.token_value,
      ident_sym->symbol_token.line_number,
      ident_sym->symbol_token.column_number
    );

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = dup_msg;

    this->diag_eng.report(diag);
    return;
  }
  const_sym->symbol_kind = SymbolKind::Constant;
  expr.identifier->identifier_symbol = const_sym;

  if (cscope_type == ScopeType::Module) {
    this->exports.emplace(const_sym->name, const_sym);
  }

  expr.literal.accept(*this);
  return;
}


void SymbolResolver::visit(NDCallExpr& expr) {
  auto ident  = expr.identifier->identifier.token_value;
  auto sym = this->sym_table.lookup(ident);

  if (!sym) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = "`Call` expression is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::CaseExpression
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = "Function call is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  expr.identifier->identifier_symbol = sym;

  for (auto& arg: expr.args) {
    arg->accept(*this);
  }

  return;
}

void SymbolResolver::visit(NDCallChain& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::CaseExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.line = expr.start_token.line_number;
    diag.location.column = expr.start_token.column_number;
    diag.message = "Call chain not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  for (auto& call: expr.calls) call->accept(*this);
}

void SymbolResolver::visit(NDFuncDeclExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.func_identifier.column_number;
    diag.location.line = expr.func_identifier.line_number;
    diag.message = "Function declaration not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  auto func_sym = this->sym_table.declare(expr.func_identifier, SymbolKind::Function);
  if (!func_sym) {
    // Means this is a redeclaration of another function.
    expr.is_poisoned = true;
    auto ident_sym = this->sym_table.lookup(expr.func_identifier.token_value);
    if (!ident_sym) return;

    auto dup_msg = std::format(
      "Duplicate function declaration of `{}` (see Ln {}, Col {} for previous declaration)",
      expr.func_identifier.token_value,
      ident_sym->symbol_token.line_number,
      ident_sym->symbol_token.column_number
    );

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.func_identifier.column_number;
    diag.location.line = expr.func_identifier.line_number;
    diag.message = dup_msg;

    this->diag_eng.report(diag);
    return;
  }

  if (cscope_type == ScopeType::Module) {
    this->exports.emplace(func_sym->name, func_sym);
  }

  ScopeGuard guard(this->sym_table, ScopeType::FunctionExpression);

  for (auto& arg: expr.func_params) {
    auto ptr = sym_table.declare(arg.param_token, SymbolKind::FuncParam);
    if (!ptr) {
      auto lkp = sym_table.lookup(arg.param_token.token_value);
      if (!lkp) continue;
      auto dup_msg = std::format(
        "Duplicate function parameter name `{}` (see Ln {}, Col {} for previous declaration)",
        arg.param_token.token_value,
        lkp->symbol_token.line_number,
        lkp->symbol_token.column_number
      );

      auto diag = Diagnostic();
      diag.level = DiagnosticLevel::Fail;
      diag.phase = DiagnosticPhase::Resolver;
      diag.location.column = arg.param_token.column_number;
      diag.location.line = arg.param_token.line_number;
      diag.message = dup_msg;

      this->diag_eng.report(diag);
    };

    if (!arg.param_sym) arg.param_sym = ptr;
  }

  for (auto& body_expr: expr.func_body) body_expr->accept(*this);

  FunctionData func_data;

  if (expr.return_type) {
    func_data.function_return_type.type_name = expr.return_type->token_value;
  }

  func_sym->symbol_data = func_data;
  expr.func_sym = func_sym;
  return;
}

void SymbolResolver::visit(NDScopeExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::ScopedExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.line = expr.open_brace.line_number;
    diag.location.column = expr.open_brace.column_number;
    diag.message = "Scoped expression is not allowed in current scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::ScopedExpression);
  for (auto& scope_expr: expr.expressions) scope_expr->accept(*this);
}

void SymbolResolver::visit(NDCaseExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::ScopedExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.line = expr.case_keyword.line_number;
    diag.location.column = expr.case_keyword.column_number;
    diag.message = "Case expression not allowed in current scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::CaseExpression);
  for (auto& condition: expr.conditions) condition->accept(*this);
  for (auto& [case_cond, ret_expr]: expr.branches) {
    for (auto& conds: case_cond) conds->accept(*this);
    ret_expr->accept(*this);
  }
}

void SymbolResolver::visit(NDBinaryExpr& expr) {
  expr.lhs->accept(*this);
  expr.rhs->accept(*this);
}

void SymbolResolver::visit(NDUnaryExpr& expr) {
  expr.rhs->accept(*this);
}

