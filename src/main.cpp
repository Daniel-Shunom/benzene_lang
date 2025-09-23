#include "../lib/files.hpp"
#include "./cmd/cmd.hpp"
#include "./lexer/lex.hpp"
#include "./types/symbols_t.hpp"
#include "./types/syntax_t.hpp"
#include "../lib/logger.hpp"
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

  std::vector<Token> tokens;
  while (true) {
    Token t = lexer.advance();
    tokens.push_back(t);

    if (t.tok_type == TOKEN_EOF) {
      break;
    }
  }

  std::printf(
    "\nDone lexing '%s'\n"
    "Resulting tokens:\n",
    args.file_path.data()
  );

  for (const auto &tok: tokens) {
    logToken(tok);
  }

  return 0;
}
