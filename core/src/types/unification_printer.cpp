#include <ether/types/type_printer.hpp>
#include <ether/types/unification_printer.hpp>
#include <ostream>

void UnificationPrinter::print(const Subst& substitutions) const {
  TypePrinter types(true);
  for (const auto& [id, type] : substitutions) {
    out << "[unify] 't" << id << " ~ " << types.print(type) << '\n';
  }
}
