#pragma once

#include <iostream>
#include "../src/types/symbols_t.hpp"
#include "../src/ast/expr_l.hpp"

inline const char* tokenTypeToString(TOKEN_TYPE type) {
  switch (type) {
    case TOKEN_PLUS: return "TOKEN_PLUS";
    case TOKEN_MINUS: return "TOKEN_MINUS";
    case TOKEN_SLASH_FORWARD: return "TOKEN_SLASH_FORWARD";
    case TOKEN_MODULO: return "TOKEN_MODULO";
    case TOKEN_EXPONENT: return "TOKEN_EXPONENT";
    case TOKEN_SLASH_BACKWARD: return "TOKEN_SLASH_BACKWARD";
    case TOKEN_ASTERISK: return "TOKEN_ASTERISK";
    case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
    case TOKEN_RETURN_TYPE: return "TOKEN_RETURN_TYPE";
    case TOKEN_CONSTANT: return "TOKEN_CONSTANT";
    case TOKEN_DELIM: return "TOKEN_DELIM";

    case TOKEN_BOOLEAN_EQUAL: return "TOKEN_BOOLEAN_EQUAL";
    case TOKEN_BOOLEAN_OR: return "TOKEN_BOOLEAN_OR";
    case TOKEN_BOOLEAN_AND: return "TOKEN_BOOLEAN_AND";
    case TOKEN_BOOLEAN_NOT: return "TOKEN_BOOLEAN_NOT";

    case TOKEN_GREATER: return "TOKEN_GREATER";
    case TOKEN_LESS: return "TOKEN_LESS";
    case TOKEN_GREATER_EQUAL: return "TOKEN_GREATER_EQUAL";
    case TOKEN_LESS_EQUAL: return "TOKEN_LESS_EQUAL";

    case TOKEN_PAREN_LEFT: return "TOKEN_PAREN_LEFT";
    case TOKEN_PAREN_RIGHT: return "TOKEN_PAREN_RIGHT";
    case TOKEN_SQUARE_BRAC_LEFT: return "TOKEN_SQUARE_BRAC_LEFT";
    case TOKEN_SQUARE_BRAC_RIGHT: return "TOKEN_SQUARE_BRAC_RIGHT";
    case TOKEN_SQUIG_BRAC_LEFT: return "TOKEN_SQUIG_BRAC_LEFT";
    case TOKEN_SQUIG_BRAC_RIGHT: return "TOKEN_SQUIG_BRAC_RIGHT";

    case TOKEN_SEMI_COLON: return "TOKEN_SEMI_COLON";
    case TOKEN_COLON: return "TOKEN_COLON";

    case TOKEN_CASE_OPEN: return "TOKEN_CASE_OPEN";
    case TOKEN_CASE_CLOSE: return "TOKEN_CASE_CLOSE";

    case TOKEN_QUOTE_SINGLE: return "TOKEN_QUOTE_SINGLE";
    case TOKEN_QUOTE_DOUBLE: return "TOKEN_QUOTE_DOUBLE";

    case TOKEN_SYMBOL: return "TOKEN_SYMBOL";
    case TOKEN_STRING_LITERAL: return "TOKEN_STRING_LITERAL";
    case TOKEN_INTEGER_LITERAL: return "TOKEN_INTEGER_LITERAL";
    case TOKEN_FLOAT_LITERAL: return "TOKEN_FLOAT_LITERAL";

    case KEYWORD_IF: return "KEYWORD_IF";
    case KEYWORD_FUNC_BEG: return "KEYWORD_FUNC_BEG";
    case KEYWORD_FUNC_END: return "KEYWORD_FUNC_END";
    case KEYWORD_FALSE: return "KEYWORD_FALSE";
    case KEYWORD_TRUE: return "KEYWORD_TRUE";

    case KEYWORD_INT: return "KEYWORD_INT";
    case KEYWORD_FLOAT: return "KEYWORD_FLOAT";
    case KEYWORD_STRING: return "KEYWORD_STRING";
    case KEYWORD_BOOL: return "KEYWORD_BOOL";

    case KEYWORD_NIL: return "KEYWORD_NIL";
    case KEYWORD_CASE: return "KEYWORD_CASE";
    case KEYWORD_DEFAULT: return "KEYWORD_DEFAULT";

    case TOKEN_EOF: return "TOKEN_EOF";
    case TOKEN_UNDEF: return "TOKEN_UNDEF";
    case TOKEN_PIPE_OPERATOR: return "TOKEN_PIPE_OPERATOR";

    default: return "UNKOWN_TOKEN";
  }
}



#include <iostream>
#include <sstream>
#include <string>
#include <memory>

// helpers to print indentation
static inline void pad(int indent) {
  for (int i = 0; i < indent; ++i) std::cout << "  ";
}

// fallback type->string. Replace/extend this with your real Type names if you have them.
static inline std::string typeToString(Type t) {
  // Example: if Type is an enum class, cast to int for now.
  return std::to_string(static_cast<int>(t));
}

// visitor to stringify the Value::value variant
struct ValuePrinter {
  std::string operator()(int i) const {
    return std::to_string(i);
  }
  std::string operator()(double d) const {
    std::ostringstream ss;
    ss << d;
    return ss.str();
  }
  std::string operator()(const std::string &s) const {
    return "\"" + s + "\"";
  }
  std::string operator()(bool b) const {
    return b ? "true" : "false";
  }
  std::string operator()(std::monostate) const {
    return "null";
  }
  std::string operator()(const std::shared_ptr<FunctionDef> &fn) const {
    if (!fn) return "<FunctionDef:nullptr>";
    // FunctionDef appears to have a .name field in your code; adjust if different.
    return "<FunctionDef:" + fn->name + ">";
  }
};

static inline std::string stringifyValue(const Value &v) {
  return std::visit(ValuePrinter{}, v.value);
}

// The tree printer for Expr (matches your Expr/variant layout)



static inline void printExprTree(const Expr& expr, int depth = 0) {
  std::string indent(depth * 2, ' ');

  switch (expr.kind) {
    case ExprKind::BinaryOp: {
      const auto& bin = std::get<BinaryExpr>(expr.node);
      std::cout << indent << "Binary Operation";
      if (!bin.op.empty()) {
        std::cout << " (op='" << bin.op << "')";
      }
      std::cout << "\n";

      std::cout << indent << "  |-- Left:\n";
      if (bin.left)
        printExprTree(*bin.left, depth + 2);
      else
        std::cout << indent << "    <null>\n";

      std::cout << indent << "  |-- Right:\n";
      if (bin.right)
        printExprTree(*bin.right, depth + 2);
      else
        std::cout << indent << "    <null>\n";
      break;
    }

    case ExprKind::BooleanOp: {
      const auto& boolOp = std::get<BooleanExpr>(expr.node);
      std::cout << indent << "Boolean Operation";
      if (!boolOp.op.empty()) {
        std::cout << " (op='" << boolOp.op << "')";
      }
      std::cout << "\n";

      std::cout << indent << "  |-- Left:\n";
      if (boolOp.left)
        printExprTree(*boolOp.left, depth + 2);
      else
        std::cout << indent << "    <null>\n";

      std::cout << indent << "  |-- Right:\n";
      if (boolOp.right)
        printExprTree(*boolOp.right, depth + 2);
      else
        std::cout << indent << "    <null>\n";
      break;
    }

    case ExprKind::Literal: {
      const auto& lit = std::get<LiteralExpr>(expr.node);
      std::cout << indent << "Literal: " << stringifyValue(lit.value) << "\n";
      break;
    }

    case ExprKind::Variable: {
      const auto& var = std::get<VariableExpr>(expr.node);
      std::cout << indent << "Variable: " << var.variable << "\n";
      break;
    }

    case ExprKind::Constant: {
      const auto& cst = std::get<ConstantExpr>(expr.node);
      std::cout << indent << "Constant: " << cst.variable << "\n";
      break;
    }

    default:
      std::cout << indent << "<unknown>\n";
      break;
  }
}

