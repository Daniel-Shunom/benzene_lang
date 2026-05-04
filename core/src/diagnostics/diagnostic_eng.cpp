#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/diagnostics/diagnostic.hpp>
#include <iostream>
#include <algorithm>
#include <format>
#include <sstream>

void DiagnosticEngine::report(Diagnostic diag) {
  this->diagnostics.push_back(diag);
}

void DiagnosticEngine::set_source(std::string path, std::string text) {
  this->source_path = std::move(path);
  this->source_lines.clear();
  std::stringstream ss(text);
  std::string line;
  while (std::getline(ss, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    this->source_lines.push_back(std::move(line));
  }
}

bool DiagnosticEngine::has_errors() const {
  for (const auto& diag: this->diagnostics) {
    if (diag.level == DiagnosticLevel::Fail) return true;
  }

  return false;
}

static const char* level_to_string(DiagnosticLevel level) {
  switch (level) {
    case DiagnosticLevel::Fail: return "error";
    case DiagnosticLevel::Warn: return "warning";
    case DiagnosticLevel::Note: return "note";
  }
  return "unknown";
}

static const char* phase_to_string(DiagnosticPhase phase) {
  switch (phase) {
    case DiagnosticPhase::Tokenizer:   return "tokenizer";
    case DiagnosticPhase::Lexer:       return "lexer";
    case DiagnosticPhase::Parser:      return "parser";
    case DiagnosticPhase::Resolver:    return "resolver";
    case DiagnosticPhase::TypeChecker: return "type-checker";
    case DiagnosticPhase::CodeGen:     return "codegen";
  }
  return "unknown";
}

static const char* level_color(DiagnosticLevel level) {
  switch (level) {
    case DiagnosticLevel::Fail: return RED;
    case DiagnosticLevel::Warn: return YELLOW;
    case DiagnosticLevel::Note: return CYAN;
  }
  return RESET;
}

static std::string gutter(size_t width, const std::string& content = "") {
  std::string pad(width - content.size(), ' ');
  return std::format("{}{}{} |{} ", pad, BLUE, content, RESET);
}

void DiagnosticEngine::print_all() {
  // Sort by line/column for readability.
  std::sort(diagnostics.begin(), diagnostics.end(),
    [](const Diagnostic& a, const Diagnostic& b) {
      if (a.location.line != b.location.line)
        return a.location.line < b.location.line;
      return a.location.column < b.location.column;
    });

  for (const auto& d : diagnostics) {
    const char* color = level_color(d.level);
    const bool has_location = d.location.line > 0;
    const bool can_show_source =
      has_location
      && !source_lines.empty()
      && d.location.line <= source_lines.size();

    // Header: "error: <message>"
    std::cout
      << BOLD << color << level_to_string(d.level) << RESET
      << BOLD << ": " << d.message << RESET << "\n";

    // Location arrow: "  --> path:line:col  [phase]"
    if (has_location) {
      std::cout
        << "  " << BLUE << "-->" << RESET << " "
        << (source_path.empty() ? "<source>" : source_path)
        << ":" << d.location.line << ":" << d.location.column
        << DIM << "  [" << MAGENTA << phase_to_string(d.phase)
        << RESET << DIM << "]" << RESET << "\n";
    } else {
      std::cout
        << "  " << BLUE << "-->" << RESET << " "
        << DIM << "(no source location)  [" << MAGENTA
        << phase_to_string(d.phase) << RESET << DIM << "]" << RESET << "\n";
    }

    // Source excerpt + caret.
    if (can_show_source) {
      std::string line_str = std::to_string(d.location.line);
      size_t gutter_w = line_str.size();

      std::cout << gutter(gutter_w) << "\n";
      std::cout
        << gutter(gutter_w, line_str)
        << source_lines[d.location.line - 1] << "\n";

      // Caret line: pad to column, then '^'.
      std::string caret_pad;
      if (d.location.column > 0) caret_pad.assign(d.location.column - 1, ' ');
      std::cout
        << gutter(gutter_w)
        << caret_pad << color << "^" << RESET << "\n";
    }

    // Related notes (currently used for cross-references).
    for (const auto& note : d.related) {
      std::cout
        << "  " << CYAN << "= note:" << RESET << " " << note.message;
      if (note.location.line > 0) {
        std::cout
          << DIM << " (" << note.location.line
          << ":" << note.location.column << ")" << RESET;
      }
      std::cout << "\n";
    }

    std::cout << "\n";
  }
}
