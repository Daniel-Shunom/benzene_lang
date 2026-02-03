#include "../lib/files.hpp"
#include "./cmd/cmd.hpp"
#include "lexer/lexer.hpp"
#include <cstdio>
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

  return 0;
}
