#pragma once

#include "ether/ast_passes/type_check/modules/module.hpp"

class TCModule_Populate: public TCModule {
  using TCModule::TCModule;
  void visit(NDLiteral& expr)          override;
  void visit(NDImportDirective& expr)  override;
  void visit(NDIdentifier& expr)       override;
  void visit(NDLetBindExpr& expr)      override;
  void visit(NDConstExpr& expr)        override;
  void visit(NDCallExpr& expr)         override;
  void visit(NDCallChain& expr)        override;
  void visit(NDFuncDeclExpr& expr)     override;
  void visit(NDCaseExpr& expr)         override;
  void visit(NDBinaryExpr& expr)       override;
  void visit(NDUnaryExpr& expr)        override;
  void visit(NDScopeExpr& expr)        override;
  void visit(NDTupleExpr& expr)        override;
  void visit(NDListExpr& expr)         override;
  void visit(NDLambdaExpr& expr)       override;
  void visit(NDFuncParam& expr)        override;
};
