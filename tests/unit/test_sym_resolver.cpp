#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/ast/symbol_resolver/symbol_resolver.hpp>
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/module/module.hpp>
#include <ether/nodes/node_expr.hpp>

#include <memory>

using namespace ether::test;

namespace {

struct ResolvedModule {
  std::unique_ptr<Module> module;
  std::unique_ptr<SymbolResolver> resolver;
  std::unordered_map<std::string, SymbolAttr*> exports;

  bool has_errors() const { return module->get_diag_engine().has_errors(); }
};

ResolvedModule resolve(const std::string& src) {
  ResolvedModule rm;
  rm.module = std::make_unique<Module>("<test>", src);
  rm.module->generate_ast();
  rm.resolver = std::make_unique<SymbolResolver>(
    rm.module->get_symbol_storage(),
    rm.module->get_diag_engine()
  );
  rm.module->attach_visitor(*rm.resolver);
  rm.module->apply_visitors();
  rm.exports = rm.resolver->take_exports();
  return rm;
}

template <typename T>
T* first_of(Parent& p) {
  for (auto& c : p.children) {
    if (auto* x = dynamic_cast<T*>(c.get())) return x;
  }
  return nullptr;
}

}  // namespace

TEST_SUITE("sym_res / let bindings") {
  TEST_CASE("let at top level is poisoned and reports error") {
    auto rm = resolve("let x = 1");
    CHECK(rm.has_errors());

    auto ast = rm.module->get_ast();
    auto* let = first_of<NDLetBindExpr>(ast);
    REQUIRE(let);
    CHECK(let->is_poisoned);
  }

  TEST_CASE("let inside a function body is allowed") {
    auto rm = resolve("func f()\n  let x = 1\nend");
    CHECK_FALSE(rm.has_errors());
  }
}

TEST_SUITE("sym_res / const") {
  TEST_CASE("const at top level succeeds and is exported") {
    auto rm = resolve("const x = 10");
    CHECK_FALSE(rm.has_errors());
    CHECK(rm.exports.contains("x"));
  }

  TEST_CASE("const inside a function is rejected") {
    auto rm = resolve("func f()\n  const x = 10\nend");
    CHECK(rm.has_errors());
  }

  TEST_CASE("duplicate const at top level reports error") {
    auto rm = resolve("const x = 1\nconst x = 2");
    CHECK(rm.has_errors());
  }
}

TEST_SUITE("sym_res / functions") {
  TEST_CASE("top-level function is exported") {
    auto rm = resolve("func f()\n  1\nend");
    CHECK_FALSE(rm.has_errors());
    CHECK(rm.exports.contains("f"));
  }

  TEST_CASE("duplicate function declaration reports an error") {
    auto rm = resolve("func f()\n  1\nend\nfunc f()\n  2\nend");
    CHECK(rm.has_errors());
  }

  TEST_CASE("duplicate parameter names report an error") {
    auto rm = resolve("func f(a, a)\n  1\nend");
    CHECK(rm.has_errors());
  }
}

TEST_SUITE("sym_res / scoped expressions") {
  TEST_CASE("top-level scoped expression is rejected") {
    auto rm = resolve("{ 1 }");
    // run_parser collects the scope as a value expression at top level; the
    // resolver flags it.
    CHECK(rm.has_errors());
  }

  TEST_CASE("scoped expression inside a function is allowed") {
    auto rm = resolve("func f()\n  let x = { 1 + 2 }\nend");
    CHECK_FALSE(rm.has_errors());
  }
}

TEST_SUITE("sym_res / case expressions") {
  TEST_CASE("case at top level is rejected") {
    auto rm = resolve("case 1 :\n  2 :> 3\nend");
    CHECK(rm.has_errors());
  }
}

TEST_SUITE("sym_res / function calls") {
  TEST_CASE("calling an undeclared function fails resolution") {
    auto rm = resolve("func f()\n  g()\nend");
    CHECK(rm.has_errors());
  }

  TEST_CASE("calling a previously declared function succeeds") {
    auto rm = resolve(
      "func g()\n  1\nend\n"
      "func f()\n  g()\nend"
    );
    CHECK_FALSE(rm.has_errors());
  }
}
