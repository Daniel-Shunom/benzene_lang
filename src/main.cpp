#include "../lib/files.hpp"
#include "./cmd/cmd.hpp"
#include "./lexer/lex.hpp"
#include "./types/symbols_t.hpp"
#include "./types/syntax_t.hpp"
#include "../lib/logger.hpp"
#include "parser/parser.hpp"
#include <cstdio>
#include <vector>

int main(int argc, char* argv[]) {
  Args args = GetArgs(argc, argv);
  Lex lexer(FileToString(args.file_path));
 
  std::printf(
    "\nLexing current file\n"
    "File path: %s\n",
    args.file_path.data()
  );

  lexer.extractAllTokens();
  auto&& tokens = lexer.getTokenList();

  std::printf(
    "\nDone lexing '%s'\n"
    "Resulting tokens:\n",
    args.file_path.data()
  );

  for (const auto &tok: tokens) {
    logToken(tok);
  }

  Parser parser(tokens);
  parser.parseTokens();

  std::printf(
    "\nDone parsing tokens'%s'\n"
    "Resulting tokens:\n",
    args.file_path.data()
  );

  return 0;
}
