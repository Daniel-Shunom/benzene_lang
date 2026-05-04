#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/ast/symbol_resolver/symbol_resolver.hpp>
#include <ether/module/module.hpp>

#include <fstream>
#include <sstream>
#include <string>

using namespace ether::test;

namespace {

std::string read_sample(const std::string& name) {
  std::string path = std::string(ETHER_TEST_SAMPLES_DIR) + "/" + name;
  std::ifstream f(path);
  REQUIRE_MESSAGE(f.is_open(), "could not open sample: ", path);
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

struct Pipeline {
  std::unique_ptr<Module> module;
  std::unique_ptr<SymbolResolver> resolver;
};

Pipeline run_full(const std::string& source, const std::string& path) {
  Pipeline pl;
  pl.module = std::make_unique<Module>(path, source);
  pl.module->generate_ast();
  pl.resolver = std::make_unique<SymbolResolver>(
    pl.module->get_symbol_storage(),
    pl.module->get_diag_engine()
  );
  pl.module->attach_visitor(*pl.resolver);
  pl.module->apply_visitors();
  pl.module->set_exports(pl.resolver->take_exports());
  return pl;
}

}  // namespace

TEST_SUITE("integration / module pipeline") {
  TEST_CASE("a clean program reports no errors and exports its symbols") {
    auto src = read_sample("valid_program.bz");
    auto pl = run_full(src, "valid_program.bz");
    CHECK_FALSE(pl.module->get_diag_engine().has_errors());

    const auto& exports = pl.module->get_exported_symbols();
    CHECK(exports.contains("greeting"));
    CHECK(exports.contains("identity"));
    CHECK(exports.contains("use_identity"));
  }

  TEST_CASE("duplicate const at top level is reported") {
    auto src = read_sample("duplicate_const.bz");
    auto pl = run_full(src, "duplicate_const.bz");
    CHECK(pl.module->get_diag_engine().has_errors());
  }

  TEST_CASE("let at top level is reported") {
    auto src = read_sample("let_at_top_level.bz");
    auto pl = run_full(src, "let_at_top_level.bz");
    CHECK(pl.module->get_diag_engine().has_errors());
  }

  TEST_CASE("pipe chain inside a function resolves cleanly") {
    auto src = read_sample("pipe_chain.bz");
    auto pl = run_full(src, "pipe_chain.bz");
    CHECK_FALSE(pl.module->get_diag_engine().has_errors());

    const auto& exports = pl.module->get_exported_symbols();
    CHECK(exports.contains("a"));
    CHECK(exports.contains("b"));
    CHECK(exports.contains("c"));
    CHECK(exports.contains("chain"));
  }

  TEST_CASE("print_errors writes to stdout when diagnostics exist") {
    auto src = read_sample("duplicate_const.bz");
    auto pl = run_full(src, "duplicate_const.bz");

    CoutSink sink;
    pl.module->print_errors();
    auto out = sink.str();

    CHECK(out.find("error") != std::string::npos);
    CHECK(out.find("Duplicate") != std::string::npos);
  }
}
