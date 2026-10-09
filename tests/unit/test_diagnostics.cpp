#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/ast_passes/scope_resolution/scope_res.hpp>
#include <ether/ast_passes/symbol_resolver/symbol_resolver.hpp>
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ether/module/module.hpp>

#include <algorithm>
#include <string>
#include <vector>

using namespace ether::test;

namespace {

// Runs the whole front-end and hands back what it reported. Diagnostics are
// part of the interface: an editor underlines exactly where these say to.
std::vector<Diagnostic> diagnose(const std::string& source) {
  Module module("<test>", source);
  module.generate_ast();

  ScopeRes scopes(module.get_symbol_storage(), module.get_diag_engine());
  SymbolResolver resolver(module.get_symbol_storage(), module.get_diag_engine());
  TypeChecker checker(false, module.get_diag_engine());

  module.attach_visitor(resolver);
  module.attach_visitor(scopes);
  module.attach_visitor(checker);
  module.apply_visitors();
  checker.unify_constraints();

  return module.get_diag_engine().all();
}

// The first diagnostic from `phase`, if there is one.
const Diagnostic* from_phase(const std::vector<Diagnostic>& reported,
                             DiagnosticPhase phase) {
  auto found = std::find_if(reported.begin(), reported.end(),
    [phase](const Diagnostic& diagnostic) { return diagnostic.phase == phase; });
  return found == reported.end() ? nullptr : &*found;
}

std::vector<size_t> lines_from(const std::vector<Diagnostic>& reported,
                               DiagnosticPhase phase) {
  std::vector<size_t> lines;
  for (const auto& diagnostic : reported) {
    if (diagnostic.phase == phase) {
      lines.push_back(diagnostic.location.line);
    }
  }
  return lines;
}

}  // namespace

TEST_SUITE("diagnostics / undefined names") {
  TEST_CASE("calls clearly say the function is not defined") {
    auto reported = diagnose("func use()\n  missing()\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::Resolver);
    REQUIRE(error);
    CHECK(error->message == "Function `missing` is not defined");
  }

  TEST_CASE("binding values clearly say the identifier is not defined") {
    auto reported = diagnose("func use()\n  let b = missing\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::Resolver);
    REQUIRE(error);
    CHECK(error->message == "Identifier `missing` is not defined");
  }
}

TEST_SUITE("diagnostics / generic constructors") {
  TEST_CASE("constructor fields follow the instantiated parent parameters") {
    auto reported = diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func get(r: Result(Int, String)) :> Int\n"
      "  case r:\n    Ok(x) :> x\n  end\nend\n"
      "func message(r: Result(Int, String)) :> String\n"
      "  case r:\n    Error(x) :> x\n  end\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("a wrong field use is rejected") {
    auto reported = diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func wrong(r: Result(Int, String)) :> String\n"
      "  case r:\n    Ok(x) :> x\n  end\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("constructor arguments preserve the types of existing bindings") {
    auto reported = diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func wrong(x: String) :> Result(Int, String)\n  Ok(x)\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("constructor expressions constrain their argument and result together") {
    CHECK(diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func make() :> Result(Int, String)\n  Ok(1)\nend\n").empty());
    auto reported = diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func wrong() :> Result(Int, String)\n  Ok(\"text\")\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("different constructor uses do not share substitutions") {
    auto reported = diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func number(r: Result(Int, String)) :> Int\n"
      "  case r:\n    Ok(x) :> x\n  end\nend\n"
      "func text(r: Result(String, Int)) :> String\n"
      "  case r:\n    Ok(x) :> x\n  end\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("nested type applications keep their structure") {
    auto reported = diagnose(
      "type Box(a) { Wrap(List(a)) }\n"
      "func unwrap(r: Box(Int)) :> List(Int)\n"
      "  case r:\n    Wrap(xs) :> xs\n  end\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("labelled fields reuse parent parameters") {
    auto reported = diagnose(
      "type Box(a) { Wrap(value: a) }\n"
      "func unwrap(r: Box(Int)) :> Int\n"
      "  case r:\n    Wrap(x) :> x\n  end\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("undeclared generic parameters have an exact source location") {
    auto reported = diagnose("type Result(a, b) {\n  Ok(c)\n}\n");
    const auto* error = from_phase(reported, DiagnosticPhase::Resolver);
    REQUIRE(error);
    CHECK(error->message.find("Type `c` is not defined") != std::string::npos);
    CHECK(error->location.line == 2);
    CHECK(error->location.column == 6);
  }

  TEST_CASE("duplicates and applied parameters are rejected") {
    CHECK(from_phase(diagnose("type Box(a, a) { Wrap(a) }"), DiagnosticPhase::Resolver) != nullptr);
    CHECK(from_phase(diagnose("type Box(a) { Wrap(a(Int)) }"), DiagnosticPhase::Resolver) != nullptr);
  }
}

TEST_SUITE("diagnostics / case pattern validation") {
  TEST_CASE("branches can bind the same name with different field types") {
    CHECK(diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func message(r: Result(Int, String)) :> String\n"
      "  case r:\n    Ok(x) :> \"number\"\n    Error(x) :> x\n  end\nend\n").empty());
  }

  TEST_CASE("literal patterns must match the condition type") {
    auto reported = diagnose(
      "func bad(x: Int)\n  case x:\n    \"text\" :> 1\n  end\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("all branch results must have compatible types") {
    auto reported = diagnose(
      "func bad(x: Int)\n  case x:\n    1 :> 1\n    2 :> \"text\"\n  end\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("constructor results are expressions rather than new patterns") {
    CHECK(diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func copy(r: Result(Int, String)) :> Result(Int, String)\n"
      "  case r:\n    Ok(x) :> Ok(x)\n    Error(x) :> Error(x)\n  end\nend\n").empty());
    auto reported = diagnose(
      "type Result(a, b) { Ok(a) Error(b) }\n"
      "func bad(r: Result(Int, String)) :> Result(Int, String)\n"
      "  case r:\n    Ok(x) :> Error(x)\n  end\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("bindings cannot leak between branches or after the case") {
    auto sibling = diagnose(
      "type Box(a) { Wrap(a) Empty }\n"
      "func bad(r: Box(Int))\n"
      "  case r:\n    Wrap(x) :> x\n    Empty :> x\n  end\nend\n");
    CHECK(from_phase(sibling, DiagnosticPhase::Resolver) != nullptr);
    auto outside = diagnose(
      "type Box(a) { Wrap(a) }\n"
      "func bad(r: Box(Int))\n  case r:\n    Wrap(x) :> x\n  end\n  x\nend\n");
    CHECK(from_phase(outside, DiagnosticPhase::Resolver) != nullptr);
  }

  TEST_CASE("nested patterns and wildcards are checked recursively") {
    CHECK(diagnose(
      "type Box(a) { Wrap(a) }\n"
      "func nested(r: Box(Box(Int))) :> Int\n"
      "  case r:\n    Wrap(Wrap(x)) :> x\n  end\nend\n"
      "func ignore(r: Box(Int)) :> Int\n"
      "  case r:\n    Wrap(_) :> 1\n    _ :> 2\n  end\nend\n").empty());
    auto reported = diagnose(
      "type Box(a) { Wrap(a) }\n"
      "func bad(r: Box(Int))\n  case r:\n    Wrap(\"text\") :> 1\n  end\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("nullary constructors are values of their declared parent") {
    CHECK(diagnose(
      "type Maybe(a) { Some(a) None }\n"
      "func unwrap(r: Maybe(Int)) :> Int\n"
      "  case r:\n    Some(x) :> x\n    None :> 0\n  end\nend\n").empty());
  }

  TEST_CASE("duplicate binders and incorrect constructor arity are rejected") {
    CHECK(from_phase(diagnose(
      "type Pair(a) { Both(a, a) }\n"
      "func bad(r: Pair(Int))\n  case r:\n    Both(x, x) :> x\n  end\nend\n"), DiagnosticPhase::Resolver) != nullptr);
    CHECK(from_phase(diagnose(
      "type Box(a) { Wrap(a) }\n"
      "func bad(r: Box(Int))\n  case r:\n    Wrap(x, y) :> 1\n  end\nend\n"), DiagnosticPhase::Resolver) != nullptr);
  }

  TEST_CASE("each condition is checked against its corresponding pattern") {
    CHECK(diagnose(
      "func match(a: Int, b: String) :> Int\n"
      "  case a, b:\n    1, \"text\" :> 1\n    x, _ :> x\n  end\nend\n").empty());
    auto reported = diagnose(
      "func bad(a: Int, b: String)\n"
      "  case a, b:\n    1, 2 :> 1\n  end\nend\n");
    CHECK(from_phase(reported, DiagnosticPhase::TypeChecker) != nullptr);
  }
}

TEST_SUITE("diagnostics / generic aliases") {
  TEST_CASE("alias mismatches include the declared alias and its expanded signature") {
    auto reported = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func foo(x: String) :> String\n  x\nend\n"
      "func use()\n  let b: Handler(Int) = foo\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(error);
    CHECK(error->message.find("Expected declared type Handler(Int)") != std::string::npos);
    CHECK(error->message.find("expands to Fn(Int) :> String") != std::string::npos);
    CHECK(error->message.find("expected Int, but found String") != std::string::npos);
  }

  TEST_CASE("call mismatches retain aliases from the function's signature") {
    auto reported = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func foo(x: String) :> String\n  x\nend\n"
      "func accept(h: Handler(Int)) :> String\n  h(1)\nend\n"
      "func use()\n  accept(foo)\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(error);
    CHECK(error->message.find("Handler(Int)") != std::string::npos);
    CHECK(error->message.find("expands to") != std::string::npos);
  }

  TEST_CASE("generic return mismatches retain the complete declared type") {
    auto reported = diagnose(
      "func wrong() :> List(Int)\n  [\"text\"]\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(error);
    CHECK(error->message.find("Expected declared type List(Int)") != std::string::npos);
  }

  TEST_CASE("a polymorphic function mismatch identifies its generalized scheme") {
    auto reported = diagnose(
      "func same(x, y)\n  x + y\nend\n"
      "func use()\n  same(1, \"text\")\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(error);
    CHECK(error->message.find("Expected generalized type forall") != std::string::npos);
    CHECK(error->message.find("instantiated as") != std::string::npos);
  }
  TEST_CASE("a function can be bound through a parameterized signature alias") {
    auto reported = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func foo(x: Int) :> String\n  \"ok\"\nend\n"
      "func use() :> String\n  let b: Handler(Int) = foo\n  b(1)\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("the alias checks both the argument and return signature") {
    auto wrong_argument = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func foo(x: String) :> String\n  x\nend\n"
      "func use()\n  let b: Handler(Int) = foo\nend\n");
    CHECK(from_phase(wrong_argument, DiagnosticPhase::TypeChecker) != nullptr);
    auto wrong_return = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func foo(x: Int) :> Int\n  x\nend\n"
      "func use()\n  let b: Handler(Int) = foo\nend\n");
    CHECK(from_phase(wrong_return, DiagnosticPhase::TypeChecker) != nullptr);
  }

  TEST_CASE("alias applications use their own explicit arguments") {
    auto reported = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func number(x: Int) :> String\n  \"number\"\nend\n"
      "func text(x: String) :> String\n  x\nend\n"
      "func use() :> String\n"
      "  let a: Handler(Int) = number\n"
      "  let b: Handler(String) = text\n  a(1)\n  b(\"ok\")\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("aliases substitute recursively through other aliases") {
    auto reported = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "type Indirect(item) = Handler(List(item))\n"
      "func foo(xs: List(Int)) :> String\n  \"ok\"\nend\n"
      "func use()\n  let b: Indirect(Int) = foo\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("generic aliases work in parameter and return annotations") {
    auto reported = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func foo(x: Int) :> String\n  \"ok\"\nend\n"
      "func pass(h: Handler(Int)) :> Handler(Int)\n  h\nend\n"
      "func use()\n  pass(foo)\nend\n");
    CHECK(reported.empty());
  }

  TEST_CASE("alias arity errors are reported at the annotation") {
    auto reported = diagnose(
      "type Handler(data) = Fn(data) :> String\n"
      "func use()\n  let b: Handler(Int, String) = 1\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(error);
    CHECK(error->message.find("expects 1 type arguments, but got 2") != std::string::npos);
    CHECK(error->location.line == 3);
    CHECK(error->location.column == 10);
  }

  TEST_CASE("nested applications of the same alias are not cycles") {
    CHECK(diagnose(
      "type Items(a) = List(a)\n"
      "func identity(x: Items(Items(Int))) :> List(List(Int))\n  x\nend\n").empty());
  }

  TEST_CASE("cyclic aliases report a diagnostic without recursing forever") {
    auto reported = diagnose(
      "type Loop(a) = Loop(a)\n"
      "func use(x: Loop(Int))\n  x\nend\n");
    const auto* error = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(error);
    CHECK(error->message.find("Cyclic type alias") != std::string::npos);
  }
}

TEST_SUITE("diagnostics / assignment needs a binder") {
  TEST_CASE("a bare assignment is rejected") {
    // Benzene is functional: there is no assignment, only binding.
    auto reported = diagnose(
      "func g()\n"
      "  a = 5\n"
      "  a\n"
      "end\n");

    const auto* parsed = from_phase(reported, DiagnosticPhase::Parser);
    REQUIRE(parsed != nullptr);
    CHECK(parsed->location.line == 2);
    CHECK(parsed->location.column == 3);
    CHECK(parsed->message.find("let") != std::string::npos);
  }

  TEST_CASE("an annotated assignment is rejected too") {
    auto reported = diagnose(
      "func g()\n"
      "  a: Int = 5\n"
      "  1\n"
      "end\n");

    const auto* parsed = from_phase(reported, DiagnosticPhase::Parser);
    REQUIRE(parsed != nullptr);
    CHECK(parsed->location.line == 2);
  }

  TEST_CASE("the statements after it still parse") {
    // The point of reporting rather than bailing: a slip on one line used to
    // take the rest of the function with it, silently.
    auto reported = diagnose(
      "func takes_int(n: Int) :> Int\n"
      "  n\n"
      "end\n"
      "\n"
      "func g()\n"
      "  a = 5\n"
      "  takes_int(\"text\")\n"
      "end\n");

    // The call below the bad line was parsed, checked, and found wrong.
    auto lines = lines_from(reported, DiagnosticPhase::TypeChecker);
    REQUIRE_FALSE(lines.empty());
    CHECK(lines[0] == 7);
  }

  TEST_CASE("a let binding is still accepted") {
    auto reported = diagnose(
      "func g()\n"
      "  let a: Int = 5\n"
      "  a\n"
      "end\n");

    CHECK(from_phase(reported, DiagnosticPhase::Parser) == nullptr);
  }

  TEST_CASE("equality is not mistaken for assignment") {
    auto reported = diagnose(
      "func g()\n"
      "  let a = 1\n"
      "  a == 2\n"
      "end\n");

    CHECK(from_phase(reported, DiagnosticPhase::Parser) == nullptr);
  }
}

TEST_SUITE("diagnostics / where a type error is reported") {
  TEST_CASE("a bad argument is reported at the call") {
    // The declaration is fine and may be called correctly elsewhere; the
    // mistake belongs to this call.
    auto reported = diagnose(
      "func takes_int(n: Int) :> Int\n"
      "  n\n"
      "end\n"
      "\n"
      "func caller()\n"
      "  let s = \"text\"\n"
      "  takes_int(s)\n"
      "end\n");

    const auto* mismatch = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(mismatch != nullptr);
    CHECK(mismatch->location.line == 7);
    CHECK(mismatch->location.column == 3);
  }

  TEST_CASE("each bad call is reported on its own line") {
    auto reported = diagnose(
      "func takes_int(n: Int) :> Int\n"
      "  n\n"
      "end\n"
      "\n"
      "func caller()\n"
      "  let s = \"text\"\n"
      "  takes_int(s)\n"
      "  takes_int(s)\n"
      "end\n");

    auto lines = lines_from(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(lines.size() >= 2);
    CHECK(lines[0] == 7);
    CHECK(lines[1] == 8);
  }

  TEST_CASE("a body that does not match the declared return blames the name") {
    auto reported = diagnose(
      "func wrong() :> Int\n"
      "  \"text\"\n"
      "end\n");

    const auto* mismatch = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(mismatch != nullptr);
    CHECK(mismatch->location.line == 1);
    CHECK(mismatch->location.column == 6);
  }

  TEST_CASE("an operator misuse blames the operator") {
    auto reported = diagnose(
      "func f()\n"
      "  let s = \"text\"\n"
      "  s && True\n"
      "end\n");

    const auto* mismatch = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(mismatch != nullptr);
    CHECK(mismatch->location.line == 3);
  }

  TEST_CASE("a bad binding blames the name being bound") {
    auto reported = diagnose(
      "func f() :> Int\n"
      "  let n: Int = \"text\"\n"
      "  n\n"
      "end\n");

    const auto* mismatch = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(mismatch != nullptr);
    CHECK(mismatch->location.line == 2);
    CHECK(mismatch->location.column == 7);
  }

  TEST_CASE("a type error is no longer parked on line 1") {
    auto reported = diagnose(
      "func takes_int(n: Int) :> Int\n"
      "  n\n"
      "end\n"
      "\n"
      "func caller()\n"
      "  takes_int(\"text\")\n"
      "end\n");

    const auto* mismatch = from_phase(reported, DiagnosticPhase::TypeChecker);
    REQUIRE(mismatch != nullptr);
    CHECK(mismatch->location.line != 1);
  }
}
