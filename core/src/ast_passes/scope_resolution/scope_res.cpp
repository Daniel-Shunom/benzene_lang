#include "ether/diagnostics/diagnostic.hpp"
#include "ether/nodes/node_expr.hpp"
#include "ether/symbols/scopes.hpp"
#include "ether/symbols/symtable.hpp"
#include <ether/ast_passes/scope_resolution/scope_res.hpp>

void ScopeRes::visit(NDLiteral& expr) { }

void ScopeRes::visit(NDImportDirective& import) {
  auto scope = this->sym_table.get_current_scope_type();
  if (scope && (scope != ScopeType::Module)) {
    import.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase  = DiagnosticPhase::ScopeResolution;
    diag.location.column = import.import_directive.column_number;
    diag.location.line = import.import_directive.line_number;
    diag.message = "Imports should be at the top of the module";

    this->diag_eng.report(diag);
  }
}

void ScopeRes::visit(NDIdentifier& identifier) { }

void ScopeRes::visit(NDLetBindExpr& let_bind) {
  auto scope = this->sym_table.get_current_scope_type();
  if (
    scope
    && scope != ScopeType::FunctionExpression
    && scope != ScopeType::LambdaExpression
    && scope != ScopeType::ScopedExpression
  ) {
    let_bind.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.column = let_bind.identifier->identifier.column_number;
    diag.location.line = let_bind.identifier->identifier.line_number;
    diag.message = "`let` expression is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  let_bind.bound_value->accept(*this);
  return;
}

void ScopeRes::visit(NDConstExpr& const_expr) {
  auto scope = this->sym_table.get_current_scope_type();
  if ( scope && scope != ScopeType::Module) {
    const_expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.column = const_expr.identifier->identifier.column_number;
    diag.location.line = const_expr.identifier->identifier.line_number;
    diag.message = "`const` expression is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  const_expr.bound_value->accept(*this);
  return;
}


void ScopeRes::visit(NDCallExpr& func_call) {
  auto scope = this->sym_table.get_current_scope_type();
  if (
    scope
    && scope != ScopeType::FunctionExpression
    && scope != ScopeType::ScopedExpression
    && scope != ScopeType::LambdaExpression
    && scope != ScopeType::CaseExpression
  ) {
    func_call.is_poisoned = true;
    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.column = func_call.identifier->identifier.column_number;
    diag.location.line = func_call.identifier->identifier.line_number;
    diag.message = "Cannot call a function outside of a valid scope";

    this->diag_eng.report(diag);
    return;
  }

  for (auto& arg: func_call.args) {
    arg->accept(*this);
  }

  return;
}

void ScopeRes::visit(NDCallChain& call_chain) {
  auto scope = this->sym_table.get_current_scope_type();
  if (
    scope
    && scope != ScopeType::FunctionExpression
    && scope != ScopeType::ScopedExpression
    && scope != ScopeType::LambdaExpression
    && scope != ScopeType::CaseExpression
  ) {
    call_chain.is_poisoned = true;
    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.column = call_chain.start_token.column_number;
    diag.location.line = call_chain.start_token.line_number;
    diag.message = "Cannot chain functions outside of a valid scope";

    this->diag_eng.report(diag);
    return;
  }

  for (auto& call: call_chain.calls) call->accept(*this);
  return;
}

void ScopeRes::visit(NDFuncDeclExpr& func_decl) {
  auto scope = this->sym_table.get_current_scope_type();
  if (
    scope
    && scope != ScopeType::Module
  ) {
    func_decl.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.column = func_decl.func_identifier.column_number;
    diag.location.line = func_decl.func_identifier.line_number;
    diag.message = "Function declaration not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::FunctionExpression);
  for (auto& expr: func_decl.func_body) expr->accept(*this);
  return;
}

void ScopeRes::visit(NDLambdaExpr& lambda) {
  auto scope = this->sym_table.get_current_scope_type();
  if (
    scope
    && scope != ScopeType::FunctionExpression
    && scope != ScopeType::LambdaExpression
    && scope != ScopeType::ScopedExpression
    && scope != ScopeType::CaseExpression
  ) {
    lambda.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.column = lambda.lambda_start.column_number;
    diag.location.line = lambda.lambda_start.line_number;
    diag.message = "Lambda declaration not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::LambdaExpression);
  for (auto& expr: lambda.func_body) expr->accept(*this);
  return;
}

void ScopeRes::visit(NDCaseExpr& case_expr) {
  auto scope = this->sym_table.get_current_scope_type();
  if (
    scope
    && scope != ScopeType::FunctionExpression
    && scope != ScopeType::ScopedExpression
    && scope != ScopeType::LambdaExpression
    && scope != ScopeType::CaseExpression
  ) {
    case_expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.line = case_expr.case_keyword.line_number;
    diag.location.column = case_expr.case_keyword.column_number;
    diag.message = "Case expressions are not allowed in current scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::CaseExpression);
  for (auto& condition: case_expr.conditions) condition->accept(*this);
  for (auto& [case_conditions, return_expression]: case_expr.branches) {
    for (auto& condition: case_conditions) condition->accept(*this);
    return_expression->accept(*this);
  }
  return;
}

void ScopeRes::visit(NDBinaryExpr& binary_expr) {
  binary_expr.lhs->accept(*this);
  binary_expr.rhs->accept(*this);
}

void ScopeRes::visit(NDUnaryExpr& unary_expr) {
  unary_expr.rhs->accept(*this);
}

void ScopeRes::visit(NDScopeExpr& scope_expr) {
  auto scope = this->sym_table.get_current_scope_type();
  if (
    scope
    && scope != ScopeType::FunctionExpression
    && scope != ScopeType::LambdaExpression
    && scope != ScopeType::ScopedExpression
    && scope != ScopeType::CaseExpression
  ) {
    scope_expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.line = scope_expr.open_brace.line_number;
    diag.location.column = scope_expr.open_brace.column_number;
    diag.message = "Scoped expression is not allowed in current scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::ScopedExpression);
  for (auto& expr: scope_expr.expressions) {
    expr->accept(*this);
  }
}

void ScopeRes::visit(NDTupleExpr& tuple) {
  for (auto& value: tuple.values) {
    value->accept(*this);
  }
}

void ScopeRes::visit(NDListExpr& list) {
  for (auto& value: list.values) {
    value->accept(*this);
  }
}
