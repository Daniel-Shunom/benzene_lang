#include <ether/types/constraint_printer.hpp>
#include <ostream>

void ConstraintPrinter::print(const Constraints& constraints) const {
  for (const auto& constraint : constraints) {
    out << "[constraint] "
        << type_printer.print(constraint.lhs)
        << " ~ "
        << type_printer.print(constraint.rhs)
        << '\n';
  }
}
