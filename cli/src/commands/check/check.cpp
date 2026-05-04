#include "check.hpp"
#include "files.hpp"

#include <ether/ast/print/print.hpp>
#include <ether/ast/sym_res/sym_res.hpp>
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
  SymResolver resolver(mod.get_symbol_storage(), mod.get_diag_engine());

  if (a.show_ast) mod.attach_visitor(printer);
  mod.attach_visitor(resolver);
  mod.apply_visitors();

  mod.set_exports(resolver.take_exports());
  mod.print_errors();

  return 0;
}
