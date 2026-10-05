#pragma once

#include <ether/types/types.hpp>
#include <iosfwd>

class UnificationPrinter {
public:
  explicit UnificationPrinter(std::ostream& output) : out(output) {}

  void print(const Subst& substitutions) const;

private:
  std::ostream& out;
};
