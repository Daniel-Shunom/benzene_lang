#include "err_res.hpp"

void ErrorRes::visit(NDLiteral& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDIdentifier& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDImportDirective& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDUnaryExpr& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDBinaryExpr& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDScopeExpr& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDCaseExpr& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDCallExpr& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDCallChain& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDFuncDeclExpr& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDLetBindExpr& expr) {
  if (!expr.is_poisoned) return;
}

void ErrorRes::visit(NDConstExpr& expr) {
  if (!expr.is_poisoned) return;
}
