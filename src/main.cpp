#include "../lib/files.hpp"
#include "./cmd/cmd.hpp"
#include "tokenizer/tokenizer.hpp"
#include <cstdio>
#include <string_view>

int main(int argc, char* argv[]) {
  Args args = GetArgs(argc, argv);
  std::string src_code = FileToString(args.file_path);
  std::string_view contents{src_code};
  Tokenizer tokenizer(contents);
 
  std::printf(
    "\nLexing current file\n"
    "File path: %s\n",
    args.file_path.data()
  );
  
  tokenizer.scan_tokens();
  tokenizer.print_tokens();

  auto tokens = tokenizer.get_tokens();

  std::printf(
    "\nDone lexing '%s'\n"
    "Resulting tokens:\n",
    args.file_path.data()
  );

  return 0;
}
