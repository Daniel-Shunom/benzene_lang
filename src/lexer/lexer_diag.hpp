#pragma once
#include "../diagnostics/diagnostic_eng.hpp"
#include "../tokens/token_types.hpp"

class LexerDiagnostics {
public:
  LexerDiagnostics(DiagnosticEngine& eng)
  : diag_eng(eng) {};

  void unknown_character(Token&);

private:
  DiagnosticEngine& diag_eng;
};
