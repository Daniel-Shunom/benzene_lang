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
    "  %screate%s %s<name>%s    Scaffold a new project\n"
    "  %sinit%s             Initialize a project in the current directory\n"
    "  %scheck%s %s<file>%s     Parse and resolve a source file\n"
    "      %s-show-ast%s    also print the AST\n"
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
    YELLOW, RESET, DIM, RESET,
    YELLOW, RESET, DIM, RESET,
    YELLOW, RESET
  );
  return 0;
}
