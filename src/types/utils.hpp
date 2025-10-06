#include "./symbols_t.hpp"
#include "./syntax_t.hpp"
#include "./types_t.hpp"

static inline bool isBinaryOp(const Token& token) {
  return token.tok_type == TOKEN_PLUS
    || token.tok_type == TOKEN_MINUS
    || token.tok_type == TOKEN_MODULO
    || token.tok_type == TOKEN_EXPONENT
    || token.tok_type == TOKEN_SLASH_FORWARD;
}

static inline bool isBooleanOp(const Token& token) {
  return token.tok_type == TOKEN_BOOLEAN_EQUAL
    || token.tok_type == TOKEN_BOOLEAN_AND
    || token.tok_type == TOKEN_BOOLEAN_NOT
    || token.tok_type == TOKEN_BOOLEAN_OR;
}

static inline Type tokToType(const Token& token) {
  Type t = Type::Nil;

  switch (token.tok_type) {
    case (TOKEN_STRING_LITERAL): {
      t = Type::String;
    }

    case (TOKEN_FLOAT_LITERAL): {
      t = Type::Float;
    }

    case (TOKEN_INTEGER_LITERAL): {
      t = Type::Integer;
    }

    default: {
      t = Type::Nil;
    }
  }

  return t;
}

static inline bool isValidBenzeneType(const Token& token) {
  return token.tok_type == KEYWORD_BOOL
    || token.tok_type == KEYWORD_STRING
    || token.tok_type == KEYWORD_FLOAT
    || token.tok_type == KEYWORD_INT;
}

static inline Type getTypeAnnotation(const Token& token) {
  switch (token.tok_type) {
    case (KEYWORD_BOOL): {
      return Type::Bool;
    }

    case (KEYWORD_INT): {
      return Type::Integer;
    }

    case (KEYWORD_FLOAT): {
      return Type::Float;
    }

    case (KEYWORD_STRING): {
      return Type::String;
    }

    default: return Type::Undefined;
  }
}
