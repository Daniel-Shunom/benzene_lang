#include "ether/ast_passes/type_check/modules/unify.hpp"
#include "ether/types/types.hpp"
#include "ether/types/type_printer.hpp"
#include <variant>

auto Unifier::report_failure(std::string message, TypePtr lhs,
                             TypePtr rhs) -> void {
  if (lhs || rhs) {
    message += " (" + TypePrinter{}.print(lhs) + " ~ "
             + TypePrinter{}.print(rhs) + ")";
  }

  Diagnostic diagnostic;
  diagnostic.level = DiagnosticLevel::Fail;
  diagnostic.phase = DiagnosticPhase::TypeChecker;
  diagnostic.location = {.line = 1, .column = 1};
  diagnostic.message = std::move(message);
  diagnostic_storage.push_back(diagnostic);
  diag_engine.report(diagnostic);
}

auto Unifier::occurs(TypeVarId id, TypePtr type,
                     std::unordered_set<const Type*>& visited) -> bool {
  type = resolve(type);
  if (!type) {
    return false;
  }

  if (!visited.insert(type.get()).second) {
    return false;
  }

  if (type->isTypeVar()) {
    return std::get<TypeVar>(type->value).get_id() == id;
  }

  if (type->isFunctionType()) {
    const auto& function = std::get<FunctionType>(type->value);
    for (const auto& parameter : function.get_param_types()) {
      if (occurs(id, parameter, visited)) {
        return true;
      }
    }
    return occurs(id, function.get_return_type(), visited);
  }

  if (type->isTypeConstructor()) {
    for (const auto& argument:
         std::get<TypeConstructor>(type->value).get_args()) {
      if (occurs(id, argument, visited)) {
        return true;
      }
    }
  }

  if (type->isPmtType()) {
    for (const auto& field: std::get<PmtType>(type->value).get_fields()) {
      if (occurs(id, field.get_type(), visited)) {
        return true;
      }
    }
  }

  return false;
}
void Unifier::solve(const Constraints& constraints) {
  for (const auto& constraint : constraints) {
    unify(constraint.lhs, constraint.rhs);
  }
}

auto Unifier::resolve(TypePtr ptr) -> TypePtr {
  if (!ptr) {
    return ptr;
  }

  if (std::holds_alternative<TypeVar>(ptr->value)) {
    auto type_id = std::get<TypeVar>(ptr->value).get_id();
    auto found = subs_record.find(type_id);

    if (found == subs_record.end()) {
      return ptr;
    }

    auto resolved = resolve(found->second);
    found->second = resolved;
    return resolved;
  }

  return ptr;
}

auto Unifier::bind_variable(TypePtr variable, TypePtr type) -> void {
  if (std::holds_alternative<TypeVar>(variable->value)) {
    auto lhs_id = std::get<TypeVar>(variable->value).get_id();
    type = resolve(type);

    if (!type) {
      report_failure("Cannot bind a type variable to an unset type");
      return;
    }

    if (type && std::holds_alternative<TypeVar>(type->value)
        && std::get<TypeVar>(variable->value).get_id()
             == std::get<TypeVar>(type->value).get_id()) {
      return;
    }

    if (subs_record.find(lhs_id) != subs_record.end()) {
      unify(subs_record.at(lhs_id), type);
      return;
    }

    std::unordered_set<const Type*> visited;
    if (occurs(lhs_id, type, visited)) {
      report_failure("Infinite type detected", variable, type);
      return;
    }

    subs_record[lhs_id] = type;
    return;
  }
}

auto Unifier::unify_functions(TypePtr func1, TypePtr func2) -> void {
  if (std::holds_alternative<FunctionType>(func1->value)
      && std::holds_alternative<FunctionType>(func2->value)
  ) {
    auto lfunc = std::get<FunctionType>(func1->value);
    auto rfunc = std::get<FunctionType>(func2->value);

    if (lfunc.get_param_types().size() != rfunc.get_param_types().size()) {
      report_failure("Function argument counts do not match", func1, func2);
      return;
    }

    unify(lfunc.get_return_type(), rfunc.get_return_type());

    for (size_t i = 0; i < lfunc.get_param_types().size(); ++i) {
      unify(lfunc.get_param_types()[i], rfunc.get_param_types()[i]);
    }
  }
}

auto Unifier::unify_constructors(TypePtr lhs, TypePtr rhs) -> void {
  if (!std::holds_alternative<TypeConstructor>(lhs->value)
      || !std::holds_alternative<TypeConstructor>(rhs->value)) {
    return;
  }

  const auto& left = std::get<TypeConstructor>(lhs->value);
  const auto& right = std::get<TypeConstructor>(rhs->value);

  if (left.name() != right.name()
      || left.get_args().size() != right.get_args().size()) {
    report_failure("Type constructors do not match", lhs, rhs);
    return;
  }

  for (size_t i = 0; i < left.get_args().size(); ++i) {
    unify(left.get_args()[i], right.get_args()[i]);
  }
}

auto Unifier::unify(TypePtr lhs, TypePtr rhs) -> void {
  lhs = resolve(lhs);
  rhs = resolve(rhs);

  if (!lhs || !rhs || type_ptr_equal(lhs, rhs)) {
    return;
  }

  // Parsed built-in annotations and inferred literal types can currently use
  // different internal variants while representing the same displayed type.
  if (TypePrinter{}.print(lhs) == TypePrinter{}.print(rhs)) {
    return;
  }

  if (lhs->isTypeVar()) {
    bind_variable(lhs, rhs);
    return;
  }

  if (rhs->isTypeVar()) {
    bind_variable(rhs, lhs);
    return;
  }

  if (lhs->isFunctionType() && rhs->isFunctionType()) {
    unify_functions(lhs, rhs);
    return;
  }

  if (lhs->isTypeConstructor() && rhs->isTypeConstructor()) {
    unify_constructors(lhs, rhs);
    return;
  }

  report_failure("Types do not unify", lhs, rhs);
}
