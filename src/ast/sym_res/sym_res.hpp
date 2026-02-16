#pragma once
#include "../../diagnostics/diagnostic_eng.hpp"
#include "../../nodes/node_visitor.hpp"
#include "../../symbols/symbol_types.hpp"
#include "../../nodes/node_expr.hpp"
#include "../../symbols/symtable.hpp"
#include <string>
#include <format>
#include <vector>

enum class SymErrType {
  InvalidDeclaration,
  InvalidScope,
  InvalidFuncCall
};

inline std::string sym_err_to_str(SymErrType err) {
  switch (err) {
    using t = SymErrType;
    case t::InvalidDeclaration: return "Invalid symbol declaration";
    case t::InvalidScope: return "Invalid scope";
    case t::InvalidFuncCall: return "Invalid function call";
    default: return "Unreachable symbol error";
  }
}

class SymResolver: public Visitor {
public:
  SymResolver(DiagnosticEngine& eng)
  : diag_eng(eng) {}

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
  DiagnosticEngine& diag_eng;
};
