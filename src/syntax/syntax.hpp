#pragma once
#include <iomanip>
#include "./symbols.hpp"
#include <iostream>
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

inline const char* tokenTypeToString(TOKEN_TYPE type) {
  switch (type) {
    case TOKEN_PLUS: return "TOKEN_PLUS";
    case TOKEN_MINUS: return "TOKEN_MINUS";
    case TOKEN_SLASH_FORWARD: return "TOKEN_SLASH_FORWARD";
    case TOKEN_SLASH_BACKWARD: return "TOKEN_SLASH_BACKWARD";
    case TOKEN_ASTERISK: return "TOKEN_ASTERISK";
    case TOKEN_ASSIGN: return "TOKEN_ASSIGN";
    case TOKEN_RETURN_TYPE: return "TOKEN_RETURN_TYPE";

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
    case KEYWORD_NIL: return "KEYWORD_NIL";

    case TOKEN_EOF: return "TOKEN_EOF";
    case TOKEN_UNDEF: return "TOKEN_UNDEF";

    default: return "UNKNOWN_TOKEN";
  }
}



inline void logToken(const Token& tok) {
  std::cout 
    << std::left << std::setw(20) << "Token"
    << "| type: " << std::setw(25) << tokenTypeToString(tok.tok_type)
    << "| val: "  << std::setw(15) << ("\"" + tok.tok_val + "\"")
    << "| row: "  << std::setw(4)  << tok.tok_row
    << "| col: "  << std::setw(4)  << tok.tok_col
    << std::endl;
}

