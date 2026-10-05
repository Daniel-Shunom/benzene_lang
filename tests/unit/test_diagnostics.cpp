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
