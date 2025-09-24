#pragma once

#include "./node_val_t.hpp"

struct Expr;

enum class ExprKind {
  Literal,
  Variable,
  Constant,
  BinaryOp,
  FuncCall,
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
  std::string callee;
  std::unique_ptr<Expr> expr;
} FuncCallExpr;

/*
 * An expression is something that evaluates to. 
 * In this language, all functions evaluate to a 
 * value hence all functions and "variables" are
 * expressions
*/

struct Expr {
  ExprKind kind;
  std::variant<
    LiteralExpr,
    VariableExpr,
    ConstantExpr,
    BinaryExpr,
    FuncCallExpr
  > node;
};
