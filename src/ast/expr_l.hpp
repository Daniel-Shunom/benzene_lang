#pragma once

#include "./node_val_t.hpp"
#include <vector>

struct Expr;

enum class ExprKind {
  Literal,
  Variable,
  Constant,
  BinaryOp,
  BooleanOp,
  FuncCall,
  FuncDef,
  ConstExpr,
  Undefined,
};

/*
 * For declaring literals.
*/
typedef struct {
  Value value;
} LiteralExpr;

/*
 * When we are evaluating this, we just look in the,
 * symbol table for that variable, and evaluate if the
 * variable is there or not.
*/
typedef struct {
  std::string variable;
} VariableExpr;

/*
* For declaring constants
*/
typedef struct {
  std::string variable;
} ConstantExpr;

/*
 * A regular old binary expression :)
*/
typedef struct {
  std::string op;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;
} BinaryExpr;

typedef struct {
  std::string op;
  std::unique_ptr<Expr> left;
  std::unique_ptr<Expr> right;
} BooleanExpr;

typedef struct {
  std::string callee;
  std::shared_ptr<std::vector<Expr>> expr;
} FuncCallExpr;

typedef struct {
  std::string func_name;
  FunctionDef def;
  std::unique_ptr<std::vector<Expr>> body;
} FuncDefExpr;

/*
 * An expression is something that evaluates to. 
 * In this language, all functions evaluate to a 
 * value hence all functions and "variables" are
 * expressions
*/
typedef struct {} UndefinedExpr;

struct Expr {
  ExprKind kind;
  std::variant<
    LiteralExpr,
    VariableExpr,
    ConstantExpr,
    BinaryExpr,
    BooleanExpr,
    FuncCallExpr,
    FuncDefExpr,
    UndefinedExpr
  > node;
};

static inline const char* exprKindToStr(ExprKind kind) {
  switch (kind) {
    case (ExprKind::Undefined): {
      return "Undefined Term";
    }

    case (ExprKind::FuncCall): {
      return "Function call";
    }

    case (ExprKind::BinaryOp): {
      return "Binary Operation";
    }

    case (ExprKind::BooleanOp): {
      return "Boolean operation";
    }

    case (ExprKind::Constant): {
      return "Constant";
    }
    
    case (ExprKind::FuncDef): {
      return "Function defintion";
    }

    case (ExprKind::Literal): {
      return "Literal";
    }

    case (ExprKind::Variable): {
      return "Variable";
    }

    case (ExprKind::ConstExpr): {
      return "Constant Expression";
    }
  }

  return "Undefined";
}
