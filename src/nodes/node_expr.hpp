#pragma once
#include "../symbols/symbol_types.hpp"
#include "../tokens/token_types.hpp"
#include <memory>
#include <optional>
#include <vector>

class Visitor;
struct Node {
  std::optional<Token> type;
  bool is_poisoned = false;
  virtual ~Node() = default;
  virtual void accept(Visitor &) = 0;
};

using NDPtr = std::unique_ptr<Node>;

struct FuncParam {
  Token param_token;
  std::optional<Token> param_type;
  SymbolAttr *param_sym;
};

struct NDLiteral : Node {
  Token literal;
  void accept(Visitor &) override;
};

struct NDUnaryExpr : Node {
  std::optional<Token> op;
  NDPtr rhs;
  void accept(Visitor &) override;
};

struct NDBinaryExpr : Node {
  NDPtr lhs;
  Token op;
  NDPtr rhs;
  void accept(Visitor &) override;
};

struct NDScopeExpr : Node {
  Token open_brace;
  std::vector<NDPtr> expressions;
  void accept(Visitor &) override;
};

struct NDImportDirective : Node {
  Token import_directive;
  void accept(Visitor &) override;
};

struct NDIdentifier : Node {
  SymbolAttr *identifier_symbol;
  Token identifier;
  void accept(Visitor &) override;
};

struct NDLetBindExpr : Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDPtr bound_value;
  void accept(Visitor &) override;
};

struct NDConstExpr : Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDLiteral literal;
  void accept(Visitor &) override;
};

struct NDCallExpr : Node {
  std::unique_ptr<NDIdentifier> identifier;
  std::vector<NDPtr> args;
  void accept(Visitor &) override;
};

struct NDCallChain : Node {
  Token start_token;
  std::vector<NDPtr> calls;
  void accept(Visitor &) override;
};

struct NDFuncDeclExpr : Node {
  Token func_identifier;
  SymbolAttr *func_sym;
  std::optional<Token> return_type;
  std::vector<FuncParam> func_params;
  std::vector<NDPtr> func_body;
  void accept(Visitor &) override;
};

struct NDCaseExpr : Node {
  struct Branch {
    std::vector<NDPtr> pattern;
    NDPtr result;
  };
  Token case_keyword;
  std::vector<NDPtr> conditions;
  std::vector<Branch> branches;
  void accept(Visitor &) override;
};

struct Parent {
  std::vector<NDPtr> children;
  std::vector<Visitor *> visitors;

  Parent() = default;

  Parent(const Parent &) = delete;
  Parent &operator=(const Parent &) = delete;

  Parent(Parent &&) = default;
  Parent &operator=(Parent &&) = default;

  void add_visitor(Visitor &v) { this->visitors.push_back(&v); }

  void apply_visitors() {
    for (auto &node : children) {
      for (auto &visitor : visitors) {
        node->accept(*visitor);
      }
    }
  }
};
