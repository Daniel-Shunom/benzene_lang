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
    REQUIRE(let->identifier->type.has_value());
    CHECK(std::get<TypeConstructor>(let->identifier->type->parsed_type->value).name() == "Int");
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
    auto* literal = dynamic_cast<NDLiteral*>(c->bound_value.get());
    REQUIRE(literal != nullptr);
    CHECK(literal->literal.token_value == "10");
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
    REQUIRE(c->identifier->type.has_value());
    CHECK(std::get<TypeConstructor>(c->identifier->type->parsed_type->value).name() == "String");
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
    CHECK(std::get<TypeConstructor>(fn->func_params[0].param_type->parsed_type->value).name() == "Int");
    REQUIRE(fn->return_type.has_value());
    CHECK(std::get<TypeConstructor>(fn->return_type->parsed_type->value).name() == "Int");
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

TEST_SUITE("parser / imports") {
  TEST_CASE("Load directive parses into NDImportDirective") {
    DiagnosticEngine diag;
    auto p = parse_source("Load benzene.list", diag);
    REQUIRE(p.has_value());
    REQUIRE(p->children.size() == 1);
    auto* imp = dynamic_cast<NDImportDirective*>(p->children[0].get());
    REQUIRE(imp);
    CHECK(imp->import_directive.token_value == "benzene.list");
    CHECK_FALSE(diag.has_errors());
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


TEST_SUITE("parser / type expressions") {
  TEST_CASE("tagged types retain their own names and consume closing parentheses") {
    DiagnosticEngine diag;
    auto state = make_state(lex_all("First(value: Int) Second(item: List(String))", diag), diag);
    for (const auto* name : {"First", "Second"}) {
      auto result = parse_type_expression()(state);
      REQUIRE(result);
      REQUIRE(result->parsed_type);
      const auto& type = std::get<TypeConstructor>(result->parsed_type->value);
      CHECK(type.name() == name);
      REQUIRE(type.get_args().size() == 1);
    }
    REQUIRE(state.peek());
    CHECK(state.peek()->token_type == TokenType::EoF);
  }

  TEST_CASE("function types preserve input and nested return types") {
    DiagnosticEngine diag;
    auto state = make_state(lex_all("Fn(String) :> List(Int)", diag), diag);
    auto result = parse_type_expression()(state);
    REQUIRE(result);
    const auto& fn = std::get<FunctionType>(result->parsed_type->value);
    REQUIRE(fn.get_param_types().size() == 1);
    CHECK(std::get<TypeConstructor>(fn.get_param_types()[0]->value).name() == "String");
    const auto& output = std::get<TypeConstructor>(fn.get_return_type()->value);
    CHECK(output.name() == "List");
    REQUIRE(output.get_args().size() == 1);
    CHECK(std::get<TypeConstructor>(output.get_args()[0]->value).name() == "Int");
    REQUIRE(state.peek());
    CHECK(state.peek()->token_type == TokenType::EoF);
  }

  TEST_CASE("malformed type expressions fail and rewind without throwing") {
    for (const auto* source : {"Pair(a: Int b: String)", "Pair(a: Int", "Pair(a:)",
                               "Fn(String :> Int", "Fn(String) Int", "Fn(String) :>"}) {
      CAPTURE(std::string(source));
      DiagnosticEngine diag;
      auto state = make_state(lex_all(source, diag), diag);
      CHECK_FALSE(parse_type_expression()(state));
      CHECK(state.pos == 0);
    }
  }

  TEST_CASE("returned tagged parsers own their names independently") {
    auto first = h_parse_type_expr_ident_wtagged_params("First");
    auto second = h_parse_type_expr_ident_wtagged_params("Second");
    DiagnosticEngine diag;
    auto first_state = make_state(lex_all("value: Int)", diag), diag);
    auto second_state = make_state(lex_all("value: Int)", diag), diag);
    auto a = first(first_state);
    auto b = second(second_state);
    REQUIRE(a);
    REQUIRE(b);
    CHECK(std::get<TypeConstructor>((*a)->value).name() == "First");
    CHECK(std::get<TypeConstructor>((*b)->value).name() == "Second");
  }

  TEST_CASE("unterminated type bodies fail without hanging or registering a type") {
    DiagnosticEngine diag;
    auto state = make_state(lex_all("type Example { Variant", diag), diag);
    CHECK_FALSE(parse_type_declaration()(state));
    CHECK(state.pos == 0);
    CHECK(state.type_collections.empty());
  }
}

TEST_CASE("supported type expression forms consume exactly one expression") {
  for (const auto* source : {"Int", "Custom", "List(Int)", "List(List(Int))", "Empty()",
                             "Pair(left: Int, right: String)", "Pair(left: Int,)",
                             "Handler(callback: Fn(Int) :> String)",
                             "Fn(Fn(Int) :> String) :> Fn(String) :> Int"}) {
    CAPTURE(std::string(source));
    DiagnosticEngine diag;
    auto state = make_state(lex_all(source, diag), diag);
    REQUIRE(parse_type_expression()(state));
    REQUIRE(state.peek());
    CHECK(state.peek()->token_type == TokenType::EoF);
  }
}


TEST_SUITE("parser / aliases and type arguments") {
  TEST_CASE("multiple arguments preserve recursive structure") {
    DiagnosticEngine diag;
    auto state = make_state(lex_all("Dict(String, List(Fn(Int, Bool) :> String))", diag), diag);
    auto result = parse_type_expression()(state);
    REQUIRE(result);
    const auto& dict = std::get<TypeConstructor>(result->parsed_type->value);
    CHECK(dict.name() == "Dict");
    REQUIRE(dict.get_args().size() == 2);
    CHECK(std::get<TypeConstructor>(dict.get_args()[0]->value).name() == "String");
    const auto& list = std::get<TypeConstructor>(dict.get_args()[1]->value);
    REQUIRE(list.get_args().size() == 1);
    const auto& fn = std::get<FunctionType>(list.get_args()[0]->value);
    REQUIRE(fn.get_param_types().size() == 2);
    CHECK(std::get<TypeConstructor>(fn.get_param_types()[0]->value).name() == "Int");
    CHECK(std::get<TypeConstructor>(fn.get_param_types()[1]->value).name() == "Bool");
    CHECK(std::get<TypeConstructor>(fn.get_return_type()->value).name() == "String");
    CHECK(state.peek()->token_type == TokenType::EoF);
  }

  TEST_CASE("aliases retain names and targets and leave the following declaration") {
    DiagnosticEngine diag;
    auto parent = parse_source("type Converter = Fn(String) :> List(Int) type Mapping = Dict(String, Int) type Bare", diag);
    REQUIRE(parent);
    REQUIRE(parent->children.size() == 3);
    auto* converter = as<NDTypeDecl>(parent->children[0]);
    auto* mapping = as<NDTypeDecl>(parent->children[1]);
    auto* bare = as<NDTypeDecl>(parent->children[2]);
    REQUIRE(converter);
    REQUIRE(mapping);
    REQUIRE(bare);
    CHECK(converter->type_identifier.token_value == "Converter");
    REQUIRE(converter->alias_target);
    CHECK(std::holds_alternative<FunctionType>(converter->alias_target->parsed_type->value));
    REQUIRE(mapping->alias_target);
    CHECK(std::get<TypeConstructor>(mapping->alias_target->parsed_type->value).name() == "Dict");
    CHECK_FALSE(bare->alias_target);
    CHECK_FALSE(diag.has_errors());
  }

  TEST_CASE("zero argument functions and trailing commas parse") {
    for (const auto* source : {"Fn() :> Int", "Fn(Int, String,) :> Bool", "Dict(String, Int,)",
                               "Pair(callback: Fn() :> Int, value: List(Int))"}) {
      CAPTURE(std::string(source));
      DiagnosticEngine diag;
      auto state = make_state(lex_all(source, diag), diag);
      REQUIRE(parse_type_expression()(state));
      CHECK(state.peek()->token_type == TokenType::EoF);
    }
  }

  TEST_CASE("malformed argument lists and aliases fail transactionally") {
    for (const auto* source : {"type A =", "type A = Dict(Int String)", "type A = Dict(Int,, String)",
                               "type A = Dict(Int,", "type A = Fn(Int Bool) :> String",
                               "type A = Fn(Int) :>", "type A = Dict(key: Int, String)"}) {
      CAPTURE(std::string(source));
      DiagnosticEngine diag;
      auto state = make_state(lex_all(source, diag), diag);
      CHECK_FALSE(parse_type_declaration()(state));
      CHECK(state.pos == 0);
      CHECK(state.type_collections.empty());
      CHECK(diag.has_errors());
    }
  }
}


TEST_SUITE("parser / type diagnostics") {
  TEST_CASE("type failures report one specific diagnostic at the offending token") {
    struct Example { const char* source; const char* message; const char* offending; };
    for (const auto& example : {
      Example{"type = Int", "Expected a type name after 'type'", "="},
      Example{"type A =", "Expected a type expression after '='", ""},
      Example{"type A = List(Int String)", "Expected ',' or ')' after type argument", "String"},
      Example{"type A = Pair(a: Int b: String)", "Expected ',' or ')' after labelled type argument", "b"},
      Example{"type A = Pair(a: Int, b String)", "Expected ':' after type argument label", "String"},
      Example{"type A = Pair(a:)", "Expected a type expression after field label and ':'", ")"},
      Example{"type A = Pair(a: Int, 2)", "Expected a field name or ')' in labelled type arguments", "2"},
      Example{"type A = List(,)", "Expected a type argument or closing ')'", ","},
      Example{"type A = Fn Int", "Expected '(' after 'Fn'", "Int"},
      Example{"type A = Fn(Int String) :> Int", "Expected ',' or ')' after function parameter type", "String"},
      Example{"type A = Fn(,) :> Int", "Expected a parameter type or ')' in function type", ","},
      Example{"type A = Fn(Int) Int", "Expected ':>' before function return type", "Int"},
      Example{"type A = Fn(Int) :>", "Expected a return type after ':>'", ""},
      Example{"type A {", "Expected '}' to close the type declaration", ""},
      Example{"type A { 123 }", "Expected a subtype expression or '}' in type declaration", "123"}
    }) {
      CAPTURE(std::string(example.source));
      DiagnosticEngine diag;
      auto state = make_state(lex_all(example.source, diag), diag);
      CHECK_FALSE(parse_type_declaration()(state));
      CHECK(state.pos == 0);
      REQUIRE(diag.all().size() == 1);
      const auto& error = diag.all().front();
      CHECK(error.message == example.message);
      CHECK(error.phase == DiagnosticPhase::Parser);
      CHECK(error.level == DiagnosticLevel::Fail);
      // Use the last occurrence: parameter and return types may share a name.
      const Token* offending = nullptr;
      for (const auto& token : state.tokens) {
        if ((std::string(example.offending).empty() && token.token_type == TokenType::EoF) ||
            (!std::string(example.offending).empty() && token.token_value == example.offending)) {
          offending = &token;
        }
      }
      REQUIRE(offending);
      CHECK(error.location.line == offending->line_number);
      CHECK(error.location.column == offending->column_number);
    }
  }

  TEST_CASE("valid alternatives and unrelated parser probes emit no diagnostics") {
    for (const auto* source : {"List(Int)", "Dict(String, Int)", "Pair(a: Int, b: Fn() :> Int)", "Empty()"}) {
      DiagnosticEngine diag;
      auto state = make_state(lex_all(source, diag), diag);
      REQUIRE(parse_type_expression()(state));
      CHECK(diag.all().empty());
    }
    DiagnosticEngine diag;
    auto state = make_state(lex_all("123", diag), diag);
    CHECK_FALSE(parse_type_declaration()(state));
    CHECK_FALSE(parse_type_expression()(state));
    CHECK_FALSE(parse_type_annotation()(state));
    CHECK(diag.all().empty());
  }

  TEST_CASE("type diagnostics handle physical end of stream and annotation failures") {
    for (const auto* source : {"type A =", "type A = Fn(Int) :>"}) {
      DiagnosticEngine diag;
      ParserState state(diag);
      state.set_state(lex_no_eof(source));
      CHECK_FALSE(parse_type_declaration()(state));
      REQUIRE(diag.all().size() == 1);
      CHECK(diag.all()[0].location.line == 1);
      CHECK(diag.all()[0].location.column > 0);
    }
    DiagnosticEngine diag;
    auto state = make_state(lex_all(": 123", diag), diag);
    CHECK_FALSE(parse_type_annotation()(state));
    REQUIRE(diag.all().size() == 1);
    CHECK(diag.all()[0].message == "Expected a type expression after ':'");
  }

  TEST_CASE("a malformed alias reports its inner error and recovers to the next declaration") {
    DiagnosticEngine diag;
    auto parent = parse_source("type Bad = List(Int String) type Good = Int", diag);
    REQUIRE(parent);
    REQUIRE(parent->children.size() == 1);
    auto* good = as<NDTypeDecl>(parent->children.front());
    REQUIRE(good);
    CHECK(good->type_identifier.token_value == "Good");
    REQUIRE(diag.all().size() == 1);
    CHECK(diag.all()[0].message == "Expected ',' or ')' after type argument");
  }
}


TEST_SUITE("parser / expression annotations") {
  TEST_CASE("functions and lambdas retain compound parameter annotations") {
    for (const auto* source : {"func f(items: Dict(String, Int), callback: Fn(Int) :> String, plain) plain end",
                               "Fn(items: Dict(String, Int), callback: Fn(Int) :> String, plain) plain end"}) {
      DiagnosticEngine diag;
      auto state = make_state(lex_all(source, diag), diag);
      auto result = parse_expression()(state);
      REQUIRE(result);
      auto* function = dynamic_cast<NDFuncDeclExpr*>(result->get());
      auto* lambda = dynamic_cast<NDLambdaExpr*>(result->get());
      REQUIRE((function || lambda));
      const auto& params = function ? function->func_params : lambda->func_params;
      REQUIRE(params.size() == 3);
      REQUIRE(params[0].param_type);
      REQUIRE(params[1].param_type);
      CHECK_FALSE(params[2].param_type);
      const auto& dict = std::get<TypeConstructor>(params[0].param_type->parsed_type->value);
      CHECK(dict.name() == "Dict");
      CHECK(dict.get_args().size() == 2);
      CHECK(std::holds_alternative<FunctionType>(params[1].param_type->parsed_type->value));
      CHECK(diag.all().empty());
    }
  }

  TEST_CASE("binding annotations retain compound types") {
    DiagnosticEngine diag;
    auto state = make_state(lex_all("let items: List(Int) = [1]", diag), diag);
    auto result = parse_let_expression()(state);
    REQUIRE(result);
    REQUIRE(result->identifier->type);
    CHECK(std::get<TypeConstructor>(result->identifier->type->parsed_type->value).name() == "List");
    CHECK(diag.all().empty());
  }

  TEST_CASE("malformed parameter annotations emit one diagnostic and fail") {
    for (const auto* source : {"func f(x: ) x end", "Fn(x: List(Int String)) x end"}) {
      DiagnosticEngine diag;
      auto state = make_state(lex_all(source, diag), diag);
      if (state.peek()->token_type == TokenType::FuncStart) {
        CHECK_FALSE(parse_function_declaration()(state));
      } else {
        CHECK_FALSE(parse_lambda_expression()(state));
      }
      CHECK(state.pos == 0);
      REQUIRE(diag.all().size() == 1);
    }
  }
}


TEST_SUITE("parser / multivariate declarations") {
  TEST_CASE("declarations retain ordered subtype expressions and consume their braces") {
    DiagnosticEngine diag;
    auto state = make_state(lex_all(
      "type Data { Integer String Custom(value: Int, desc: String) "
      "Dict(String, List(Int)) Fn(Int) :> String } type Next", diag), diag);
    auto result = parse_type_declaration()(state);
    REQUIRE(result);
    REQUIRE(result->sub_types);
    REQUIRE(result->sub_types->size() == 5);
    CHECK_FALSE(result->alias_target);
    const auto& members = *result->sub_types;
    CHECK(std::get<TypeConstructor>(members[0].parsed_type->value).name() == "Integer");
    CHECK(std::get<TypeConstructor>(members[1].parsed_type->value).name() == "String");
    const auto& custom = std::get<TypeConstructor>(members[2].parsed_type->value);
    CHECK(custom.name() == "Custom");
    REQUIRE(custom.get_args().size() == 2);
    CHECK(std::get<TypeConstructor>(custom.get_args()[0]->value).name() == "value");
    CHECK(std::get<TypeConstructor>(members[3].parsed_type->value).name() == "Dict");
    CHECK(std::holds_alternative<FunctionType>(members[4].parsed_type->value));
    REQUIRE(state.type_collections.size() == 1);
    const auto& nominal = std::get<TypeConstructor>(state.type_collections[0]->value);
    CHECK(nominal.name() == "Data");
    CHECK(nominal.get_args().empty());
    auto next = parse_type_declaration()(state);
    REQUIRE(next);
    CHECK(next->type_identifier.token_value == "Next");
    CHECK_FALSE(next->sub_types);
    CHECK(diag.all().empty());
  }

  TEST_CASE("empty and commented bodies parse without phantom members") {
    for (const auto* source : {"type Empty {}", "type Data { Cmt note\n Item Cmt { explanation } Other }"}) {
      DiagnosticEngine diag;
      auto state = make_state(lex_all(source, diag), diag);
      auto result = parse_type_declaration()(state);
      REQUIRE(result);
      REQUIRE(result->sub_types);
      CHECK(result->sub_types->size() == (std::string(source).find("Empty") != std::string::npos ? 0 : 2));
      CHECK(diag.all().empty());
      CHECK(state.peek()->token_type == TokenType::EoF);
    }
  }

  TEST_CASE("bad subtype bodies rewind and never register a partial definition") {
    for (const auto* source : {"type Data { List(Int String) }", "type Data { Item", "type Data { 1 }",
                               "type Data { Fn(Int) :> }", "type Data { Item type Next"}) {
      CAPTURE(std::string(source));
      DiagnosticEngine diag;
      ParserState state(diag);
      state.set_state(lex_no_eof(source));
      CHECK_FALSE(parse_type_declaration()(state));
      CHECK(state.pos == 0);
      CHECK(state.type_collections.empty());
      REQUIRE(diag.all().size() == 1);
    }
  }

  TEST_CASE("a missing brace recovers at the next declaration") {
    DiagnosticEngine diag;
    auto parent = parse_source("type Bad { Item type Good { Value }", diag);
    REQUIRE(parent);
    REQUIRE(parent->children.size() == 1);
    auto* good = as<NDTypeDecl>(parent->children[0]);
    REQUIRE(good);
    CHECK(good->type_identifier.token_value == "Good");
    REQUIRE(good->sub_types);
    CHECK(good->sub_types->size() == 1);
    REQUIRE(diag.all().size() == 1);
    CHECK(diag.all()[0].message == "Expected '}' to close the type declaration");
  }
}
