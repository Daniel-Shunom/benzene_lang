#include "cmd.hpp"
#include "files.hpp"
#include <ether/ast/print/print.hpp>
#include <ether/ast/sym_res/sym_res.hpp>
#include <ether/module/module.hpp>

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

int main(int argc, char* argv[]) {
#ifdef _WIN32
  // Tree connectors and diagnostic glyphs are emitted as UTF-8. cmd.exe's
  // default OEM code page would render them as garbage; switching to CP
  // 65001 makes modern Windows consoles render them correctly.
  SetConsoleOutputCP(CP_UTF8);
#endif

  Args args = GetArgs(argc, argv);

  std::string source = FileToString(args.file_path);
  if (source.empty()) {
    std::fprintf(stderr, "ether: could not read source file `%s`\n",
                 args.file_path.c_str());
    return 1;
  }

  Module main(args.file_path, std::move(source));
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
