#include "../lib/files.hpp"
#include "ast/print/print.hpp"
#include "ast/sym_res/sym_res.hpp"
#include "command_line/cmd.hpp"
#include "module/module.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // The AST printer and diagnostic engine emit UTF-8 box-drawing characters.
  // cmd.exe's default OEM code page would render those as garbage. Switching
  // to UTF-8 (CP 65001) lets modern Windows consoles render them correctly.
  SetConsoleOutputCP(CP_UTF8);
#endif

  Args args = GetArgs(argc, argv);

  Module main(args.file_path);
  main.generate_ast();

  TreePrinter printer;
  SymResolver resolver(main.get_symbol_storage(), main.get_diag_engine());

  main.attach_visitor(printer);
  main.attach_visitor(resolver);
  main.apply_visitors();

  main.set_exports(resolver.take_exports());
  main.print_errors();

  return 0;
}
