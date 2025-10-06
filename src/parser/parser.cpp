#include "./parser.hpp"
#include "../types/utils.hpp"
#include <cstddef>
#include <cstdio>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

using TokenBuf = std::vector<Token>;
ExprKind Parser::peek() {
  if (this->pos >= this->tokens.size()) {
    return ExprKind::Undefined;
  }

  size_t cnt = this->pos;
  ExprKind last_match = ExprKind::Undefined;
  Token& tok = this->tokens[cnt];

  switch (tok.tok_type) {
    case (TOKEN_SYMBOL): {
      if (cnt + 1 >= this->tokens.size()
         || cnt + 2 >= this->tokens.size()
      ) {
        last_match = ExprKind::Undefined;
        break;
      }

      auto& asssignment = this->tokens[cnt + 1];
      auto& value = this->tokens[cnt + 2];

      if (asssignment.tok_type != TOKEN_ASSIGN) {
        last_match = ExprKind::Undefined;
        break;
      }

      switch (value.tok_type) {
        case (TOKEN_SYMBOL):
        case (TOKEN_INTEGER_LITERAL):
        case (TOKEN_FLOAT_LITERAL): {
          auto&& v = this->tokFromCurrPos(3);

          if (!v) {
            last_match = ExprKind::Literal;
            break;
          }

          if (isBinaryOp(*v)) {
            last_match = ExprKind::BinaryOp;
          } else if (isBooleanOp(*v)) {
            last_match = ExprKind::BooleanOp;
          } else if (v->tok_type == TOKEN_PAREN_LEFT) {
            last_match = ExprKind::FuncCall;
          } else {
            last_match = ExprKind::Literal;
          }; 

          break;
        }

        case (TOKEN_STRING_LITERAL): {
          last_match = ExprKind::Literal;
          break;
        }

        default: {
          last_match = ExprKind::Undefined;
          break;
        }
      }

      break;
    }

    case (TOKEN_CONSTANT): {
      last_match = ExprKind::Constant;
      break;
    }

    case (KEYWORD_FUNC_BEG): {
      last_match = ExprKind::FuncDef;
      break;
    }

    default: {
      last_match = ExprKind::Undefined;
      break;
    }
  }

  return last_match;
}

Expr Parser::parseExpression() {
  switch (this->peek()) {
    case (ExprKind::Literal):
    case (ExprKind::FuncCall): {
      return this->parseVarExpression();
    }

    case (ExprKind::FuncDef): {
      return this->parseFuncExpression();
    }

    case (ExprKind::Constant): {
      return this->parseConstExpression();
    }

    case (ExprKind::BinaryOp): {
      return this->parseBinaryOp();
    }

    case (ExprKind::BooleanOp): {
      return this->parseBooleanOp();
    }

    default: {
      this->pos++;
      return (Expr) {
        .kind = ExprKind::Undefined,
        .node = UndefinedExpr()
      };
    }
  }
}

/*
 * This functions still needs to implement operator precedence
 * but in spirit, this will recursively construct the binrary 
 * operations that are needed in the operators here.
 *
 * I also added symbols are part of the allowed token types.
 * During compilation, we will ensure that each symbol is of
 * the necessary type that we will need to have a valid 
 * binary expression.
*/
Expr Parser::parseBinaryOp() {
  Expr expr;
  auto left = this->tokens[this->pos];
  if (left.tok_type != TOKEN_SYMBOL
      && left.tok_type != TOKEN_FLOAT_LITERAL
      && left.tok_type != TOKEN_INTEGER_LITERAL
  ) {
    expr.kind = ExprKind::Undefined;
    expr.node = UndefinedExpr();
    return expr;
  }

  auto&& left_op_val = (LiteralExpr) {
    .value = (Value) {
      .type = tokToType(left),
      .value = left.tok_val
    }
  };

  BinaryExpr bin_expr;
  bin_expr.left = std::make_unique<Expr>((Expr) {
    .kind = ExprKind::Literal,
    .node = left_op_val
  });

  auto op = this->peekNextTok();
  if (op && isBinaryOp(*op)) {
    this->pos++;
    bin_expr.op = op->tok_val;
    this->pos++;
    bin_expr.right = std::make_unique<Expr>(parseBinaryOp());
  } else {
    expr.kind = ExprKind::BinaryOp;
    expr.node = std::move(bin_expr);
    this->pos++;
  }

  return expr;
}

/*
 * This functions still needs to implement operator precedence
 * but in spirit, this will recursively construct the boolean 
 * operations that are needed in the operators here.
 *
 * I also added symbols are part of the allowed token types.
 * During compilation, we will ensure that each symbol is of
 * the necessary type that we will need to have a valid 
 * binary expression.
*/
Expr Parser::parseBooleanOp() {
  Expr expr;
  auto left = this->tokens[this->pos];
  if (left.tok_type != TOKEN_SYMBOL
      && left.tok_type != TOKEN_FLOAT_LITERAL
      && left.tok_type != TOKEN_INTEGER_LITERAL
  ) {
    expr.kind = ExprKind::Undefined;
    expr.node = UndefinedExpr();
    return expr;
  }

  auto&& left_op_val = (LiteralExpr) {
    .value = (Value) {
      .type = tokToType(left),
      .value = left.tok_val
    }
  };

  BooleanExpr bin_expr;
  bin_expr.left = std::make_unique<Expr>((Expr) {
    .kind = ExprKind::Literal,
    .node = left_op_val
  });

  auto op = this->peekNextTok();
  if (op && isBooleanOp(*op)) {
    this->pos++;
    bin_expr.op = op->tok_val;
    this->pos++;
    bin_expr.right = std::make_unique<Expr>(parseBooleanOp());
  } else {
    expr.kind = ExprKind::BooleanOp;
    expr.node = std::move(bin_expr);
  }

  return expr;
}

Expr Parser::parseFuncExpression() {
  Expr expr = (Expr) {
    .kind = ExprKind::FuncDef,
    .node = UndefinedExpr()
  };

  if (this->tokens[this->pos].tok_type != KEYWORD_FUNC_BEG) {
    std::printf("\nNot FuncBegin Keyword\n");
    return expr;
  }

  auto&& func_symbol = this->peekNextTok();

  if (func_symbol->tok_type != TOKEN_SYMBOL) {
    std::printf("\nNot function symbol Keyword\n");
    return expr;
  }

  this->pos++;

  auto&& param_open = this->peekNextTok();

  if (param_open->tok_type != TOKEN_PAREN_LEFT) {
    std::printf("\nNot function opening parantheses\n");
    return expr;
  }

  this->pos += 2;

  std::vector<Param> params;
  while (this->pos < this->tokens.size() 
    && this->tokens[this->pos].tok_type != TOKEN_PAREN_RIGHT
  ) {
    auto&& name = this->tokens[this->pos];
    auto&& colon = this->tokFromCurrPos(1);
    auto&& type  = this->tokFromCurrPos(2);

    if ( name.tok_type != TOKEN_SYMBOL
        || (!colon || colon->tok_type != TOKEN_COLON 
            || colon->tok_type == TOKEN_PAREN_RIGHT)
        || (!type || !isValidBenzeneType(*type)
            || type->tok_type == TOKEN_PAREN_RIGHT)
    ) {
      std::printf("\nNot function params\n");
      break;
    }

    params.emplace_back((Param) {
      .name = name.tok_val,
      .type = getTypeAnnotation(*type)
    });

    this->pos += 3;

    if (this->pos < this->tokens.size()
        && this->tokens[this->pos].tok_type == TOKEN_DELIM
    ) {
      this->pos++;
      continue;
    }
  }

  this->pos++;

  Type return_type = Type::Undefined;
  if (auto&& t = this->tokens[this->pos]; t.tok_type == TOKEN_RETURN_TYPE) {
    this->pos++;
    if (auto&& n = this->tokens[this->pos]; isValidBenzeneType(n)) {
      return_type = getTypeAnnotation(n);
    }
  }

  if (this->peekNextTok()->tok_type != TOKEN_DELIM) {
    std::printf("\nNot function opening delimter\n");
    return expr;
  }

  std::vector<Expr> func_body;
  while (this->pos < this->tokens.size() 
         && this->tokens[this->pos].tok_type != KEYWORD_FUNC_END
  ) {
    // func_body.emplace_back(this->parseExpression());
    this->pos++;
  }
  this->pos++;

  this->function_registry.assign(func_symbol->tok_val, FuncBooklet((FunctionDef) {
    .name = func_symbol->tok_val,
    .params = params,
    .return_type = return_type
  }, std::make_shared<std::vector<Expr>>(std::move(func_body))));

  expr.kind = ExprKind::FuncDef;
  expr.node = (FuncDefExpr) {
    .func_name = func_symbol->tok_val,
    .def = (FunctionDef) {
      .name = func_symbol->tok_val,
      .params = params,
      .return_type = return_type,
    },
    .body = std::make_unique<std::vector<Expr>>(std::move(func_body))
  };

  return expr;
}

Expr Parser::parseVarExpression() {
  Expr expr = (Expr) {
    .kind = ExprKind::Variable,
    .node = UndefinedExpr()
  };

  Token& current = this->tokens[this->pos];
  auto&& assign  = this->peekNextTok();

  if (!assign) {
    expr.kind = ExprKind::Undefined;
    this->pos++;
    return expr;
  } else if (assign->tok_type != TOKEN_ASSIGN) {
    expr.kind = ExprKind::Undefined;
    this->pos++;
    return expr;
  }

  this->pos += 2;

  auto&& value = this->peek();
  auto&& val_tok = this->peekNextTok();

  if (!val_tok) {
    expr.kind = ExprKind::Undefined;
    return expr;
  }

  if (value == ExprKind::Literal) {
    expr.kind = ExprKind::Literal;
    expr.node = (LiteralExpr) {
      .value = (Value) {
        .type = getTypeAnnotation(*val_tok),
        .value = val_tok->tok_val
      }
    };
    this->pos += 2;
  } else if (value == ExprKind::FuncCall) {
    auto&& def = this->function_registry.getDefinition(val_tok->tok_val);
    expr.kind = ExprKind::FuncCall;
    expr.node = (FuncCallExpr) {
      .callee = val_tok->tok_val,
      .expr   = def.has_value()
        ? def.value().second
        : nullptr
    };
    this->pos += 2;
  }

  this->scope.assign(current.tok_val, (Value) {
    .type  = getTypeAnnotation(*val_tok),
    .value = val_tok->tok_val
  });

  return expr;
}

Expr Parser::parseConstExpression() {
  Expr expr = (Expr) {
    .kind = ExprKind::ConstExpr,
    .node = UndefinedExpr()
  };

  if (this->tokens[this->pos].tok_type != TOKEN_CONSTANT) {
    printf("\nreturning early from const expr parser\n");
    return expr;
  }
  this->pos++;

  Token& current = this->tokens[this->pos];
  auto&& assign  = this->peekNextTok();

  if (!assign) {
    expr.kind = ExprKind::Undefined;
    printf("\nreturning early from const expr parser 1\n");
    return expr;
  } else if (assign->tok_type != TOKEN_ASSIGN) {
    printf("\nreturning early from const expr parser 2\n");
    expr.kind = ExprKind::Undefined;
    return expr;
  }

  if (current.tok_type != TOKEN_SYMBOL) {
    printf("\nreturning early from const expr parser 3\n");
    return expr;
  }

  this->pos += 2;

  auto&& value = this->peek();
  auto&& val_tok = this->tokFromCurrPos(0);

  if (!val_tok) {
    expr.kind = ExprKind::Undefined;
    printf("\nreturning early from const expr parser 4\n");
    return expr;
  } else if (val_tok->tok_type != TOKEN_STRING_LITERAL
             && val_tok->tok_type != TOKEN_INTEGER_LITERAL
             && val_tok->tok_type != TOKEN_FLOAT_LITERAL
  ) {
    expr.kind = ExprKind::Undefined;
    printf("\nreturning early from const expr parser 5\n");
    return expr;
  }

  if (value == ExprKind::Literal) {
    expr.kind = ExprKind::Literal;
    expr.node = (LiteralExpr) {
      .value = (Value) {
        .type = getTypeAnnotation(*val_tok),
        .value = val_tok->tok_val
      }
    };
    this->pos++;
  } else if (value == ExprKind::FuncCall) {
    auto&& def = this->function_registry.getDefinition(val_tok->tok_val);
    expr.kind = ExprKind::FuncCall;
    expr.node = (FuncCallExpr) {
      .callee = val_tok->tok_val,
      .expr   = def.has_value()
        ? def.value().second
        : nullptr
    };
    this->pos++;
  }

  this->scope.assign(current.tok_val, (Value) {
    .type  = getTypeAnnotation(*val_tok),
    .value = val_tok->tok_val
  });

  return expr;
}

std::unique_ptr<Token> Parser::peekNextTok() {
  if (this->pos + 1 >= this->tokens.size())  {
    return nullptr;
  }

  Token tok = this->tokens[this->pos + 1];
  return std::make_unique<Token>(tok);
}

std::unique_ptr<Token> Parser::tokFromCurrPos(size_t idx) {
  if (this->pos + idx >= this->tokens.size()) {
    return nullptr;
  }

  Token tok = this->tokens[this->pos + idx];
  return std::make_unique<Token>(tok);
}

void Parser::parseTokens() {
  std::printf(
    "\nParsing tokens\n"
    "Tokens Size: %zu\n"
    "Current cursor position: %zu\n",
    this->tokens.size(),
    this->pos
  );
  size_t start_pos = this->pos;
  while (this->pos < this->tokens.size()) {
    std::printf("\nBegining token parsing\n");
    auto&& expr = this->parseExpression();
    std::printf("Parsed Expression: %s\n", 
                exprKindToStr(expr.kind));
    this->expressions.push_back(std::move(expr));
    this->pos++;
  }

  if (this->pos == start_pos) {
    this->pos++;
  }
}
