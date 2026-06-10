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

inline auto symbol_error_to_str(SymbolErrorKind err) -> std::string {
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

  auto take_exports() -> std::unordered_map<std::string, SymbolAttr*> {
    return std::move(exports);
  }

private:
  DiagnosticEngine& diag_eng;
  SymbolTable sym_table;
  std::unordered_map<std::string, SymbolAttr*> exports;
};
