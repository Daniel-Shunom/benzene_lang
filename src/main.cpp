#include "../lib/files.hpp"
#include "ast/print/print.hpp"
#include "ast/sym_res/sym_res.hpp"
#include "cmd/cmd.hpp"
#include "lexer/lexer.hpp"
#include "parsers/combinator/parser_types.hpp"
#include "parsers/combinator/parsers.hpp"
#include <cstdio>
#include <string_view>

int main(int argc, char* argv[]) {

  Args args = GetArgs(argc, argv);
  std::string src_code = FileToString(args.file_path);
  std::string_view contents{src_code};
  Lexer lexer(contents);

  std::printf( "\nLexing current file\n" "File path: %s\n", args.file_path.data());

  if (args.is_logs_enabled()) std::printf("[LOGS ENABLED]\n");

  lexer.scan_tokens();

  auto tokens = lexer.get_tokens();
  auto state = ParserState();
  state.set_state(tokens);

  if (args.is_logs_enabled()) state.activivate_logs();
  std::printf("Beginning AST Parsing\n");

  PResult<Parent> parent = run_parser(state);

  if(!parent) {
    std::printf("Parsing failed, Displaying Parse Logs\n");
    state.display_logs();
    std::printf("Ended AST Parsing\n");
  } else {
    std::printf(
      "[MAIN] Parse Success!\n"
      "[MAIN] Total Captured Expressions: %zu\n",
      parent.value().children.size()
    );
    state.display_logs();
    state.display_captured_exprs();

    TreePrinter printer;
    SymResolver resolver;

    parent->add_visitor(resolver);
    parent->add_visitor(printer);
    parent->apply_visitors();
  }

  return 0;
}
