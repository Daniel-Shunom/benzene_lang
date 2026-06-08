#pragma once

#include "ether/ast_passes/type_check/modules/module.hpp"

class TCModule_Unify: public TCModule {
  using TCModule::TCModule;
  void visit(NDLiteral&)          override;
  void visit(NDImportDirective&)  override;
  void visit(NDIdentifier&)       override;
  void visit(NDLetBindExpr&)      override;
  void visit(NDConstExpr&)        override;
  void visit(NDCallExpr&)         override;
  void visit(NDCallChain&)        override;
  void visit(NDFuncDeclExpr&)     override;
  void visit(NDCaseExpr&)         override;
  void visit(NDBinaryExpr&)       override;
  void visit(NDUnaryExpr&)        override;
  void visit(NDScopeExpr&)        override;
  void visit(NDTupleExpr&)        override;
  void visit(NDListExpr&)         override;
  void visit(NDLambdaExpr&)       override;
};
