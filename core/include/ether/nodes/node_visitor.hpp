#pragma once

#include "ether/nodes/node_expr.hpp"
struct NDLiteral;
struct NDImportDirective;
struct NDIdentifier;
struct NDLetBindExpr;
struct NDConstExpr;
struct NDCallExpr;
struct NDCallChain;
struct NDFuncDeclExpr;
struct NDCaseExpr;
struct NDBinaryExpr;
struct NDUnaryExpr;
struct NDScopeExpr;
struct NDListExpr;
struct NDTupleExpr;
struct NDLambdaExpr;

class Visitor {
public:
  virtual ~Visitor() = default;

  virtual void visit(NDLiteral&) = 0;
  virtual void visit(NDImportDirective&) = 0;
  virtual void visit(NDIdentifier&) = 0;
  virtual void visit(NDLetBindExpr&) = 0;
  virtual void visit(NDConstExpr&) = 0;
  virtual void visit(NDCallExpr&) = 0;
  virtual void visit(NDCallChain&) = 0;
  virtual void visit(NDFuncDeclExpr&) = 0;
  virtual void visit(NDCaseExpr&) = 0;
  virtual void visit(NDBinaryExpr&) = 0;
  virtual void visit(NDUnaryExpr&) = 0;
  virtual void visit(NDListExpr&) = 0;
  virtual void visit(NDScopeExpr&) = 0;
  virtual void visit(NDTupleExpr&) = 0;
  virtual void visit(NDLambdaExpr&) = 0;
  virtual void visit(NDFuncParam&) = 0;
};
