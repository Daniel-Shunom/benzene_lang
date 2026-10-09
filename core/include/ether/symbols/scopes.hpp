#pragma once
#include <ether/symbols/symbol_types.hpp>

enum class ScopeType {
  Application,
  Module,
  CaseExpression,
  TypeDeclaration,
  FunctionDeclaration,
  LambdaExpression,
  ScopedExpression,
};

inline auto scope_type_to_str(ScopeType& type) -> std::string {
  switch (type) {
    case ScopeType::TypeDeclaration: return "TypeDeclaration";
    case ScopeType::Application: return "Application";
    case ScopeType::Module: return "Module";
    case ScopeType::FunctionDeclaration: return "FunctionDeclaration";
    case ScopeType::ScopedExpression: return "ScopedExpression";
    case ScopeType::CaseExpression: return "CaseExpression";
    case ScopeType::LambdaExpression: return "LambdaExpression";
  }
}

struct Scope {
  ScopeType scope_type;
  SymTable scope_sym_table;

  [[nodiscard]] auto get_scope_type() const -> ScopeType {
    return this->scope_type;
  }
};

