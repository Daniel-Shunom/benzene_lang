#pragma once
#include "symbol_types.hpp"

enum class ScopeType {
  Application,
  Module,
  CaseExpression,
  FunctionExpression,
  ScopedExpression,
};

struct Scope {
  ScopeType scope_type;
  SymTable scope_sym_table;

  ScopeType get_scope_type() const {
    return this->scope_type;
  }
};

