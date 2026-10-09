#include "help.hpp"
#include <cstdio>

namespace {
  constexpr auto RESET   = "\033[0m";
  constexpr auto BOLD    = "\033[1m";
  constexpr auto DIM     = "\033[2m";
  constexpr auto GREEN   = "\033[32m";
  constexpr auto YELLOW  = "\033[33m";
  constexpr auto MAGENTA = "\033[35m";
  constexpr auto CYAN    = "\033[36m";
}

int HandleHelp(const ArgHelp&) {
  std::printf(
    "%s%sether%s: the benzene compiler\n"
    "\n"
    "%s%sUSAGE:%s\n"
    "  %sether%s %s<command>%s %s[args]%s\n"
    "\n"
    "%s%sCOMMANDS:%s\n"
    "  %snew%s %s<project_name>%s    Scaffold a project (create is an alias)\n"
    "  %sinit%s             Initialize in the current directory (not yet implemented)\n"
    "  %scheck%s %s<file>%s     Parse, resolve and type-check a source file\n"
    "  scan <file>       Return JSON diagnostics and inferred types\n"
    "      %s-show-ast%s    also print the AST\n"
    "      %s-show-types%s  print inferred types during checking\n"
    "      -show-constraints    print generated type constraints\n"
    "      -show-unification    print solved type substitutions\n"
    "  %sbuild%s            Compile the project %s(not yet implemented)%s\n"
    "  %srun%s              Build and execute %s(not yet implemented)%s\n"
    "  %shelp%s             Show this help\n",
    BOLD, GREEN, RESET,
    BOLD, CYAN, RESET,
    GREEN, RESET, YELLOW, RESET, MAGENTA, RESET,
    BOLD, CYAN, RESET,
    YELLOW, RESET, MAGENTA, RESET,
    YELLOW, RESET,
    YELLOW, RESET, MAGENTA, RESET,
    CYAN, RESET,
    CYAN, RESET,
    YELLOW, RESET, DIM, RESET,
    YELLOW, RESET, DIM, RESET,
    YELLOW, RESET
  );
  return 0;
}
