#pragma once
#include "ether/symbols/symtable.hpp"
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/nodes/node_visitor.hpp>

class ScopeRes : public Visitor {
public:
  ScopeRes(SymbolStorage& arena, DiagnosticEngine& diag)
    : diag_eng(diag), sym_table(arena) {}

  void visit(NDLiteral&)         override;
  void visit(NDImportDirective&) override;
  void visit(NDIdentifier&)      override;
  void visit(NDLetBindExpr&)     override;
  void visit(NDConstExpr&)       override;
  void visit(NDCallExpr&)        override;
  void visit(NDCallChain&)       override;
  void visit(NDFuncDeclExpr&)    override;
  void visit(NDCaseExpr&)        override;
  void visit(NDBinaryExpr&)      override;
  void visit(NDUnaryExpr&)       override;
  void visit(NDScopeExpr&)       override;
  void visit(NDTupleExpr&)       override;
  void visit(NDListExpr&)        override;
  void visit(NDLambdaExpr&)      override;

private:
  DiagnosticEngine& diag_eng;
  SymbolTable sym_table;
};
