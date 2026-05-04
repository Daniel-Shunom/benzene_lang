#pragma once
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/nodes/node_visitor.hpp>
#include <ether/symbols/symbol_types.hpp>
#include <ether/symbols/symtable.hpp>
#include <string>
#include <unordered_map>

enum class SymbolErrorKind {
  InvalidDeclaration,
  InvalidScope,
  InvalidFuncCall,
};

inline std::string symbol_error_to_str(SymbolErrorKind err) {
  switch (err) {
    case SymbolErrorKind::InvalidDeclaration: return "Invalid symbol declaration";
    case SymbolErrorKind::InvalidScope:       return "Invalid scope";
    case SymbolErrorKind::InvalidFuncCall:    return "Invalid function call";
  }
  return "Unreachable symbol error";
}

class SymbolResolver : public Visitor {
public:
  SymbolResolver(SymbolStorage& arena, DiagnosticEngine& diag)
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

  std::unordered_map<std::string, SymbolAttr*> take_exports() {
    return std::move(exports);
  }

private:
  DiagnosticEngine& diag_eng;
  SymbolTable sym_table;
  std::unordered_map<std::string, SymbolAttr*> exports;
};
