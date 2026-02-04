#pragma once
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include "../symbols/symbol_types.hpp"

struct Type;
using TypeRef = std::shared_ptr<Type>;

struct ASTNode {
  TypeRef type_ref;
  bool poisoned;
  bool is_poisoned() {
    return this->poisoned;
  }
  virtual ~ASTNode() = default;
};

using ASTPtr = std::unique_ptr<ASTNode>;
using Case = std::pair<std::vector<ASTPtr>, ASTNode*>;

struct SeqNode: ASTNode {
  SeqNode(std::vector<ASTPtr> nodes) {
    this->children = std::move(nodes);
  }
  std::vector<ASTPtr> children;
};

struct TokNode: ASTNode {
  TokNode(Token tok) { this->token=tok;}
  Token token;
};

struct ImportStmt: ASTNode {
  Symbol* symbol;
  Token import_token;
};

struct LiteralExpr: ASTNode {
  Token literal;
};

struct IdentifierExpr: ASTNode {
  Symbol* identifier_sym;
};

struct BinOpExpr: ASTNode {
  BinOpExpr(ASTPtr left, std::string op, ASTPtr right) {
    this->left = std::move(left);
    this->op = op;
    this->right = std::move(right);
  }
  ASTPtr left;
  std::string op;
  ASTPtr right;
};

struct UnaryOpExpr: ASTNode {
  std::string op;
  ASTPtr right;
};

struct LetBindExpr: ASTNode {
  Symbol* symbol;
  ASTPtr value;
};

struct FuncInvocExpr: ASTNode {
  Symbol* symbol;
  std::vector<ASTPtr> params;
};

struct CaseExpr: ASTNode {
  Symbol* symbol;
  std::vector<ASTPtr> conditions;
  std::vector<Case> cases;
};

struct FunctionDeclExpr: ASTNode {
  Symbol* symbol;
  std::vector<ASTPtr> params;
  ASTPtr body;
};

struct FunctionDeclBody: ASTNode {
  std::vector<ASTPtr> statements;
};

struct ConstExpr: ASTNode {
  Symbol* symbol;
  ASTPtr value;
};

struct ModuleNode: ASTNode {
  Symbol* symbol;
  std::vector<ASTPtr> module_nodes;
};

struct ProgramNode: ASTNode {
  Symbol* symbol;
  std::vector<ASTPtr> program_nodes;
};

