#pragma once

#include <ether/types/constraints.hpp>
#include <ether/types/type_printer.hpp>
#include <iosfwd>

class ConstraintPrinter {
public:
  explicit ConstraintPrinter(std::ostream& out, bool use_color = true)
    : out(out), type_printer(use_color) {}

  void print(const Constraints& constraints) const;

private:
  std::ostream& out;
  TypePrinter type_printer;
};
