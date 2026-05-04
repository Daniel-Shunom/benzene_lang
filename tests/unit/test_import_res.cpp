#include <doctest/doctest.h>

// NOTE: import_res.hpp redefines `Module` as an empty struct (a stub for
// future work). Pulling it into a TU that also transitively includes
// module/module.hpp produces a redefinition error — so this file deliberately
// avoids fixtures.hpp and only includes the minimum surface.
#include <ether/import_res/import_res.hpp>
#include <ether/tokens/token_types.hpp>

namespace {
Token import_tok(TokenType t, std::string value) {
  return Token{ .token_type = t, .token_value = std::move(value), .line_number = 1, .column_number = 1 };
}
}

TEST_SUITE("import_res / is_valid_import_path") {
  TEST_CASE("non ImportModule token is rejected") {
    auto t = import_tok(TokenType::Identifier, "benzene.list");
    CHECK_FALSE(is_valid_import_path(t));
  }

  TEST_CASE("plain dotted module path is accepted") {
    auto t = import_tok(TokenType::ImportModule, "benzene.list");
    CHECK(is_valid_import_path(t));
  }

  TEST_CASE("path with underscores accepted") {
    auto t = import_tok(TokenType::ImportModule, "benzene.extended_cpu");
    CHECK(is_valid_import_path(t));
  }

  TEST_CASE("path with disallowed characters rejected") {
    auto t = import_tok(TokenType::ImportModule, "benzene/list");
    CHECK_FALSE(is_valid_import_path(t));
  }

  TEST_CASE("digits are not currently allowed in import paths") {
    auto t = import_tok(TokenType::ImportModule, "benzene.cpu2");
    CHECK_FALSE(is_valid_import_path(t));
  }
}
