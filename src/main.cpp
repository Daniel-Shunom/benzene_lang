#include "../lib/files.hpp"
#include "cmd/cmd.hpp"
#include "lexer/lexer.hpp"
#include "parsers/combinator/parsers.hpp"
#include <cstdio>
#include <print>
#include <string_view>

int main(int argc, char* argv[]) {
  Args args = GetArgs(argc, argv);
  std::string src_code = FileToString(args.file_path);
  std::string_view contents{src_code};
  Lexer lexer(contents);
 
  std::printf(
    "\nLexing current file\n"
    "File path: %s\n",
    args.file_path.data()
  );
  
  lexer.scan_tokens();
  lexer.print_tokens();

  auto tokens = lexer.get_tokens();

  std::printf(
    "\nDone lexing '%s'\n"
    "Resulting tokens:\n",
    args.file_path.data()
  );

  auto parser = parse_expr();
  auto state = TokenState();
  state.set_state_list(tokens);
  
  std::println("Beginning Parsing");
  auto res = run_parser(parser, state);
  std::println("Ended parsing");
  return 0;
}
