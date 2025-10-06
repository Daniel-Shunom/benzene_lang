#pragma once
#include "../types/symbols_t.hpp"
#include <string>

typedef struct {
  TOKEN_TYPE tok_type;
  std::string tok_val;
  size_t tok_row;
  size_t tok_col;
} Token;

