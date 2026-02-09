#pragma once
#include "../../nodes/node_visitor.hpp"
#include "../../symbols/symbol_types.hpp"
#include "../../nodes/node_expr.hpp"
#include "../../symbols/symtable.hpp"

class SymResolver: public Visitor {
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

private:
  SymbolTable sym_table{};
};
