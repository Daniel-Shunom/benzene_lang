#include "help.hpp"
#include <cstdio>

int HandleHelp(const ArgHelp&) {
  std::printf(
    "ether: the benzene compiler\n"
    "\n"
    "USAGE:\n"
    "  ether <command> [args]\n"
    "\n"
    "COMMANDS:\n"
    "  create <name>    Scaffold a new project\n"
    "  init             Initialize a project in the current directory\n"
    "  check <file>     Parse and resolve a source file\n"
    "      -show-ast    also print the AST\n"
    "  build            Compile the project (not yet implemented)\n"
    "  run              Build and execute (not yet implemented)\n"
    "  help             Show this help\n"
  );
  return 0;
}
