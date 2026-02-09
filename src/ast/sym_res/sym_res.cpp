#include "sym_res.hpp"

void SymResolver::visit(NDImportDirective& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if ( cscope_type && (cscope_type != ScopeType::Module)) {
    // Cause I mean, if it's not here then where else would it be lol
    expr.is_poisoned = true;
    return;
  }
}


void SymResolver::visit(NDLiteral& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && (cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module)
  ) {
    // Cause I mean, if it's not here then where else would it be lol
    expr.is_poisoned = true;
    return;
  }
}

void SymResolver::visit(NDIdentifier& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && (cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module)
  ) {
    // Cause I mean, if it's not here then where else would it be lol
    expr.is_poisoned = true;
    return;
  }
}

void SymResolver::visit(NDLetBindExpr& expr) {
  auto ident = expr.identifier->identifier.token_value;
  auto sym = this->sym_table.lookup(ident);

  if (sym) { return; }
  auto cscope_type = this->sym_table.get_current_scope_type();

  if (
    cscope_type 
    && (cscope_type != ScopeType::FunctionExpression 
    && cscope_type != ScopeType::ScopedExpression)
  ) {
    // We mark this node as poisoned because let expressions are not
    // allowed outside of function expressiosn or scoped expressions.
    expr.is_poisoned = true;
    expr.bound_value->accept(*this);
  }

  expr.bound_value->accept(*this);
  this->sym_table.declare(expr.identifier->identifier, SymbolKind::Binding);
  return;
}

void SymResolver::visit(NDConstExpr& expr) {
  auto ident = expr.identifier->identifier.token_value;
  auto sym = this->sym_table.lookup(ident);

  if (sym) { return; }
  auto cscope_type = this->sym_table.get_current_scope_type();

  if (
    cscope_type 
    && (cscope_type != ScopeType::Module 
    && cscope_type != ScopeType::Application)
  ) {
    // We mark this node as poisoned because const expressions are not
    // allowed only at the top level modules or in the application scope.
    expr.is_poisoned = true;
    return;
  }

  this->sym_table.declare(expr.identifier->identifier, SymbolKind::Constant);
  return;
}


void SymResolver::visit(NDCallExpr& expr) {
  auto ident  = expr.identifier->identifier.token_value;
  auto sym = this->sym_table.lookup(ident);

  if (!sym) {
    // Marking as poisoned because this would not be a defined 
    // this function.
    expr.is_poisoned = true;
    for (auto& arg: expr.args) {
      arg->accept(*this);
    }

    return;
  }

  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && (cscope_type != ScopeType::CaseExpression 
    && cscope_type != ScopeType::ScopedExpression 
    && cscope_type != ScopeType::FunctionExpression)
  ) {
    expr.is_poisoned = true;
  }
  for (auto& arg: expr.args) {
    arg->accept(*this);
  }

  return;
}

void SymResolver::visit(NDCallChain& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && (cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::CaseExpression
  )
  ) {
    expr.is_poisoned = true;
  }

  for (auto& call: expr.calls) call->accept(*this);
}

void SymResolver::visit(NDFuncDeclExpr& expr) {
  ScopeGuard guard(this->sym_table, ScopeType::FunctionExpression);
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && (cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module)
  ) {
    expr.is_poisoned = true;
  }

  auto _ = this->sym_table.lookup(expr.func_identifier.token_value);
  if (!_) this->sym_table.declare(expr.func_identifier, SymbolKind::Function);

  for (auto& arg: expr.func_params) {
    sym_table.declare(Token{.token_value = arg.param_name}, SymbolKind::FuncParam);
  }
  for (auto& body_expr: expr.func_body) body_expr->accept(*this);
  return;
}

void SymResolver::visit(NDScopeExpr& expr) {
  ScopeGuard guard(this->sym_table, ScopeType::ScopedExpression);
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && (cscope_type != ScopeType::FunctionExpression 
    && cscope_type != ScopeType::ScopedExpression)
  ) {
    // scoped expressions can only appear in other scoped expressions 
    // or function expressions.
    expr.is_poisoned = true;
  }
  for (auto& scope_expr: expr.expressions) scope_expr->accept(*this);
}

void SymResolver::visit(NDCaseExpr& expr) {
  ScopeGuard guard(this->sym_table, ScopeType::CaseExpression);
  for (auto& condition: expr.conditions) condition->accept(*this);
  for (auto& [case_cond, ret_expr]: expr.branches) {
    for (auto& conds: case_cond) conds->accept(*this);
    ret_expr->accept(*this);
  }
}

void SymResolver::visit(NDBinaryExpr& expr) {
  expr.lhs->accept(*this);
  expr.rhs->accept(*this);
}

void SymResolver::visit(NDUnaryExpr& expr) {
  expr.rhs->accept(*this);
}
