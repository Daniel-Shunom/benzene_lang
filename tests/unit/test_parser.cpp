#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/lexer/lexer.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/parser/parser_types.hpp>
#include <ether/parser/parsers.hpp>
#include <ether/tokens/token_types.hpp>

using namespace ether::test;

namespace {

// Helper: lex the source and run the top-level parser on it.
PResult<Parent> parse_source(const std::string& src, DiagnosticEngine& diag) {
  Lexer lex(src, diag);
  lex.scan_tokens();
  ParserState state(diag);
  state.set_state(lex.get_tokens());
  return run_parser(state);
}

template <typename T>
T* as(const NDPtr& n) { return dynamic_cast<T*>(n.get()); }

}  // namespace

TEST_SUITE("parser / literals and identifiers") {
  TEST_CASE("parse_literal accepts integer literal") {
    DiagnosticEngine diag;
    auto state = make_state({ make_tok(TokenType::IntegerLiteral, "42") }, diag);
    auto r = parse_literal()(state);
    REQUIRE(r.has_value());
    CHECK(r->literal.token_value == "42");
  }

  TEST_CASE("parse_literal rejects non-literal") {
    DiagnosticEngine diag;
    auto state = make_state({ make_tok(TokenType::Identifier, "x") }, diag);
    auto r = parse_literal()(state);
    CHECK_FALSE(r.has_value());
  }

  TEST_CASE("parse_identifier accepts identifier") {
    DiagnosticEngine diag;
    auto state = make_state({ make_tok(TokenType::Identifier, "foo") }, diag);
    auto r = parse_identifier()(state);
    REQUIRE(r.has_value());
    CHECK(r->identifier.token_value == "foo");
  }
}

TEST_SUITE("parser / let bindings") {
  TEST_CASE("simple let with literal") {
    DiagnosticEngine diag;
    auto p = parse_source("let x = 1", diag);
    REQUIRE(p.has_value());
    REQUIRE(p->children.size() == 1);
    auto* let = as<NDLetBindExpr>(p->children[0]);
    REQUIRE(let);
    CHECK(let->identifier->identifier.token_value == "x");
    CHECK_FALSE(let->is_poisoned);
  }

  TEST_CASE("let with type annotation") {
    DiagnosticEngine diag;
    auto p = parse_source("let x: Int = 1", diag);
    REQUIRE(p.has_value());
    auto* let = as<NDLetBindExpr>(p->children[0]);
    REQUIRE(let);
    REQUIRE(let->type.has_value());
    CHECK(let->type->token_value == "Int");
  }

  TEST_CASE("let missing `=` reports parser diagnostic") {
    DiagnosticEngine diag;
    auto p = parse_source("let x 1", diag);
    REQUIRE(p.has_value());
    CHECK(diag.has_errors());
  }
}

TEST_SUITE("parser / const expressions") {
  TEST_CASE("const literal succeeds") {
    DiagnosticEngine diag;
    auto p = parse_source("const x = 10", diag);
    REQUIRE(p.has_value());
    REQUIRE(p->children.size() == 1);
    auto* c = as<NDConstExpr>(p->children[0]);
    REQUIRE(c);
    CHECK(c->literal.literal.token_value == "10");
  }

  TEST_CASE("const with non-literal RHS fails") {
    DiagnosticEngine diag;
    auto p = parse_source("const x = y", diag);
    // run_parser may still produce a Parent, but the const node should be
    // absent and the diag engine should have an error reported.
    REQUIRE(p.has_value());
    CHECK(diag.has_errors());
  }

  TEST_CASE("const with type annotation") {
    DiagnosticEngine diag;
    auto p = parse_source("const greeting: String = \"hi\"", diag);
    REQUIRE(p.has_value());
    auto* c = as<NDConstExpr>(p->children[0]);
    REQUIRE(c);
    REQUIRE(c->type.has_value());
    CHECK(c->type->token_value == "String");
  }
}

TEST_SUITE("parser / binary expressions and precedence") {
  TEST_CASE("multiplication binds tighter than addition") {
    DiagnosticEngine diag;
    auto p = parse_source("let x = 1 + 2 * 3", diag);
    REQUIRE(p.has_value());
    auto* let = as<NDLetBindExpr>(p->children[0]);
    REQUIRE(let);
    auto* add = dynamic_cast<NDBinaryExpr*>(let->bound_value.get());
    REQUIRE(add);
    CHECK(add->op.token_type == TokenType::PlusOp);
    auto* rhs = dynamic_cast<NDBinaryExpr*>(add->rhs.get());
    REQUIRE(rhs);
    CHECK(rhs->op.token_type == TokenType::MultiplyOp);
  }

  TEST_CASE("equal-precedence operators chain left") {
    DiagnosticEngine diag;
    auto p = parse_source("let x = 1 - 2 - 3", diag);
    REQUIRE(p.has_value());
    auto* let = as<NDLetBindExpr>(p->children[0]);
    REQUIRE(let);
    auto* outer = dynamic_cast<NDBinaryExpr*>(let->bound_value.get());
    REQUIRE(outer);
    CHECK(outer->op.token_type == TokenType::MinusOp);
    auto* inner = dynamic_cast<NDBinaryExpr*>(outer->lhs.get());
    REQUIRE(inner);
    CHECK(inner->op.token_type == TokenType::MinusOp);
  }

  TEST_CASE("unary minus on primary produces NDUnaryExpr") {
    DiagnosticEngine diag;
    auto p = parse_source("let x = -5", diag);
    REQUIRE(p.has_value());
    auto* let = as<NDLetBindExpr>(p->children[0]);
    REQUIRE(let);
    auto* u = dynamic_cast<NDUnaryExpr*>(let->bound_value.get());
    REQUIRE(u);
    REQUIRE(u->op.has_value());
    CHECK(u->op->token_type == TokenType::MinusOp);
  }

  TEST_CASE("logical operators below comparisons") {
    DiagnosticEngine diag;
    auto p = parse_source("let x = 1 < 2 && 3 > 0", diag);
    REQUIRE(p.has_value());
    auto* let = as<NDLetBindExpr>(p->children[0]);
    REQUIRE(let);
    auto* andn = dynamic_cast<NDBinaryExpr*>(let->bound_value.get());
    REQUIRE(andn);
    CHECK(andn->op.token_type == TokenType::AndOp);
    CHECK(dynamic_cast<NDBinaryExpr*>(andn->lhs.get())->op.token_type == TokenType::Lt);
    CHECK(dynamic_cast<NDBinaryExpr*>(andn->rhs.get())->op.token_type == TokenType::Gt);
  }
}

TEST_SUITE("parser / scoped expressions") {
  TEST_CASE("scope contains nested expressions") {
    DiagnosticEngine diag;
    auto p = parse_source("let x = { 1 + 2 }", diag);
    REQUIRE(p.has_value());
    auto* let = as<NDLetBindExpr>(p->children[0]);
    REQUIRE(let);
    auto* scope = dynamic_cast<NDScopeExpr*>(let->bound_value.get());
    REQUIRE(scope);
    CHECK(scope->expressions.size() == 1);
  }
}

TEST_SUITE("parser / function declarations") {
  TEST_CASE("zero-arg function with body") {
    DiagnosticEngine diag;
    auto p = parse_source("func f()\n  1\nend", diag);
    REQUIRE(p.has_value());
    REQUIRE(p->children.size() == 1);
    auto* fn = as<NDFuncDeclExpr>(p->children[0]);
    REQUIRE(fn);
    CHECK(fn->func_identifier.token_value == "f");
    CHECK(fn->func_params.empty());
    CHECK(fn->func_body.size() == 1);
  }

  TEST_CASE("function with typed params and return type") {
    DiagnosticEngine diag;
    auto p = parse_source("func add(a: Int, b: Int) :> Int\n  a\nend", diag);
    REQUIRE(p.has_value());
    auto* fn = as<NDFuncDeclExpr>(p->children[0]);
    REQUIRE(fn);
    CHECK(fn->func_params.size() == 2);
    REQUIRE(fn->func_params[0].param_type.has_value());
    CHECK(fn->func_params[0].param_type->token_value == "Int");
    REQUIRE(fn->return_type.has_value());
    CHECK(fn->return_type->token_value == "Int");
  }

  TEST_CASE("missing closing paren reports a diagnostic") {
    DiagnosticEngine diag;
    auto p = parse_source("func bad(a end", diag);
    CHECK(diag.has_errors());
  }
}

TEST_SUITE("parser / call expressions and pipe chains") {
  TEST_CASE("call expression with no args") {
    DiagnosticEngine diag;
    auto state = make_state({
      make_tok(TokenType::Identifier, "f"),
      make_tok(TokenType::LParen, "("),
      make_tok(TokenType::RParen, ")"),
    }, diag);
    auto r = parse_call_expression()(state);
    REQUIRE(r.has_value());
    CHECK(r->identifier->identifier.token_value == "f");
    CHECK(r->args.empty());
  }

  TEST_CASE("call expression with two args") {
    DiagnosticEngine diag;
    auto state = make_state({
      make_tok(TokenType::Identifier, "f"),
      make_tok(TokenType::LParen, "("),
      make_tok(TokenType::IntegerLiteral, "1"),
      make_tok(TokenType::Delim, ","),
      make_tok(TokenType::IntegerLiteral, "2"),
      make_tok(TokenType::RParen, ")"),
    }, diag);
    auto r = parse_call_expression()(state);
    REQUIRE(r.has_value());
    CHECK(r->args.size() == 2);
  }

  TEST_CASE("pipe chain produces NDCallChain") {
    DiagnosticEngine diag;
    auto p = parse_source("a() |=> b() |=> c()", diag);
    REQUIRE(p.has_value());
    REQUIRE(p->children.size() == 1);
    auto* chain = dynamic_cast<NDCallChain*>(p->children[0].get());
    REQUIRE(chain);
    CHECK(chain->calls.size() == 3);
  }
}

TEST_SUITE("parser / case expressions") {
  TEST_CASE("simple case expression with one branch") {
    DiagnosticEngine diag;
    auto p = parse_source("case 1 :\n  2 :> 3\nend", diag);
    REQUIRE(p.has_value());
    REQUIRE(p->children.size() == 1);
    auto* c = dynamic_cast<NDCaseExpr*>(p->children[0].get());
    REQUIRE(c);
    CHECK(c->conditions.size() == 1);
    CHECK(c->branches.size() == 1);
  }

  TEST_CASE("case without colon reports an error") {
    DiagnosticEngine diag;
    auto p = parse_source("case 1\n  2 :> 3\nend", diag);
    CHECK(diag.has_errors());
  }
}

TEST_SUITE("parser / recovery") {
  TEST_CASE("a stray top-level token does not abort the rest of the program") {
    DiagnosticEngine diag;
    // Leading garbage `}` followed by a valid declaration.
    auto p = parse_source("} let x = 1", diag);
    REQUIRE(p.has_value());
    // The let still parses despite the leading garbage.
    bool found_let = false;
    for (auto& c : p->children) {
      if (dynamic_cast<NDLetBindExpr*>(c.get())) found_let = true;
    }
    CHECK(found_let);
  }
}
