#pragma once

#include <variant>
#include "../types/types_t.hpp"

typedef struct {
  Type type;
  std::variant<
    int, 
    double, 
    std::string, 
    bool,
    std::monostate,
    std::shared_ptr<Function>
  > value;
} Value;
