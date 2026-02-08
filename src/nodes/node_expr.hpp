#pragma once
#include "../tokens/token_types.hpp"
#include "../symbols/symbol_types.hpp"
#include "../typing/types.hpp"
#include <optional>
#include <memory>
#include <vector>

class Visitor;
struct Node {
  std::optional<Token> type;
  virtual ~Node() = default;
  virtual void accept(Visitor&) = 0;
};

using NDPtr = std::unique_ptr<Node>;

struct FuncParam {
  std::string param_name;
  std::optional<Token> param_type;
};

struct NDLiteral: Node {
  Token literal;
  void accept(Visitor&) override;
};

struct NDUnaryExpr: Node {
  Token op;
  NDPtr rhs;
  void accept(Visitor &) override;
};

struct NDBinaryExpr: Node {
  NDPtr lhs;
  Token op;
  NDPtr rhs;
  void accept(Visitor&) override;
};

struct NDScopeExpr: Node {
  std::vector<NDPtr> expressions;
  void accept(Visitor &) override;
};

struct NDImportDirective: Node {
  Token import_directive;
  void accept(Visitor&) override;
};

struct NDIdentifier: Node {
  Symbol* identifier_symbol;
  Token identifier;
  void accept(Visitor&) override;
};

struct NDLetBindExpr: Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDPtr bound_value;
  void accept(Visitor&) override;
};

struct NDConstExpr: Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDLiteral literal;
  void accept(Visitor&) override;
};

struct NDCallExpr: Node {
  std::unique_ptr<NDIdentifier> identifier;
  std::vector<NDPtr> args;
  void accept(Visitor&) override;
};

struct NDCallChain: Node {
  std::vector<NDPtr> calls;
  void accept(Visitor&) override;
};

struct NDFuncDeclExpr: Node {
  Token func_identifier;
  std::optional<Token> return_type;
  std::vector<FuncParam> func_params;
  std::vector<NDPtr> func_body;
  void accept(Visitor&) override;
};

struct NDCaseExpr: Node {
  struct Branch {
    std::vector<NDPtr> pattern;
    NDPtr result;
  };
  std::vector<NDPtr> conditions;
  std::vector<Branch> branches;
  void accept(Visitor&) override;
};

struct Parent {
  std::vector<NDPtr> children;
};
