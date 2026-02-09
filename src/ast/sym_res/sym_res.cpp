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
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module
  ) {
    expr.is_poisoned = true;
    return;
  }
}

void SymResolver::visit(NDIdentifier& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module
  ) {
    // Cause I mean, if it's not here then where else would it be lol
    expr.is_poisoned = true;
    return; }
}

void SymResolver::visit(NDLetBindExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && cscope_type != ScopeType::FunctionExpression 
    && cscope_type != ScopeType::ScopedExpression
  ) {
    // We mark this node as poisoned because let expressions are not
    // allowed outside of function expressions or scoped expressions.
    expr.is_poisoned = true;
    return;
  }

  auto expr_sym = this->sym_table.declare(expr.identifier->identifier, SymbolKind::Binding);
  if (!expr_sym) {
    expr.is_poisoned = true;
    return;
  }

  BindingData binding_data;

  if (expr.type) {
    binding_data.binding_type.type_name = expr.type->token_value;
  }

  expr.bound_value->accept(*this);
  return;
}

void SymResolver::visit(NDConstExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();

  if (
    cscope_type 
    && cscope_type != ScopeType::Module 
    && cscope_type != ScopeType::Application
  ) {
    expr.is_poisoned = true;
    return;
  }

  auto const_sym = this->sym_table.declare(expr.identifier->identifier, SymbolKind::Constant);
  if (!const_sym) {
    expr.is_poisoned = true;
    return;
  }

  expr.literal.accept(*this);
  return;
}


void SymResolver::visit(NDCallExpr& expr) {
  auto ident  = expr.identifier->identifier.token_value;
  auto sym = this->sym_table.lookup(ident);

  if (!sym) {
    expr.is_poisoned = true;
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
    return;
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
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::CaseExpression
  ) {
    expr.is_poisoned = true;
    return;
  }

  for (auto& call: expr.calls) call->accept(*this);
}

void SymResolver::visit(NDFuncDeclExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type 
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::Module
  ) {
    expr.is_poisoned = true;
    return;
  }

  auto func_sym = this->sym_table.declare(expr.func_identifier, SymbolKind::Function);
  if (!func_sym) {
    // Means this is a redeclaration of another function.
    expr.is_poisoned = true;
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::FunctionExpression);

  for (auto& arg: expr.func_params) {
    // Need to handle edge case where param_names are duplicated.
    sym_table.declare(Token{.token_value = arg.param_name}, SymbolKind::FuncParam);
  }

  for (auto& body_expr: expr.func_body) body_expr->accept(*this);

  FunctionData func_data;

  if (expr.return_type) {
    func_data.funtion_return_type.type_name = expr.return_type->token_value;
  }

  func_sym->symbol_data = func_data;
  return;
}

void SymResolver::visit(NDScopeExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionExpression 
    && cscope_type != ScopeType::ScopedExpression
  ) {
    // I think scoped expressions should only appear in other scoped expressions 
    // or function expressions.
    expr.is_poisoned = true;
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::ScopedExpression);
  for (auto& scope_expr: expr.expressions) scope_expr->accept(*this);
}

void SymResolver::visit(NDCaseExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionExpression
    && cscope_type != ScopeType::ScopedExpression
  ) {
    expr.is_poisoned = true;
    return;
  }
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
