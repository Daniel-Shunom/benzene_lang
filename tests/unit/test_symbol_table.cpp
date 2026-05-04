#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/symbols/scopes.hpp>
#include <ether/symbols/symbol_types.hpp>
#include <ether/symbols/symtable.hpp>
#include <ether/tokens/token_types.hpp>

using namespace ether::test;

TEST_SUITE("symbols / SymbolTable") {
  TEST_CASE("constructor seeds a Module scope") {
    SymbolStorage arena;
    SymbolTable table(arena);
    auto t = table.get_current_scope_type();
    REQUIRE(t.has_value());
    CHECK(*t == ScopeType::Module);
  }

  TEST_CASE("declare allocates and lookup retrieves") {
    SymbolStorage arena;
    SymbolTable table(arena);
    auto tok = make_tok(TokenType::Identifier, "x");
    auto* sym = table.declare(tok, SymbolKind::Binding);
    REQUIRE(sym != nullptr);
    CHECK(sym->name == "x");
    CHECK(sym->symbol_kind == SymbolKind::Binding);
    CHECK(table.lookup("x") == sym);
  }

  TEST_CASE("declare returns null on duplicate in same scope") {
    SymbolStorage arena;
    SymbolTable table(arena);
    auto tok = make_tok(TokenType::Identifier, "y");
    REQUIRE(table.declare(tok, SymbolKind::Binding) != nullptr);
    CHECK(table.declare(tok, SymbolKind::Binding) == nullptr);
  }

  TEST_CASE("inner scope shadows outer; pop restores outer") {
    SymbolStorage arena;
    SymbolTable table(arena);
    auto tok_outer = make_tok(TokenType::Identifier, "n", 1, 1);
    auto* outer = table.declare(tok_outer, SymbolKind::Binding);
    REQUIRE(outer);

    table.new_scope(ScopeType::FunctionExpression);
    auto tok_inner = make_tok(TokenType::Identifier, "n", 5, 1);
    auto* inner = table.declare(tok_inner, SymbolKind::Binding);
    REQUIRE(inner);
    CHECK(inner != outer);
    CHECK(table.lookup("n") == inner);
    CHECK(*table.get_current_scope_type() == ScopeType::FunctionExpression);

    table.pop_scope();
    CHECK(table.lookup("n") == outer);
    CHECK(*table.get_current_scope_type() == ScopeType::Module);
  }

  TEST_CASE("ScopeGuard pushes on construction and pops on destruction") {
    SymbolStorage arena;
    SymbolTable table(arena);
    {
      ScopeGuard g(table, ScopeType::ScopedExpression);
      CHECK(*table.get_current_scope_type() == ScopeType::ScopedExpression);
    }
    CHECK(*table.get_current_scope_type() == ScopeType::Module);
  }

  TEST_CASE("SymbolStorage tracks total allocations") {
    SymbolStorage arena;
    SymbolTable table(arena);
    CHECK(arena.size() == 0);
    table.declare(make_tok(TokenType::Identifier, "a"), SymbolKind::Binding);
    table.declare(make_tok(TokenType::Identifier, "b"), SymbolKind::Binding);
    CHECK(arena.size() == 2);
  }
}
