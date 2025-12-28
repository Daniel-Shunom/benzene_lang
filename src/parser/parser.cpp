#include "./parser.hpp"
#include "../types/utils.hpp"
#include <cstddef>
#include <cstdio>
#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>
#include "../../lib/utils.hpp"

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

    case (TOKEN_EOF): {
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

    case (KEYWORD_CASE): {
      this->parseCaseExpression();
    }

    case (TOKEN_FLOAT_LITERAL):
    case (TOKEN_INTEGER_LITERAL): {
      auto&& op = this->peekNextTok();

      if (!op) {
        last_match = ExprKind::Literal;
        break;
      }

      if (isBooleanOp(*op)) {
        last_match = ExprKind::BooleanOp;
        break;
      } else if (isBinaryOp(*op)) {
        last_match = ExprKind::BinaryOp;
        break;
      } 
    }

    case (TOKEN_PAREN_LEFT):
    case (TOKEN_SQUIG_BRAC_LEFT): {
      last_match = ExprKind::ScopeExpr;
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
      break;
    }

    case (ExprKind::FuncDef): {
      return this->parseFuncExpression();
      break;
    }

    case (ExprKind::Constant): {
      return this->parseConstExpression();
      break;
    }

    case (ExprKind::BinaryOp): {
      this->advanceNext();
      this->advanceNext();
      return this->parseBinaryOp();
      break;
    }

    case (ExprKind::BooleanOp): {
      this->advanceNext();
      this->advanceNext();
      return this->parseBooleanOp();
      break;
    }

    default: {
      this->advanceNext();
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
Expr Parser::parseBinaryOp(float min_precedence) {
  Expr expr = (Expr) {
    .kind = ExprKind::BinaryOp,
    .node = UndefinedExpr()
  };

  if (this->pos >= this->tokens.size()) {
    return expr;
  }

  auto left = this->tokens[this->pos];
  if (left.tok_type != TOKEN_SYMBOL
      && left.tok_type != TOKEN_FLOAT_LITERAL
      && left.tok_type != TOKEN_INTEGER_LITERAL
  ) {
    return expr;
  }

  Expr left_expr = (Expr) {
    .kind = ExprKind::Literal,
    .node = (LiteralExpr) {
      .value = (Value) {
        .type = tokToType(left),
        .value = left.tok_val
      }
    }
  };

  BinaryExpr bin_expr;
  while (true) {
    auto&& left_op = this->peekNextTok();
    if (!left_op || !isBinaryOp(*left_op)) {
      break;
    }

    auto [left_prec, right_prec] = this->getBinOpPrecedence(left_op->tok_val);
    if (left_prec < min_precedence) {
      break;
    }

    this->advanceNext();
    this->advanceNext();

    Expr right_expr = this->parseBinaryOp(right_prec);

    left_expr = (Expr) {
      .kind = ExprKind::BinaryOp,
      .node = (BinaryExpr) {
        .op = left_op->tok_val,
        .left  = std::make_shared<Expr>(left_expr),
        .right = std::make_shared<Expr>(right_expr),
      }
    };
  }

  return left_expr;
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
  bin_expr.left = std::make_shared<Expr>((Expr) {
    .kind = ExprKind::Literal,
    .node = left_op_val
  });

  auto op = this->peekNextTok();
  if (op && isBooleanOp(*op)) {
    this->advanceNext();
    bin_expr.op = op->tok_val;
    this->advanceNext();
    bin_expr.right = std::make_shared<Expr>(parseBooleanOp());
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
    return expr;
  }

  auto&& func_symbol = this->peekNextTok();

  if (func_symbol->tok_type != TOKEN_SYMBOL) {
    return expr;
  }

  this->advanceNext();

  auto&& param_open = this->peekNextTok();

  if (param_open->tok_type != TOKEN_PAREN_LEFT) {
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
      this->advanceNext();
      continue;
    }
  }

  this->advanceNext();

  Type return_type = Type::Undefined;
  if (auto&& t = this->tokens[this->pos]; t.tok_type == TOKEN_RETURN_TYPE) {
    this->advanceNext();
    if (auto&& n = this->tokens[this->pos]; isValidBenzeneType(n)) {
      return_type = getTypeAnnotation(n);
    }
  }

  if (this->peekNextTok()->tok_type != TOKEN_DELIM) {
    return expr;
  }

  this->advanceNext();
  std::vector<Expr> func_body;
  auto local_scope = std::make_shared<Env>(this->scope);
  this->advanceNext();

  printf("Current token at cursor: %s\n", this->tokens[this->pos].tok_val.data());
  printf("\n\n\nFUNCTION_BODY_BEGIN\n");
  while (this->pos < this->tokens.size() 
         && this->tokens[this->pos].tok_type != KEYWORD_FUNC_END
  ) {

    auto expr = this->parseExpression();
    if (expr.kind == ExprKind::Undefined) {
      break;
    }

    printf("\nFunction Body Expression"
           "\nExpression type: %s\n", 
           exprKindToStr(expr.kind));

    func_body.push_back(std::move(expr));
    this->advanceNext();
  }
  printf("\nFUNCTION_BODY_END\n\n\n");

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
    .body = std::make_shared<std::vector<Expr>>(std::move(func_body))
  };

  return expr;
}

Expr Parser::parseVarExpression() {
  Expr expr = (Expr) {
    .kind = ExprKind::Variable,
    .node = UndefinedExpr()
  };

  Token& var_name = this->tokens[this->pos];
  auto&& assign  = this->peekNextTok();

  if (!assign) {
    expr.kind = ExprKind::Undefined;
    this->advanceNext();
    return expr;
  } else if (assign->tok_type != TOKEN_ASSIGN) {
    expr.kind = ExprKind::Undefined;
    this->advanceNext();
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
  } else if (value == ExprKind::BinaryOp) {
    expr = this->parseBinaryOp();
  } else if (value == ExprKind::BooleanOp) {
    expr = this->parseBooleanOp();
  }

  this->scope.assign(var_name.tok_val, (Value) {
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
    return expr;
  }

  this->advanceNext();
  Token& const_var_name = this->tokens[this->pos];
  auto&& assign  = this->peekNextTok();

  if (!assign) {
    expr.kind = ExprKind::Undefined;
    return expr;
  } else if (assign->tok_type != TOKEN_ASSIGN) {
    expr.kind = ExprKind::Undefined;
    return expr;
  }

  if (const_var_name.tok_type != TOKEN_SYMBOL) {
    return expr;
  }

  this->pos += 2;
  // auto&& value = this->peek();
  auto&& val_tok = this->tokFromCurrPos(0);

  if (!val_tok) {
    expr.kind = ExprKind::Undefined;
    return expr;
  } else if (val_tok->tok_type != TOKEN_STRING_LITERAL
             && val_tok->tok_type != TOKEN_INTEGER_LITERAL
             && val_tok->tok_type != TOKEN_FLOAT_LITERAL
  ) {
    expr.kind = ExprKind::Undefined;
    return expr;
  }



  this->scope.assign(const_var_name.tok_val, (Value) {
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

Expr Parser::parseCaseExpression() {
  Expr e = (Expr) {
    .kind = ExprKind::CaseExpr,
    .node = UndefinedExpr()
  };

  this->advance();

  if (this->tokens[this->pos].tok_type != TOKEN_PAREN_LEFT) {
    return e;
  }

  this->advance();

  Expr condition = this->parseExpression();

  if (this->tokens[this->pos].tok_type != TOKEN_PAREN_RIGHT
      || this->tokFromCurrPos(1)->tok_type != TOKEN_CASE_OPEN
  ) {
    return e;
  }

  this->advance();
  this->advance();

  // Here, based on the type the condition-expression is to return,we
  // check if the type can have one or more forms. If so, we proceed
  // with the parsing. Otherwise, we emit a warning in the ast that this
  // can be re-written without the use of a case statement.
  while (this->tokens[this->pos].tok_type != TOKEN_CASE_CLOSE) {

  }

  return e;
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
    auto&& expr = this->parseExpression();

    if (expr.kind == ExprKind::Undefined) {
      break;
    }

    if (expr.kind == ExprKind::BinaryOp) {
      printExprTree(expr);
    }

    std::printf("\nParsed Expression: %s\nCurrent value at cursor: %s\n", 
                exprKindToStr(expr.kind),
                this->tokens[this->pos].tok_val.data());

    this->expressions.push_back(std::move(expr));
    this->advanceNext();

    if (this->pos == start_pos) {
      this->advanceNext();
    }
  }
}

std::pair<float, float> Parser::getBinOpPrecedence(std::string& op) {
  if (op == "+" || op == "-") {
    return std::make_pair(1.0, 1.1);
  } else if (op == "*" || op == "/") {
    return std::make_pair(2.0, 2.1);
  } else {
    throw std::runtime_error("Not a valid operator");
  }
}
