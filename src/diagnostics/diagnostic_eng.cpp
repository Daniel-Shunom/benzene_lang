#include "diagnostic_eng.hpp"
#include "diagnostic.hpp"
#include <iostream>
#include <algorithm>

void DiagnosticEngine::report(Diagnostic diag) {
  this->diagnostics.push_back(diag);
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

void DiagnosticEngine::print_all() {

  // Sort by line/column for nice output
  std::sort(diagnostics.begin(), diagnostics.end(),
    [](const Diagnostic& a, const Diagnostic& b) {
      if (a.location.line != b.location.line)
        return a.location.line < b.location.line;
      return a.location.column < b.location.column;
    });

  for (const auto& d : diagnostics) {

    const char* color = level_color(d.level);

    std::cout
      << BOLD
      << color
      << level_to_string(d.level)
      << RESET
      << DIM
      << " ["
      << MAGENTA
      << phase_to_string(d.phase)
      << RESET
      << DIM
      << "] "
      << "(Ln "
      << d.location.line
      << ", Col "
      << d.location.column
      << ")"
      << RESET
      << "\n  "
      << color
      << d.message
      << RESET
      << "\n";

    // Print related diagnostics (notes)
    for (const auto& note : d.related) {
      std::cout
        << "    "
        << CYAN
        << "note"
        << RESET
        << DIM
        << " (Ln "
        << note.location.line
        << ", Col "
        << note.location.column
        << ")"
        << RESET
        << "\n      "
        << note.message
        << "\n";
    }

    std::cout << "\n";
  }
}
