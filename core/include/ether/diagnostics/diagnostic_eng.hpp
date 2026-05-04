#pragma once
#include <ether/diagnostics/diagnostic.hpp>
#include <iosfwd>
#include <iostream>
#include <string>
#include <vector>

class DiagnosticEngine {
public:
  void report(Diagnostic);

  // Provides the source for line/caret rendering. Call once per module after
  // reading the file. The engine takes ownership of the text.
  void set_source(std::string path, std::string text);

  void print_all(std::ostream& out = std::cout);

  bool has_errors() const;

  const std::vector<Diagnostic>& all() const { return diagnostics; }

private:
  std::vector<Diagnostic> diagnostics{};
  std::string source_path{};
  std::vector<std::string> source_lines{};
};
