#pragma once

#include <ether/diagnostics/diagnostic.hpp>
#include <ether/types/types.hpp>
#include <vector>
#include <optional>

struct Constraint {
  TypePtr lhs;
  TypePtr rhs;

  // Where in the source this requirement came from. A unification failure is
  // reported here, so that calling a function with the wrong argument types
  // underlines the call rather than the top of the file.
  //
  // Zero means the constraint was generated without a token to blame.
  SourceLocation location{};
  // Preserve the source annotation and polymorphic scheme through recursive
  // unification, which otherwise reports only the mismatching inner types.
  TypePtr expected_declared;
  std::optional<Scheme> expected_scheme;
};

using Constraints = std::vector<Constraint>;
