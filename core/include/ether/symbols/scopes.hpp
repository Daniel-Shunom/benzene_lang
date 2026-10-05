#pragma once
#include <ether/symbols/symbol_types.hpp>

enum class ScopeType {
  Application,
  Module,
  CaseExpression,
  FunctionExpression,
  LambdaExpression,
  ScopedExpression,
};

inline auto scope_type_to_str(ScopeType& type) -> std::string {
  switch (type) {
    case ScopeType::Application: return "Application";
    case ScopeType::Module: return "Module";
    case ScopeType::FunctionExpression: return "FunctionExpression";
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

