#pragma once
#include "ether/ast_passes/type_check/modules/module.hpp"
#include <ether/nodes/node_visitor.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/types/types.hpp>
#include <memory>
#include <vector>

class TypeChecker: public Visitor {
public:
  TypeChecker();
  void dispatch_to_modules(Node&);

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

private:
  TypeVarFactory varFactory;
  std::vector<std::unique_ptr<TCModule>> modules;
};

