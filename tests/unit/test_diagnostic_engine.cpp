#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/diagnostics/diagnostic.hpp>
#include <ether/diagnostics/diagnostic_eng.hpp>

using namespace ether::test;

namespace {
Diagnostic make_diag(DiagnosticLevel level, std::string msg, size_t line = 1, size_t col = 1) {
  Diagnostic d;
  d.level = level;
  d.phase = DiagnosticPhase::Lexer;
  d.location = { .line = line, .column = col };
  d.message = std::move(msg);
  return d;
}
}

TEST_SUITE("diagnostics / engine") {
  TEST_CASE("fresh engine has no errors") {
    DiagnosticEngine eng;
    CHECK_FALSE(eng.has_errors());
  }

  TEST_CASE("Warn diagnostics do not flip has_errors") {
    DiagnosticEngine eng;
    eng.report(make_diag(DiagnosticLevel::Warn, "warn"));
    eng.report(make_diag(DiagnosticLevel::Note, "note"));
    CHECK_FALSE(eng.has_errors());
  }

  TEST_CASE("Fail diagnostics flip has_errors") {
    DiagnosticEngine eng;
    eng.report(make_diag(DiagnosticLevel::Fail, "boom"));
    CHECK(eng.has_errors());
  }

  TEST_CASE("print_all renders header, location, and source line") {
    DiagnosticEngine eng;
    eng.set_source("test.bz", "first line\nsecond line\n");
    eng.report(make_diag(DiagnosticLevel::Fail, "bad token", 2, 8));

    CoutSink sink;
    eng.print_all();
    auto out = sink.str();

    CHECK(out.find("error") != std::string::npos);
    CHECK(out.find("bad token") != std::string::npos);
    CHECK(out.find("test.bz") != std::string::npos);
    CHECK(out.find("second line") != std::string::npos);
  }

  TEST_CASE("print_all sorts diagnostics by line then column") {
    DiagnosticEngine eng;
    eng.set_source("t.bz", "a\nb\nc\n");
    eng.report(make_diag(DiagnosticLevel::Fail, "third",  3, 1));
    eng.report(make_diag(DiagnosticLevel::Fail, "first",  1, 1));
    eng.report(make_diag(DiagnosticLevel::Fail, "second", 2, 1));

    CoutSink sink;
    eng.print_all();
    auto out = sink.str();

    auto pf = out.find("first");
    auto ps = out.find("second");
    auto pt = out.find("third");
    REQUIRE(pf != std::string::npos);
    REQUIRE(ps != std::string::npos);
    REQUIRE(pt != std::string::npos);
    CHECK(pf < ps);
    CHECK(ps < pt);
  }

  TEST_CASE("set_source handles trailing CR (Windows newlines)") {
    DiagnosticEngine eng;
    eng.set_source("t.bz", "alpha\r\nbeta\r\n");
    eng.report(make_diag(DiagnosticLevel::Fail, "x", 2, 1));

    CoutSink sink;
    eng.print_all();
    auto out = sink.str();
    CHECK(out.find("beta\r") == std::string::npos);
    CHECK(out.find("beta") != std::string::npos);
  }
}
