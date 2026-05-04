#pragma once
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/tokens/token_types.hpp>

class LexerDiagnostics {
public:
  LexerDiagnostics(DiagnosticEngine& eng)
  : diag_eng(eng) {};

  void unknown_character(Token&);

private:
  DiagnosticEngine& diag_eng;
};
