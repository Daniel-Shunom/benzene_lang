#pragma once

#include <ether/types/types.hpp>
#include <vector>

struct Constraint {
  TypePtr lhs;
  TypePtr rhs;
};

using Constraints = std::vector<Constraint>;
