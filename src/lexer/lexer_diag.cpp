#include "lexer_diag.hpp"

void LexerDiagnostics::unknown_character(Token& tok) {
  Diagnostic diag;
  diag.level = DiagnosticLevel::Warn;
  diag.phase = DiagnosticPhase::Lexer;
  diag.location = {
    .line = tok.line_number,
    .column = tok.column_number,
  };

  this->diag_eng.report(diag);
}
