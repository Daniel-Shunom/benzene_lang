#include <doctest/doctest.h>
#include "fixtures.hpp"
#include <ether/types/type_printer.hpp>
#include <ether/ast_passes/print/print.hpp>
#include <sstream>
#include <regex>

using namespace ether::test;

TEST_SUITE("types / printer") {
  TEST_CASE("formats every type variant without resolving or modifying it") {
    TypePrinter printer;
    CHECK(printer.print(makeInt()) == "Int");
    CHECK(printer.print(makeFloat()) == "Float");
    CHECK(printer.print(makeString()) == "String");
    CHECK(printer.print(makeBool()) == "Bool");
    CHECK(printer.print(makeNil()) == "Nil");
    CHECK(printer.print(Type(TypeVar{7})) == "'t7");
    CHECK(printer.print(makeTypeConstructor("Unknown", {})) == "Unknown");
    CHECK(printer.print(makeDict(makeString(), makeList(makeInt()))) == "Dict(String, List(Int))");
    Type fields(PmtType{"Record", {TypeField{"value", makeInt()}, TypeField{"items", makeList(makeString())}}});
    CHECK(printer.print(fields) == "Record(value: Int, items: List(String))");
    CHECK(printer.print(TypePtr{}) == "<unset>");
  }

  TEST_CASE("function formatting respects parameters followed by the return type") {
    TypePrinter printer;
    CHECK(printer.print(makeFunc(makeString(), makeList(makeInt()))) == "Fn(String) :> List(Int)");
    CHECK(printer.print(makeTypeConstructor("Fn", {makeInt()})) == "Fn() :> Int");
    CHECK(printer.print(makeTypeConstructor("Fn", {makeInt(), makeString(), makeBool()})) == "Fn(Int, String) :> Bool");
    CHECK(printer.print(makeFunc(makeFunc(makeInt(), makeString()), makeBool())) == "Fn(Fn(Int) :> String) :> Bool");
  }

  TEST_CASE("shared subtrees are printed normally and cycles terminate") {
    TypePrinter printer;
    auto value = makeInt();
    CHECK(printer.print(makeTuple({value, value})) == "Tuple(Int, Int)");
    auto recursive = makeTypeConstructor("Loop", {});
    recursive->value = TypeConstructor("Loop", {recursive});
    CHECK(printer.print(recursive) == "Loop(<recursive>)");
    recursive->value = BaseType::Int; // Release the deliberately created ownership cycle.
    CHECK(printer.print(recursive) == "Int");
  }

  TEST_CASE("the AST printer delegates alias targets to the type printer") {
    DiagnosticEngine diag;
    auto state = make_state(lex_all("type Converter = Fn(String) :> List(Int)", diag), diag);
    auto declaration = parse_type_declaration()(state);
    REQUIRE(declaration);
    REQUIRE(declaration->alias_target);
    const auto original = declaration->alias_target->parsed_type;
    std::ostringstream output;
    TreePrinter printer(output);
    declaration->accept(printer);
    CHECK(output.str().find("Converter") != std::string::npos);
    const auto plain = std::regex_replace(output.str(), std::regex("\x1b\\[[0-9;]*m"), "");
    CHECK(plain.find("Fn(String) :> List(Int)") != std::string::npos);
    CHECK(output.str().find("\033[35mFn\033[0m") != std::string::npos);
    CHECK(declaration->alias_target->parsed_type == original);
    CHECK(diag.all().empty());
  }
}


TEST_CASE("the AST printer displays multivariate subtype expressions") {
  DiagnosticEngine diag;
  auto state = make_state(lex_all("type Data { Integer Custom(value: List(Int)) Fn(Int) :> String }", diag), diag);
  auto declaration = parse_type_declaration()(state);
  REQUIRE(declaration);
  std::ostringstream output;
  TreePrinter printer(output);
  declaration->accept(printer);
  const auto plain = std::regex_replace(output.str(), std::regex("\x1b\\[[0-9;]*m"), "");
  CHECK(plain.find("subtypes") != std::string::npos);
  CHECK(plain.find("Integer") != std::string::npos);
  CHECK(plain.find("Custom(value(List(Int)))") != std::string::npos);
  CHECK(plain.find("Fn(Int) :> String") != std::string::npos);
}
