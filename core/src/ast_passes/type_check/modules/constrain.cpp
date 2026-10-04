#include "ether/ast_passes/type_check/modules/constrain.hpp"
void TCModule_Constrain::visit(NDLiteral&) { }
void TCModule_Constrain::visit(NDImportDirective&) { }
void TCModule_Constrain::visit(NDIdentifier&) { }
void TCModule_Constrain::visit(NDLetBindExpr&) { }
void TCModule_Constrain::visit(NDConstExpr&) { }
void TCModule_Constrain::visit(NDCallExpr&) { }
void TCModule_Constrain::visit(NDCallChain&) { }
void TCModule_Constrain::visit(NDTypeDecl& expr) {
  if (expr.alias_target) {
    expr.alias_target->accept(*this);
  }
  if (expr.sub_types) {
    for (auto& member : *expr.sub_types) {
      member.accept(*this);
    }
  }
}
void TCModule_Constrain::visit(NDFuncDeclExpr&) { }
void TCModule_Constrain::visit(NDCaseExpr&) { }
void TCModule_Constrain::visit(NDBinaryExpr&) { }
void TCModule_Constrain::visit(NDUnaryExpr&) { }
void TCModule_Constrain::visit(NDScopeExpr&) { }
void TCModule_Constrain::visit(NDTupleExpr&) { }
void TCModule_Constrain::visit(NDListExpr&) { }
void TCModule_Constrain::visit(NDLambdaExpr&) { }
void TCModule_Constrain::visit(NDFuncParam& expr) {
  if (expr.param_type) {
    expr.param_type->accept(*this);
  }
}

void TCModule_Constrain::visit(NDTypeExpr&) {
  // Type-expression semantics are not implemented in this pass yet.
}
