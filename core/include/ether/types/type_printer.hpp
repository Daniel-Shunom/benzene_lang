#pragma once
#include <ether/types/types.hpp>
#include <string>

// Pure formatting of the type model; independent of AST nodes and resolution.
class TypePrinter {
public:
  explicit TypePrinter(bool use_color = false) : use_color(use_color) {}

  [[nodiscard]] auto print(const TypePtr& type) const -> std::string;
  [[nodiscard]] auto print(const Type& type) const -> std::string;
private:
  bool use_color;
};
