#pragma once
#include "../../nodes/node_visitor.hpp"
#include "../../nodes/node_expr.hpp"

class TypeChecker: public Visitor {
public:
  void visit(NDLiteral&) override;
  void visit(NDImportDirective&) override;
  void visit(NDIdentifier&) override;
  void visit(NDLetBindExpr&) override;
  void visit(NDConstExpr&) override;
  void visit(NDCallExpr&) override;
  void visit(NDCallChain&) override;
  void visit(NDFuncDeclExpr&) override;
  void visit(NDCaseExpr&) override;
  void visit(NDBinaryExpr&) override;
  void visit(NDUnaryExpr&) override;
  void visit(NDScopeExpr&) override;
};
