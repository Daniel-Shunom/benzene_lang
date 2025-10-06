#pragma once

#include <algorithm>
#include <optional>
#include <unordered_map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "../ast/expr_l.hpp"
#include "../types/types_t.hpp"

using FuncBooklet = std::pair<FunctionDef, std::shared_ptr<std::vector<Expr>>>;
class FunctionRegistry {
public:
  void assign(std::string name, FuncBooklet def) {
    this->functions[name] = std::move(def);
  }

  std::optional<FuncBooklet> getDefinition(const std::string& name) {
    if (auto&& it = this->functions.find(name); it != this->functions.end()) {
      return it->second;
    }

    return std::nullopt;
  }

private:
  std::unordered_map<std::string, FuncBooklet> functions;
};
