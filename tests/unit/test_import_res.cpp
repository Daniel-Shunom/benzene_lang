#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/import_res/import_res.hpp>
#include <ether/tokens/token_types.hpp>

using namespace ether::test;

TEST_SUITE("import_res / is_valid_import_path") {
  TEST_CASE("non ImportModule token is rejected") {
    auto t = make_tok(TokenType::Identifier, "benzene.list");
    CHECK_FALSE(is_valid_import_path(t));
  }

  TEST_CASE("plain dotted module path is accepted") {
    auto t = make_tok(TokenType::ImportModule, "benzene.list");
    CHECK(is_valid_import_path(t));
  }

  TEST_CASE("path with underscores accepted") {
    auto t = make_tok(TokenType::ImportModule, "benzene.extended_cpu");
    CHECK(is_valid_import_path(t));
  }

  TEST_CASE("path with disallowed characters rejected") {
    auto t = make_tok(TokenType::ImportModule, "benzene/list");
    CHECK_FALSE(is_valid_import_path(t));
  }

  TEST_CASE("digits are not currently allowed in import paths") {
    auto t = make_tok(TokenType::ImportModule, "benzene.cpu2");
    CHECK_FALSE(is_valid_import_path(t));
  }
}
