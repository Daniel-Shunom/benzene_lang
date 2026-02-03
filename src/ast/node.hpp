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
  virtual ~ASTNode() = default;
};

using ASTPtr = std::unique_ptr<ASTNode>;
using Case = std::pair<std::vector<ASTPtr>, ASTNode*>;

struct LiteralExpr: ASTNode {
  Token literal;
};

struct IdentifierExpr: ASTNode {
  Symbol* identifier_sym;
};

struct BinOpExpr: ASTNode {
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

struct FunctionExpr: ASTNode {
  Symbol* symbol;
  std::vector<ASTPtr> params;
  ASTPtr body;
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

