#include "check.hpp"
#include "ether/ast_passes/scope_resolution/scope_res.hpp"
#include "ether/ast_passes/type_check/type_check.hpp"
#include "ether/types/constraint_printer.hpp"
#include "ether/types/unification_printer.hpp"
#include "files.hpp"

#include <ether/ast_passes/print/print.hpp>
#include <ether/ast_passes/symbol_resolver/symbol_resolver.hpp>
#include <ether/module/module.hpp>

#include <cstdio>
#include <print>
#include <string>
#include <utility>

int HandleCheck(const ArgCheck& a) {
  std::string source = FileToString(a.path);
  if (source.empty()) {
    std::println(stderr, "ether: could not read source file `{}`", a.path);
    return 1;
  }

  Module mod(a.path, std::move(source));
  mod.generate_ast();

  ScopeRes scope_resolver(mod.get_symbol_storage(), mod.get_diag_engine());
  SymbolResolver resolver(mod.get_symbol_storage(), mod.get_diag_engine());
  TypeChecker type_checker(a.show_types, mod.get_diag_engine());

  mod.attach_visitor(resolver);
  mod.attach_visitor(scope_resolver);
  mod.attach_visitor(type_checker);
  mod.apply_visitors();
  type_checker.unify_constraints();

  TreePrinter printer(std::cout, a.show_types, &type_checker.substitutions());

  // Print after type checking so inferred types are visible on AST nodes.
  if (a.show_ast || a.show_types) {
    mod.apply_visitor(printer);
  }

  mod.set_exports(resolver.take_exports());
  if (a.show_constraints) {
    ConstraintPrinter{std::cout}.print(type_checker.constraints());
  }
  if (a.show_unification) {
    UnificationPrinter{std::cout}.print(type_checker.substitutions());
  }
  mod.print_errors();

  return 0;
}
