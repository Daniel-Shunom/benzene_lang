#pragma once
#include "../types/symbols_t.hpp"
#include <cstdint>
#include <string>

typedef enum {
  CONSTANT,
  VARIABLE,
  FUNCTION,
} SYMBOL_TYPE;

typedef struct {
  TOKEN_TYPE tok_type;
  std::string tok_val;
  size_t tok_row;
  size_t tok_col;
} Token;

typedef enum {
  STRING,
  NUMBER,
  FLOAT,
  BOOLEAN,
  EXPRESSION
} EXPRESSION_TYPE;

typedef union {
  std::string str;
  int64_t     num;
  double      flt;
  bool        boolean;
} EXPRESSION_VALUE;

struct Expression;
struct Expression {
  SYMBOL_TYPE      sym;
  EXPRESSION_TYPE  expr_typ;
  union {
    EXPRESSION_VALUE expr_val;
    Expression       *expr;
  } val;
};

