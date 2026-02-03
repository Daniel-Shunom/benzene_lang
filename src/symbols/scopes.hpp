#pragma once
#include "symbol_types.hpp"

enum class ScopeType {
  Application,
  Module,
  Function,
};

struct Scope {
  ScopeType scope_type;
  SymTable scope_sym_table;
};
