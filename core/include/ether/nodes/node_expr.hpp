#pragma once
#include <ether/symbols/symbol_types.hpp>
#include <ether/tokens/token_types.hpp>
#include <ether/types/types.hpp>
#include <memory>
#include <optional>
#include <vector>

class Visitor;
struct Node {
  TypePtr inferred_type;
  bool type_is_resolved;
  bool is_poisoned = false;
  virtual ~Node() = default;
  virtual void accept(Visitor &) = 0;
};

using NDPtr = std::unique_ptr<Node>;

// Node for containing explicit type annotations.
struct NDExplicitTypeAnot: Node {
  TypePtr explicit_type;
  void accept(Visitor & visitor) override;
};

struct NDTypeExpr: Node {
  TypePtr parsed_type;
  void accept(Visitor & visitor) override;
};

struct NDTypeDecl: Node {
  Token type_identifier;
  std::optional<NDTypeExpr> alias_target;
  // A body is distinct from a bare declaration, even when it has no members.
  std::optional<std::vector<NDTypeExpr>> sub_types;

  void accept(Visitor & visitor) override;
};

struct NDIdentifier : Node {
  SymbolAttr *identifier_symbol;
  Token identifier;
  std::optional<NDTypeExpr> type;
  void accept(Visitor & visitor) override;
};

struct NDFuncParam: Node {
  NDIdentifier identifier;
  std::optional<NDTypeExpr> param_type;
  SymbolAttr *param_sym;
  void accept(Visitor & visitor) override;
};

struct NDLiteral : Node {
  Token literal;
  void accept(Visitor & visitor) override;
};

struct NDUnaryExpr : Node {
  std::optional<Token> op;
  NDPtr rhs;
  void accept(Visitor & visitor) override;
};

struct NDBinaryExpr : Node {
  NDPtr lhs;
  Token op;
  NDPtr rhs;
  void accept(Visitor & visitor) override;
};

struct NDScopeExpr : Node {
  Token open_brace;
  std::vector<NDPtr> expressions;
  void accept(Visitor & visitor) override;
};

struct NDListExpr : Node {
  Token open_brac;
  std::vector<NDPtr> values;
  void accept(Visitor & visitor) override;
};

struct NDTupleExpr : Node {
  Token at_sym;
  std::vector<NDPtr> values;
  void accept(Visitor & visitor) override;
};

struct NDImportDirective : Node {
  Token import_directive;
  void accept(Visitor & visitor) override;
};

struct NDLetBindExpr : Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDPtr bound_value;
  void accept(Visitor & visitor) override;
};

struct NDConstExpr : Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDPtr bound_value;
  void accept(Visitor & visitor) override;
};

struct NDCallExpr : Node {
  std::unique_ptr<NDIdentifier> identifier;
  std::vector<NDPtr> args;
  void accept(Visitor & visitor) override;
};

struct NDCallChain : Node {
  Token start_token;
  std::vector<NDPtr> calls;
  void accept(Visitor & visitor) override;
};

struct NDFuncDeclExpr : Node {
  Token func_identifier;
  SymbolAttr *func_sym;
  std::optional<NDTypeExpr> return_type;
  std::vector<NDFuncParam> func_params;
  std::vector<NDPtr> func_body;
  void accept(Visitor & visitor) override;
};

struct NDLambdaExpr : Node {
  SymbolAttr *func_sym;
  Token lambda_start;
  std::optional<NDTypeExpr> return_type;
  std::vector<NDFuncParam> func_params;
  std::vector<NDPtr> func_body;
  void accept(Visitor & visitor) override;
};

struct NDCaseExpr : Node {
  struct Branch {
    std::vector<NDPtr> pattern;
    NDPtr result;
  };
  Token case_keyword;
  std::vector<NDPtr> conditions;
  std::vector<Branch> branches;
  void accept(Visitor & visitor) override;
};

struct Parent {
  std::vector<NDPtr> children;
  std::vector<Visitor *> visitors;

  Parent() = default;

  Parent(const Parent &) = delete;
  auto operator=(const Parent &) -> Parent & = delete;

  Parent(Parent &&) = default;
  auto operator=(Parent &&) -> Parent & = default;

  void add_visitor(Visitor &visitor) { this->visitors.push_back(&visitor); }

  void apply_visitors() {
    for (auto &node : children) {
      for (auto &visitor : visitors) {
        node->accept(*visitor);
      }
    }
  }
};
