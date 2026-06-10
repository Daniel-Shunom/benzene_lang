#pragma once
#include "ether/symbols/symtable.hpp"
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/nodes/node_visitor.hpp>

class ScopeRes : public Visitor {
public:
  ScopeRes(SymbolStorage& arena, DiagnosticEngine& diag)
    : diag_eng(diag), sym_table(arena) {}

  void visit(NDLiteral& expr)         override;
  void visit(NDImportDirective& expr) override;
  void visit(NDIdentifier& expr)      override;
  void visit(NDLetBindExpr& expr)     override;
  void visit(NDConstExpr& expr)       override;
  void visit(NDCallExpr& expr)        override;
  void visit(NDCallChain& expr)       override;
  void visit(NDFuncDeclExpr& expr)    override;
  void visit(NDCaseExpr& expr)        override;
  void visit(NDBinaryExpr& expr)      override;
  void visit(NDUnaryExpr& expr)       override;
  void visit(NDScopeExpr& expr)       override;
  void visit(NDTupleExpr& expr)       override;
  void visit(NDListExpr& expr)        override;
  void visit(NDLambdaExpr& expr)      override;
  void visit(NDFuncParam& expr)       override;

private:
  DiagnosticEngine& diag_eng;
  SymbolTable sym_table;
};
