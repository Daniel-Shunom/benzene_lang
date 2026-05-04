#pragma once
#include <ether/diagnostics/diagnostic.hpp>
#include <string>
#include <vector>

namespace {
  constexpr auto RESET  = "\033[0m";
  constexpr auto BOLD   = "\033[1m";
  constexpr auto DIM    = "\033[2m";

  constexpr auto RED    = "\033[31m";
  constexpr auto YELLOW = "\033[33m";
  constexpr auto CYAN   = "\033[36m";
  constexpr auto MAGENTA= "\033[35m";
  constexpr auto BLUE   = "\033[34m";
}

class DiagnosticEngine {
public:
  void report(Diagnostic);

  // Provides the source for line/caret rendering. Call once per module after
  // reading the file. The engine takes ownership of the text.
  void set_source(std::string path, std::string text);

  void print_all();

  bool has_errors() const;

private:
  std::vector<Diagnostic> diagnostics{};
  std::string source_path{};
  std::vector<std::string> source_lines{};
};
