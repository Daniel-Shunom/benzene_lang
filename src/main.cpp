#include "../lib/files.hpp"
#include "ast/print/print.hpp"
#include "ast/sym_res/sym_res.hpp"
#include "command_line/cmd.hpp"
#include "diagnostics/diagnostic_eng.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser_types.hpp"
#include "parser/parsers.hpp"
#include <cstdio>
#include <string_view>

int main(int argc, char* argv[]) {

  Args args = GetArgs(argc, argv);
  DiagnosticEngine diag_eng;

  std::string src_code = FileToString(args.file_path);
  std::string_view contents{src_code};

  Lexer lexer(contents, diag_eng);

  std::printf("\nLexing current file\n" "File path: %s\n", args.file_path.data());

  if (args.is_logs_enabled()) std::printf("[LOGS ENABLED]\n");

  lexer.scan_tokens();

  if (args.is_logs_enabled()) lexer.print_tokens();

  auto tokens = lexer.get_tokens();
  auto state  = ParserState(diag_eng);
  state.set_state(tokens);

  if (args.is_logs_enabled()) state.activate_logs();
  std::printf("Beginning AST Parsing\n");

  PResult<Parent> parent = run_parser(state);

  if(!parent) {
    std::printf("Parsing failed\n");
    return 0;
  }

  std::printf(
    "[MAIN] Parse Success!\n"
    "[MAIN] Total Captured Expressions: %zu\n",
    parent.value().children.size()
  );

  TreePrinter printer;
  SymResolver resolver(diag_eng);

  parent->add_visitor(resolver);
  parent->add_visitor(printer);
  parent->apply_visitors();

  if (args.is_logs_enabled()) {
    diag_eng.print_all();
  }

  return 0;
}
