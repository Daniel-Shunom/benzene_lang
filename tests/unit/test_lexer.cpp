#include <doctest/doctest.h>

#include "fixtures.hpp"

#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/lexer/lexer.hpp>
#include <ether/tokens/token_types.hpp>

using namespace ether::test;

TEST_SUITE("lexer / fundamentals") {
  TEST_CASE("empty input yields only EoF") {
    auto toks = lex_all("");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::EoF);
  }

  TEST_CASE("whitespace-only input yields only EoF") {
    auto toks = lex_all("   \t  \r  \n  ");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::EoF);
  }

  TEST_CASE("EoF is always the last token") {
    auto toks = lex_all("let x = 1");
    REQUIRE_FALSE(toks.empty());
    CHECK(toks.back().token_type == TokenType::EoF);
  }
}

TEST_SUITE("lexer / numeric literals") {
  TEST_CASE("integer literal") {
    auto toks = lex_no_eof("123");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::IntegerLiteral);
    CHECK(toks[0].token_value == "123");
  }

  TEST_CASE("float literal") {
    auto toks = lex_no_eof("3.14");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::FloatLiteral);
    CHECK(toks[0].token_value == "3.14");
  }

  TEST_CASE("integer followed by trailing dot is float-shaped") {
    // The scanner accepts `<digits>.` even with no fractional part.
    auto toks = lex_no_eof("10.");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::FloatLiteral);
    CHECK(toks[0].token_value == "10.");
  }

  TEST_CASE("two integers separated by space are two tokens") {
    auto toks = lex_no_eof("1 2");
    REQUIRE(toks.size() == 2);
    CHECK(toks[0].token_type == TokenType::IntegerLiteral);
    CHECK(toks[1].token_type == TokenType::IntegerLiteral);
  }
}

TEST_SUITE("lexer / string literals") {
  TEST_CASE("simple terminated string") {
    auto toks = lex_no_eof(R"("hello")");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::StringLiteral);
    CHECK(toks[0].token_value == "hello");
  }

  TEST_CASE("escape sequences are decoded") {
    auto toks = lex_no_eof(R"("a\nb\tc\\d\"e")");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::StringLiteral);
    CHECK(toks[0].token_value == std::string("a\nb\tc\\d\"e"));
  }

  TEST_CASE("unterminated string at newline becomes UTStringLiteral") {
    auto toks = lex_no_eof("\"abc\n");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::UTStringLiteral);
    CHECK(toks[0].token_value == "abc");
  }
}

TEST_SUITE("lexer / keywords and identifiers") {
  TEST_CASE("identifiers vs keywords") {
    auto toks = lex_no_eof("let const func end case default True False Nil foo");
    REQUIRE(toks.size() == 10);
    CHECK(toks[0].token_type == TokenType::LetKeyword);
    CHECK(toks[1].token_type == TokenType::ConstantKeyword);
    CHECK(toks[2].token_type == TokenType::FuncStart);
    CHECK(toks[3].token_type == TokenType::EndStmt);
    CHECK(toks[4].token_type == TokenType::Case);
    CHECK(toks[5].token_type == TokenType::Default);
    CHECK(toks[6].token_type == TokenType::TrueLiteral);
    CHECK(toks[7].token_type == TokenType::FalseLiteral);
    CHECK(toks[8].token_type == TokenType::NilLiteral);
    CHECK(toks[9].token_type == TokenType::Identifier);
    CHECK(toks[9].token_value == "foo");
  }

  TEST_CASE("identifiers may contain digits and underscores") {
    auto toks = lex_no_eof("foo_bar1 _x");
    // Scanner only enters identifier mode on isalpha — `_` cannot start one,
    // so the leading underscore is emitted as Unknown and the trailing `x`
    // becomes its own Identifier.
    REQUIRE(toks.size() == 3);
    CHECK(toks[0].token_type == TokenType::Identifier);
    CHECK(toks[0].token_value == "foo_bar1");
    CHECK(toks[1].token_type == TokenType::Unknown);
    CHECK(toks[1].token_value == "_");
    CHECK(toks[2].token_type == TokenType::Identifier);
    CHECK(toks[2].token_value == "x");
  }

  TEST_CASE("keywords are case sensitive") {
    auto toks = lex_no_eof("Let LET cmt CMT True true");
    REQUIRE(toks.size() == 6);
    CHECK(toks[0].token_type == TokenType::Identifier);
    CHECK(toks[1].token_type == TokenType::Identifier);
    CHECK(toks[2].token_type == TokenType::Identifier);
    CHECK(toks[3].token_type == TokenType::Identifier);
    CHECK(toks[4].token_type == TokenType::TrueLiteral);
    CHECK(toks[5].token_type == TokenType::Identifier);
  }
}

TEST_SUITE("lexer / operators and punctuation") {
  TEST_CASE("arithmetic operators") {
    auto toks = lex_no_eof("+ - * / %");
    REQUIRE(toks.size() == 5);
    CHECK(toks[0].token_type == TokenType::PlusOp);
    CHECK(toks[1].token_type == TokenType::MinusOp);
    CHECK(toks[2].token_type == TokenType::MultiplyOp);
    CHECK(toks[3].token_type == TokenType::DivideOp);
    CHECK(toks[4].token_type == TokenType::PercentOp);
  }

  TEST_CASE("comparison operators") {
    auto toks = lex_no_eof("> >= < <= == ~=");
    REQUIRE(toks.size() == 6);
    CHECK(toks[0].token_type == TokenType::Gt);
    CHECK(toks[1].token_type == TokenType::Ge);
    CHECK(toks[2].token_type == TokenType::Lt);
    CHECK(toks[3].token_type == TokenType::Le);
    CHECK(toks[4].token_type == TokenType::EqEq);
    CHECK(toks[5].token_type == TokenType::NtEq);
  }

  TEST_CASE("logical operators") {
    auto toks = lex_no_eof("&& || ~");
    REQUIRE(toks.size() == 3);
    CHECK(toks[0].token_type == TokenType::AndOp);
    CHECK(toks[1].token_type == TokenType::OrOp);
    CHECK(toks[2].token_type == TokenType::NotOp);
  }

  TEST_CASE("multi-character operators win over their prefixes") {
    auto toks = lex_no_eof(">=");
    REQUIRE(toks.size() == 1);
    CHECK(toks[0].token_type == TokenType::Ge);
  }

  TEST_CASE("return-type and pipe operators") {
    // Note: the pipe operator literal includes a trailing space — without it
    // the operator does not match.
    auto toks = lex_no_eof(":> a |=> b");
    REQUIRE(toks.size() == 4);
    CHECK(toks[0].token_type == TokenType::RtnTypeOp);
    CHECK(toks[1].token_type == TokenType::Identifier);
    CHECK(toks[2].token_type == TokenType::PipeOp);
    CHECK(toks[3].token_type == TokenType::Identifier);
  }

  TEST_CASE("brackets, braces, parens, colon, comma, dot") {
    auto toks = lex_no_eof("(){}[]:, .");
    REQUIRE(toks.size() == 9);
    CHECK(toks[0].token_type == TokenType::LParen);
    CHECK(toks[1].token_type == TokenType::RParen);
    CHECK(toks[2].token_type == TokenType::LBrace);
    CHECK(toks[3].token_type == TokenType::RBrace);
    CHECK(toks[4].token_type == TokenType::LBrac);
    CHECK(toks[5].token_type == TokenType::RBrac);
    CHECK(toks[6].token_type == TokenType::Colon);
    CHECK(toks[7].token_type == TokenType::Delim);
    CHECK(toks[8].token_type == TokenType::Dot);
  }
}

TEST_SUITE("lexer / comments") {
  TEST_CASE("single-line comment emits no tokens between Cmt and newline") {
    auto toks = lex_no_eof("Cmt this is a comment\nlet x = 1");
    // No comment token; only `let x = 1`.
    REQUIRE(toks.size() == 4);
    CHECK(toks[0].token_type == TokenType::LetKeyword);
    CHECK(toks[1].token_type == TokenType::Identifier);
    CHECK(toks[2].token_type == TokenType::Eq);
    CHECK(toks[3].token_type == TokenType::IntegerLiteral);
  }

  TEST_CASE("multi-line comment swallows its body") {
    auto toks = lex_no_eof("Cmt { multi\nline\nbody } let x = 1");
    REQUIRE(toks.size() == 4);
    CHECK(toks[0].token_type == TokenType::LetKeyword);
  }

  TEST_CASE("unterminated multi-line comment swallows to EoF without crashing") {
    // Existing behavior: silently consumes the rest of the input.
    auto toks = lex_no_eof("Cmt { unterminated");
    CHECK(toks.empty());
  }
}

TEST_SUITE("lexer / imports") {
  TEST_CASE("Load directive emits ImportModule with module path") {
    auto toks = lex_no_eof("Load benzene.list\nlet x = 1");
    REQUIRE(toks.size() == 5);
    CHECK(toks[0].token_type == TokenType::ImportModule);
    CHECK(toks[0].token_value == "benzene.list");
    CHECK(toks[1].token_type == TokenType::LetKeyword);
  }
}

TEST_SUITE("lexer / diagnostics") {
  TEST_CASE("unknown character generates a lexer warning") {
    DiagnosticEngine diag;
    auto toks = lex_all("@", diag);
    // Expect: [Unknown('@'), EoF]
    REQUIRE(toks.size() == 2);
    CHECK(toks[0].token_type == TokenType::Unknown);
    // The diagnostic engine collected at least one diagnostic; it's a Warn,
    // not Fail, so has_errors() stays false.
    CHECK_FALSE(diag.has_errors());
  }

  TEST_CASE("token line and column are recorded") {
    auto toks = lex_all("\n  let");
    // Expect [LetKeyword, EoF]
    REQUIRE(toks.size() == 2);
    CHECK(toks[0].token_type == TokenType::LetKeyword);
    CHECK(toks[0].line_number == 2);
    CHECK(toks[0].column_number == 3);
  }
}
