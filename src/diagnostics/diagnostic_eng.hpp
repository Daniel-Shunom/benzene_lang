#pragma once
#include "diagnostic.hpp"
#include <vector>

namespace {
  constexpr auto RESET  = "\033[0m";
  constexpr auto BOLD   = "\033[1m";
  constexpr auto DIM    = "\033[2m";

  constexpr auto RED    = "\033[31m";
  constexpr auto YELLOW = "\033[33m";
  constexpr auto CYAN   = "\033[36m";
  constexpr auto MAGENTA= "\033[35m";
}

class DiagnosticEngine {
public:
  void report(Diagnostic);

  void print_all();

  bool has_errors() const;

private:
  std::vector<Diagnostic> diagnostics{};
};
