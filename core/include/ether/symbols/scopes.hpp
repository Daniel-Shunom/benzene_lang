#pragma once
#include "symbol_types.hpp"

enum class ScopeType {
  Application,
  Module,
  CaseExpression,
  FunctionExpression,
  ScopedExpression,
};

inline std::string scope_type_to_str(ScopeType& type) {
  switch (type) {
    case ScopeType::Application: return "Application";
    case ScopeType::Module: return "Module";
    case ScopeType::FunctionExpression: return "FunctionExpression";
    case ScopeType::ScopedExpression: return "ScopedExpression";
    case ScopeType::CaseExpression: return "CaseExpression";
  }
}

struct Scope {
  ScopeType scope_type;
  SymTable scope_sym_table;

  ScopeType get_scope_type() const {
    return this->scope_type;
  }
};

