#pragma once
#include "../tokens/token_types.hpp"
#include "../symbols/symbol_types.hpp"
#include "../typing/types.hpp"
#include <optional>
#include <memory>
#include <vector>

struct Node {
  std::optional<Token> type;
  virtual ~Node() = default;
};

struct FuncParam {
  std::string param_name;
  std::optional<Token> param_type;
};

using NDPtr = std::unique_ptr<Node>;

struct NDLiteral: Node {
  Token literal;
};

struct NDImportDirective: Node {
  Token import_directive;
};

struct NDIdentifier: Node {
  Symbol* identifier_symbol;
  Token identifier;
};

struct NDLetBindExpr: Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDPtr bound_value;
};

struct NDConstExpr: Node {
  std::unique_ptr<NDIdentifier> identifier;
  NDLiteral literal;
};

struct NDCallExpr: Node {
  std::unique_ptr<NDIdentifier> identifier;
  std::vector<NDPtr> args;
};

struct NDCallChain: Node {
  std::vector<NDPtr> calls;
};

struct NDFuncDeclExpr: Node {
  Token func_identifier;
  std::optional<Token> return_type;
  std::vector<FuncParam> func_params;
  std::vector<NDPtr> func_body;
};

struct NDCaseExpr: Node {
  struct Branch {
    std::vector<NDPtr> pattern;
    NDPtr result;
  };
  std::vector<NDPtr> conditions;
  std::vector<Branch> branches;
};

struct Parent {
  std::vector<NDPtr> children;
};
