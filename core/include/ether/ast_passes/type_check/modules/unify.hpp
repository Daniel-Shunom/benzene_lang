#pragma once

#include <ether/diagnostics/diagnostic.hpp>
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/types/constraints.hpp>
#include "ether/types/types.hpp"
#include <unordered_set>
#include <vector>

class Unifier {
  Subst subs_record;
  std::vector<Diagnostic> diagnostic_storage;
  DiagnosticEngine& diag_engine;

  auto report_failure(std::string message, TypePtr lhs = nullptr,
                      TypePtr rhs = nullptr) -> void;
  auto occurs(TypeVarId id, TypePtr type,
              std::unordered_set<const Type*>& visited) -> bool;

public:
  explicit Unifier(DiagnosticEngine& diagnostics)
    : diag_engine(diagnostics) {}

  void solve(const Constraints& constraints);
  [[nodiscard]] auto substitutions() const noexcept -> const Subst& {
    return subs_record;
  }

  [[nodiscard]] auto diagnostics() const noexcept
      -> const std::vector<Diagnostic>& { return diagnostic_storage; }

private:
  auto resolve(TypePtr) -> TypePtr;
  auto unify(TypePtr lhs, TypePtr rhs) -> void;
  auto unify_functions(TypePtr func1, TypePtr func2) -> void;
  auto unify_constructors(TypePtr lhs, TypePtr rhs) -> void;
  auto bind_variable(TypePtr variable, TypePtr type) -> void;
};
