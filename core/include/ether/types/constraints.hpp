#pragma once

#include <ether/diagnostics/diagnostic.hpp>
#include <ether/types/types.hpp>
#include <vector>

struct Constraint {
  TypePtr lhs;
  TypePtr rhs;

  // Where in the source this requirement came from. A unification failure is
  // reported here, so that calling a function with the wrong argument types
  // underlines the call rather than the top of the file.
  //
  // Zero means the constraint was generated without a token to blame.
  SourceLocation location{};
};

using Constraints = std::vector<Constraint>;
