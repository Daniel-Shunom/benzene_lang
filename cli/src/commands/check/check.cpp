#include "check.hpp"
#include "ether/ast_passes/scope_resolution/scope_res.hpp"
#include "ether/ast_passes/type_check/type_check.hpp"
#include "files.hpp"

#include <ether/ast_passes/print/print.hpp>
#include <ether/ast_passes/symbol_resolver/symbol_resolver.hpp>
#include <ether/module/module.hpp>

#include <cstdio>
#include <string>
#include <utility>

int HandleCheck(const ArgCheck& a) {
  std::string source = FileToString(a.path);
  if (source.empty()) {
    std::fprintf(stderr, "ether: could not read source file `%s`\n", a.path.c_str());
    return 1;
  }

  Module mod(a.path, std::move(source));
  mod.generate_ast();

  TreePrinter printer;
  ScopeRes scope_resolver(mod.get_symbol_storage(), mod.get_diag_engine());
  SymbolResolver resolver(mod.get_symbol_storage(), mod.get_diag_engine());
  TypeChecker type_checker;

  if (a.show_ast) mod.attach_visitor(printer);
  // mod.attach_visitor(resolver);
  mod.attach_visitor(scope_resolver);
  mod.attach_visitor(type_checker);
  mod.apply_visitors();

  mod.set_exports(resolver.take_exports());
  mod.print_errors();

  return 0;
}
