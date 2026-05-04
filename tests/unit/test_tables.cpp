#include <doctest/doctest.h>

#include "fixtures.hpp"

// utils.hpp transitively includes keyword_table, literal_table, and
// operator_table. operator_table.hpp lacks an include guard, so including it
// twice in the same TU is a redefinition error — we go through utils only.
#include <ether/tables/utils.hpp>
#include <ether/tokens/token_types.hpp>

using namespace ether::test;

TEST_SUITE("tables / keyword table") {
  TEST_CASE("known keywords map to expected token types") {
    CHECK(KeywordTable.at("Load")    == TokenType::ImportKeyword);
    CHECK(KeywordTable.at("Cmt")     == TokenType::CommentKeyword);
    CHECK(KeywordTable.at("const")   == TokenType::ConstantKeyword);
    CHECK(KeywordTable.at("let")     == TokenType::LetKeyword);
    CHECK(KeywordTable.at("Nil")     == TokenType::NilLiteral);
    CHECK(KeywordTable.at("True")    == TokenType::TrueLiteral);
    CHECK(KeywordTable.at("False")   == TokenType::FalseLiteral);
    CHECK(KeywordTable.at("func")    == TokenType::FuncStart);
    CHECK(KeywordTable.at("end")     == TokenType::EndStmt);
    CHECK(KeywordTable.at("case")    == TokenType::Case);
    CHECK(KeywordTable.at("default") == TokenType::Default);
  }

  TEST_CASE("non-keywords absent from table") {
    CHECK_FALSE(KeywordTable.contains("LET"));
    CHECK_FALSE(KeywordTable.contains("foo"));
    CHECK_FALSE(KeywordTable.contains(""));
  }
}

TEST_SUITE("tables / literal table") {
  TEST_CASE("literal token types are members") {
    CHECK(LiteralTable.contains(TokenType::IntegerLiteral));
    CHECK(LiteralTable.contains(TokenType::FloatLiteral));
    CHECK(LiteralTable.contains(TokenType::StringLiteral));
    CHECK(LiteralTable.contains(TokenType::UTStringLiteral));
    CHECK(LiteralTable.contains(TokenType::TrueLiteral));
    CHECK(LiteralTable.contains(TokenType::FalseLiteral));
    CHECK(LiteralTable.contains(TokenType::NilLiteral));
  }

  TEST_CASE("non-literal token types are absent") {
    CHECK_FALSE(LiteralTable.contains(TokenType::Identifier));
    CHECK_FALSE(LiteralTable.contains(TokenType::PlusOp));
    CHECK_FALSE(LiteralTable.contains(TokenType::EoF));
  }
}

TEST_SUITE("tables / operator list") {
  TEST_CASE("multi-character operators precede their single-character prefixes") {
    // Order matters: scan_operator iterates in declaration order, and `>=`
    // must be tried before `>` to avoid mis-tokenizing `>=` as `>`, `=`.
    auto pos_of = [](const std::string& op) -> int {
      for (size_t i = 0; i < OperatorList.size(); ++i) {
        if (OperatorList[i].first == op) return static_cast<int>(i);
      }
      return -1;
    };
    CHECK(pos_of(">=") < pos_of(">"));
    CHECK(pos_of("<=") < pos_of("<"));
    CHECK(pos_of("==") < pos_of("="));
    CHECK(pos_of("~=") < pos_of("~"));
    CHECK(pos_of(":>") >= 0);
  }
}

TEST_SUITE("tables / utils helpers") {
  TEST_CASE("is_keyword recognizes spelled keywords by token_value") {
    auto t1 = make_tok(TokenType::Identifier, "let");
    CHECK(is_keyword(t1));

    auto t2 = make_tok(TokenType::Identifier, "foo");
    CHECK_FALSE(is_keyword(t2));
  }

  TEST_CASE("is_operator recognizes operator token types") {
    auto plus = make_tok(TokenType::PlusOp, "+");
    auto pipe = make_tok(TokenType::PipeOp, "|=> ");
    auto ident = make_tok(TokenType::Identifier, "x");
    CHECK(is_operator(plus));
    CHECK(is_operator(pipe));
    CHECK_FALSE(is_operator(ident));
  }

  TEST_CASE("is_literal recognizes literal token types") {
    auto i = make_tok(TokenType::IntegerLiteral, "1");
    auto s = make_tok(TokenType::StringLiteral, "x");
    auto id = make_tok(TokenType::Identifier, "x");
    CHECK(is_literal(i));
    CHECK(is_literal(s));
    CHECK_FALSE(is_literal(id));
  }
}
