#pragma once
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

  void log_errors() {
    for (const auto& err: this->errors) {
      printf("%s\n", err.data());
    }
  }

private:
  std::vector<std::string> errors;
  SymbolTable sym_table{};

  std::string make_error(
    SymErrType err_type,
    Token& tok, 
    const std::string msg
  ) {
    constexpr auto RESET  = "\033[0m";
    constexpr auto RED    = "\033[31m";
    constexpr auto YELLOW = "\033[33m";
    constexpr auto CYAN   = "\033[36m";
    constexpr auto BOLD   = "\033[1m";

    auto header = std::format(
      "{}{}Symbol Error:{} {}{}{} ({}Ln {}, Col {}{})",
      BOLD, RED, RESET,
      CYAN, sym_err_to_str(err_type), RESET,
      YELLOW, tok.line_number, tok.column_number, RESET
    );

    return std::format(
      "{}\n {}\n\n",
      header,
      msg
    );
  };

  std::string make_error(
    SymErrType err_type,
    const std::string& msg
  ) {
    constexpr auto RESET  = "\033[0m";
    constexpr auto RED    = "\033[31m";
    constexpr auto YELLOW = "\033[33m";
    constexpr auto CYAN   = "\033[36m";
    constexpr auto BOLD   = "\033[1m";

    auto header = std::format(
      "{}{}Scope Error:{} {}{}{}",
      BOLD, RED, RESET,
      CYAN, sym_err_to_str(err_type), RESET
    );
    
    return std::format(
      "{}\n {}{}{}\n\n",
      header,
      YELLOW, msg, RESET
    );
  }
};
