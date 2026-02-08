#pragma once

struct NDLiteral;
struct NDImportDirective;
struct NDIdentifier;
struct NDLetBindExpr;
struct NDConstExpr;
struct NDCallExpr;
struct NDCallChain;
struct NDFuncDeclExpr;
struct NDCaseExpr;

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
};
