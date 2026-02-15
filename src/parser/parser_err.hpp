#pragma once
#include <format>
#include <string>
#include "../tokens/token_types.hpp"

enum class ParseErrorType {
  InvalidImportExpr,
  InvalidLetExpr,
  InvalidConstExpr,
  InvalidCaseExpr,
  InvalidFuncDeclExpr,
  InvalidFuncCallExpr,
  InvalidScopedExpr,
  InvalidPrimaryExpr,
  InvalidUnaryExpr,
  InvalidBinaryExpr,
  InvalidTypeAnnotation,

  InvalidTopLvlExpr,
};

const inline std::string p_err_tToStr(ParseErrorType err) {
  switch (err) {
    using t = ParseErrorType;
    case t::InvalidUnaryExpr: return "Invalid unary expression";
    case t::InvalidPrimaryExpr: return "Invalid primary expression";
    case t::InvalidBinaryExpr: return "Invalid binary expression";
    case t::InvalidLetExpr: return "Invalid `let` expression";
    case t::InvalidCaseExpr: return "Invalid `case` expression";
    case t::InvalidConstExpr: return "Invalid `const` expression";
    case t::InvalidScopedExpr: return "Invalid scoped Expr";
    case t::InvalidFuncCallExpr: return "Invalid function call";
    case t::InvalidFuncDeclExpr: return "Invalid function declaration";
    case t::InvalidTopLvlExpr: return "Invalid Top Level expression";
    case t::InvalidImportExpr: return "Invalid Import expression";
    case t::InvalidTypeAnnotation: return "Invalid type annotation";
    default: return "Unknown parser error type";
  }
}
inline std::string make_parser_error(
  ParseErrorType t,
  Token& tok,
  std::string msg
) {
  constexpr auto RESET  = "\033[0m";
  constexpr auto RED    = "\033[31m";
  constexpr auto YELLOW = "\033[33m";
  constexpr auto CYAN   = "\033[36m";
  constexpr auto BOLD   = "\033[1m";

  auto header = std::format(
    "{}{}Error:{} {}{}{} ({}Ln {}, Col {}{})",
    BOLD, RED, RESET,
    CYAN, p_err_tToStr(t), RESET,
    YELLOW, tok.line_number, tok.column_number, RESET
  );

  return std::format(
    "{}\n  {}\n\n",
    header,
    msg
  );
}
