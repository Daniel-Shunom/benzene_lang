#include "ether/ast_passes/type_check/modules/unify.hpp"
#include "ether/nodes/node_expr.hpp"
void TCModule_Unify::visit(NDLiteral&) { }
void TCModule_Unify::visit(NDImportDirective&) { }
void TCModule_Unify::visit(NDIdentifier&) { }
void TCModule_Unify::visit(NDLetBindExpr&) { }
void TCModule_Unify::visit(NDConstExpr&) { }
void TCModule_Unify::visit(NDCallExpr&) { }
void TCModule_Unify::visit(NDCallChain&) { }
void TCModule_Unify::visit(NDTypeDecl& expr) {
  if (expr.alias_target) {
    expr.alias_target->accept(*this);
  }
  if (expr.sub_types) {
    for (auto& member : *expr.sub_types) {
      member.accept(*this);
    }
  }
}
void TCModule_Unify::visit(NDFuncDeclExpr&) { }
void TCModule_Unify::visit(NDCaseExpr&) { }
void TCModule_Unify::visit(NDBinaryExpr&) { }
void TCModule_Unify::visit(NDUnaryExpr&) { }
void TCModule_Unify::visit(NDScopeExpr&) { }
void TCModule_Unify::visit(NDTupleExpr&) { }
void TCModule_Unify::visit(NDListExpr&) { }
void TCModule_Unify::visit(NDLambdaExpr&) { }
void TCModule_Unify::visit(NDFuncParam& expr) {
  if (expr.param_type) {
    expr.param_type->accept(*this);
  }
}

void TCModule_Unify::visit(NDTypeExpr&) {
  // Type-expression semantics are not implemented in this pass yet.
}
