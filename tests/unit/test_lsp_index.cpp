#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/ast_passes/lsp_index/lsp_index.hpp>
#include <ether/ast_passes/scope_resolution/scope_res.hpp>
#include <ether/ast_passes/symbol_resolver/symbol_resolver.hpp>
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ether/module/module.hpp>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace ether::test;

namespace {

// Runs the same pipeline `ether scan` does, and hands back the index. The
// Module has to outlive the entries: they hold positions copied out of it, but
// the pass itself walks nodes the Module owns.
struct Indexed {
  std::unique_ptr<Module> module;
  std::unique_ptr<TypeChecker> checker;
  std::vector<IndexEntry> entries;
};

Indexed index_source(const std::string& source) {
  Indexed result;
  result.module = std::make_unique<Module>("<test>", source);
  result.module->generate_ast();

  ScopeRes scopes(result.module->get_symbol_storage(),
                  result.module->get_diag_engine());
  SymbolResolver resolver(result.module->get_symbol_storage(),
                          result.module->get_diag_engine());
  result.checker =
    std::make_unique<TypeChecker>(false, result.module->get_diag_engine());

  result.module->attach_visitor(resolver);
  result.module->attach_visitor(scopes);
  result.module->attach_visitor(*result.checker);
  result.module->apply_visitors();
  result.checker->unify_constraints();

  LspIndexer indexer(&result.checker->substitutions(), result.checker.get());
  result.module->apply_visitor(indexer);
  result.entries = indexer.entries();
  return result;
}

// The declaration of `name`, if the pass recorded one.
std::optional<IndexEntry> declaration(const std::vector<IndexEntry>& entries,
                                      const std::string& name) {
  auto found = std::find_if(entries.begin(), entries.end(),
    [&](const IndexEntry& entry) {
      return entry.name == name && entry.is_definition;
    });
  if (found == entries.end()) {
    return std::nullopt;
  }
  return *found;
}

size_t occurrences(const std::vector<IndexEntry>& entries,
                   const std::string& name) {
  return static_cast<size_t>(std::count_if(entries.begin(), entries.end(),
    [&](const IndexEntry& entry) { return entry.name == name; }));
}

}  // namespace

TEST_SUITE("lsp index / positions") {
  TEST_CASE("a declaration points at itself") {
    auto indexed = index_source("const greeting: String = \"hello\"\n");
    auto entry = declaration(indexed.entries, "greeting");
    REQUIRE(entry.has_value());

    CHECK(entry->line == 1);
    CHECK(entry->column == 7);
    CHECK(entry->length == 8);
    CHECK(entry->def_line == entry->line);
    CHECK(entry->def_column == entry->column);
  }

  TEST_CASE("a use points back at its declaration") {
    auto indexed = index_source(
      "func identity(x: Int) :> Int\n"
      "  x\n"
      "end\n");

    REQUIRE(occurrences(indexed.entries, "x") == 2);
    const auto& entries = indexed.entries;
    auto use = std::find_if(entries.begin(), entries.end(),
      [](const IndexEntry& entry) {
        return entry.name == "x" && !entry.is_definition;
      });
    REQUIRE(use != entries.end());

    // The parameter is declared on line 1; this occurrence is on line 2.
    CHECK(use->line == 2);
    CHECK(use->def_line == 1);
    CHECK(use->def_column == 15);
  }

  TEST_CASE("an unresolvable name carries no declaration site") {
    // Go-to-definition and rename both key off this being zero.
    auto indexed = index_source(
      "func caller()\n"
      "  nowhere(1)\n"
      "end\n");

    const auto& entries = indexed.entries;
    auto found = std::find_if(entries.begin(), entries.end(),
      [](const IndexEntry& entry) { return entry.name == "nowhere"; });
    REQUIRE(found != entries.end());
    CHECK(found->def_line == 0);
    CHECK(found->def_column == 0);
  }
}

TEST_SUITE("lsp index / types") {
  TEST_CASE("declarations carry their solved type") {
    auto indexed = index_source(
      "func identity(x: Int) :> Int\n"
      "  x\n"
      "end\n"
      "\n"
      "func caller()\n"
      "  let result = identity(42)\n"
      "  result\n"
      "end\n");

    auto binding = declaration(indexed.entries, "result");
    REQUIRE(binding.has_value());
    // Never written down anywhere: inference is what produced this.
    CHECK(binding->type == "Int");
  }

  TEST_CASE("a function renders a usable signature") {
    auto indexed = index_source(
      "func identity(x: Int) :> Int\n"
      "  x\n"
      "end\n");

    auto entry = declaration(indexed.entries, "identity");
    REQUIRE(entry.has_value());
    CHECK(entry->detail == "identity(x: Int) :> Int");
  }

  TEST_CASE("an inferred return type still reaches the signature") {
    // No `:>` on the declaration, so this comes from unification.
    auto indexed = index_source(
      "func answer()\n"
      "  42\n"
      "end\n");

    auto entry = declaration(indexed.entries, "answer");
    REQUIRE(entry.has_value());
    CHECK(entry->detail == "answer() :> Int");
  }

  TEST_CASE("only what the user wrote counts as annotated") {
    auto indexed = index_source(
      "func annotated(x: Int) :> Int\n"
      "  x\n"
      "end\n"
      "\n"
      "func bare(y)\n"
      "  y\n"
      "end\n");

    auto with_types = declaration(indexed.entries, "annotated");
    REQUIRE(with_types.has_value());
    CHECK(with_types->annotated);

    auto param = declaration(indexed.entries, "x");
    REQUIRE(param.has_value());
    CHECK(param->annotated);

    // The parser builds a return-type node either way; only a written one
    // should suppress the editor's inlay hint.
    auto without = declaration(indexed.entries, "bare");
    REQUIRE(without.has_value());
    CHECK_FALSE(without->annotated);

    auto bare_param = declaration(indexed.entries, "y");
    REQUIRE(bare_param.has_value());
    CHECK_FALSE(bare_param->annotated);
  }
}

TEST_SUITE("lsp index / what the checker knows") {
  TEST_CASE("a constructor resolves to the type it builds") {
    // The node is left open by unification; the checker records constructors
    // in a table instead, and the editor should say `Box` rather than a bare
    // type variable.
    auto indexed = index_source(
      "type Box {\n"
      "  Wrap(value: Int)\n"
      "}\n"
      "\n"
      "func make() :> Box\n"
      "  Wrap(1)\n"
      "end\n");

    const auto& entries = indexed.entries;
    auto use = std::find_if(entries.begin(), entries.end(),
      [](const IndexEntry& entry) { return entry.name == "Wrap"; });
    REQUIRE(use != entries.end());
    CHECK(use->type == "Box");
  }

  TEST_CASE("an alias declaration reports its target") {
    auto indexed = index_source("type Count = Int\n");
    auto entry = declaration(indexed.entries, "Count");
    REQUIRE(entry.has_value());
    CHECK(entry->type == "Int");
  }

  TEST_CASE("an import is a module, not an unresolved name") {
    // Nothing declares a symbol for an import, but calling it unresolved tells
    // the editor the compiler failed at something it never attempted.
    auto indexed = index_source("Load benzene.list\n");
    auto entry = declaration(indexed.entries, "benzene.list");
    REQUIRE(entry.has_value());
    CHECK(entry->kind == "Module");
  }

  TEST_CASE("an import carries no type") {
    // The checker deliberately does not type imports, and nothing should
    // invent one by matching the path against a table.
    auto indexed = index_source("Load benzene.list\n");
    auto entry = declaration(indexed.entries, "benzene.list");
    REQUIRE(entry.has_value());
    CHECK(entry->type.empty());
  }

  TEST_CASE("a literal carries its type but is not a declaration") {
    // Marking one a declaration would put an inlay hint after every number in
    // the file and list each one in the outline.
    auto indexed = index_source(
      "func f()\n"
      "  42\n"
      "end\n");

    const auto& entries = indexed.entries;
    auto literal = std::find_if(entries.begin(), entries.end(),
      [](const IndexEntry& entry) { return entry.kind == "Literal"; });
    REQUIRE(literal != entries.end());
    CHECK(literal->type == "Int");
    CHECK_FALSE(literal->is_definition);
  }

  TEST_CASE("a string literal spans its quotes") {
    auto indexed = index_source(
      "func f()\n"
      "  \"hi\"\n"
      "end\n");

    const auto& entries = indexed.entries;
    auto literal = std::find_if(entries.begin(), entries.end(),
      [](const IndexEntry& entry) { return entry.kind == "Literal"; });
    REQUIRE(literal != entries.end());
    // `hi` plus the two quotes the lexer did not keep.
    CHECK(literal->length == 4);
  }
}

TEST_SUITE("lsp index / return types") {
  TEST_CASE("a declared return type is used as written") {
    auto indexed = index_source(
      "func f() :> Int\n"
      "  2\n"
      "end\n");

    auto entry = declaration(indexed.entries, "f");
    REQUIRE(entry.has_value());
    CHECK(entry->returns == "Int");
  }

  TEST_CASE("the return type is reported on its own, not split out of the signature") {
    // Recovering it from the rendered `Fn(...) :> R` is ambiguous the moment a
    // parameter is itself a function: there is more than one arrow to split on.
    auto indexed = index_source(
      "func apply(g: Fn(Int) :> Int, v: Int) :> Int\n"
      "  g(v)\n"
      "end\n");

    auto entry = declaration(indexed.entries, "apply");
    REQUIRE(entry.has_value());
    CHECK(entry->returns == "Int");
  }

  TEST_CASE("an inferred return type is reported too") {
    auto indexed = index_source(
      "func answer()\n"
      "  42\n"
      "end\n");

    auto entry = declaration(indexed.entries, "answer");
    REQUIRE(entry.has_value());
    CHECK(entry->returns == "Int");
  }

  TEST_CASE("a polymorphic return stays a variable") {
    // Nothing should invent a concrete type here: the function really is
    // generic, and the editor must not claim otherwise.
    auto indexed = index_source(
      "func identity(x)\n"
      "  x\n"
      "end\n");

    auto entry = declaration(indexed.entries, "identity");
    REQUIRE(entry.has_value());
    CHECK(entry->returns.starts_with("'"));
  }
}

TEST_SUITE("lsp index / type declarations") {
  TEST_CASE("a declared type is recorded as a type") {
    // The resolver declares a symbol for the name but discards the pointer, so
    // the node cannot say what it is. Reporting it as unresolved would tell the
    // editor the compiler failed to bind a name it binds perfectly well.
    auto indexed = index_source("type Data {\n  Integer\n}\n");
    auto entry = declaration(indexed.entries, "Data");
    REQUIRE(entry.has_value());
    CHECK(entry->kind == "Type");
  }

  TEST_CASE("a sum type lists its constructors") {
    auto indexed = index_source(
      "type Shape {\n"
      "  Circle(radius: Int)\n"
      "  Square(side: Int)\n"
      "}\n");

    auto entry = declaration(indexed.entries, "Shape");
    REQUIRE(entry.has_value());
    CHECK(entry->detail.starts_with("Shape {"));
    CHECK(entry->detail.find("Circle") != std::string::npos);
    CHECK(entry->detail.find("Square") != std::string::npos);
  }

  TEST_CASE("an alias shows what it aliases") {
    auto indexed = index_source("type Count = Int\n");
    auto entry = declaration(indexed.entries, "Count");
    REQUIRE(entry.has_value());
    CHECK(entry->detail == "Count = Int");
  }

  TEST_CASE("a bare declaration shows just its name") {
    auto indexed = index_source("type Opaque\n");
    auto entry = declaration(indexed.entries, "Opaque");
    REQUIRE(entry.has_value());
    CHECK(entry->kind == "Type");
    CHECK(entry->detail == "Opaque");
  }

  TEST_CASE("a declared type is usable as an annotation") {
    // The point of all this: the checker resolves the name, so a parameter
    // annotated with it gets that type, and the editor should agree.
    auto indexed = index_source(
      "type Data {\n  Integer\n}\n"
      "\n"
      "func take(d: Data) :> Int\n"
      "  1\n"
      "end\n");

    auto param = declaration(indexed.entries, "d");
    REQUIRE(param.has_value());
    CHECK(param->type == "Data");
  }
}

TEST_SUITE("lsp index / scopes") {
  TEST_CASE("module-level declarations have no enclosing scope") {
    auto indexed = index_source("const top: Int = 1\n");
    auto entry = declaration(indexed.entries, "top");
    REQUIRE(entry.has_value());
    CHECK(entry->scope_line == 0);
    CHECK(entry->scope_column == 0);
  }

  TEST_CASE("locals record the function that declares them") {
    auto indexed = index_source(
      "func outer()\n"
      "  let inner = 1\n"
      "  inner\n"
      "end\n");

    auto function = declaration(indexed.entries, "outer");
    auto local = declaration(indexed.entries, "inner");
    REQUIRE(function.has_value());
    REQUIRE(local.has_value());

    CHECK(local->scope_line == function->line);
    CHECK(local->scope_column == function->column);
  }

  TEST_CASE("a function is not its own scope") {
    // Were it, the editor's outline would nest it under itself forever.
    auto indexed = index_source(
      "func solo()\n"
      "  1\n"
      "end\n");

    auto entry = declaration(indexed.entries, "solo");
    REQUIRE(entry.has_value());
    CHECK(entry->scope_line == 0);
  }

  TEST_CASE("sibling functions do not share locals") {
    auto indexed = index_source(
      "func first()\n"
      "  let a = 1\n"
      "  a\n"
      "end\n"
      "\n"
      "func second()\n"
      "  let b = 2\n"
      "  b\n"
      "end\n");

    auto a = declaration(indexed.entries, "a");
    auto b = declaration(indexed.entries, "b");
    REQUIRE(a.has_value());
    REQUIRE(b.has_value());
    CHECK(a->scope_line != b->scope_line);
  }
}

TEST_SUITE("lsp index / robustness") {
  TEST_CASE("an empty source yields an empty index") {
    auto indexed = index_source("");
    CHECK(indexed.entries.empty());
  }

  TEST_CASE("a file that does not parse still indexes without crashing") {
    // The editor asks for an index on every keystroke, most of which land
    // mid-edit on something incomplete.
    auto indexed = index_source("func broken(\n  let =\n");
    CHECK(indexed.entries.size() < 1000);
  }

  TEST_CASE("duplicate declarations are both recorded") {
    auto indexed = index_source("const x = 1\nconst x = 2\n");
    CHECK(occurrences(indexed.entries, "x") == 2);
  }
}
