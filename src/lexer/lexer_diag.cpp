#include "lexer_diag.hpp"
#include <format>

void LexerDiagnostics::unknown_character(Token& tok) {
  Diagnostic diag;
  diag.level = DiagnosticLevel::Warn;
  diag.phase = DiagnosticPhase::Lexer;
  diag.location = {
    .line = tok.line_number,
    .column = tok.column_number,
  };
  diag.message = std::format(
    "Unrecognized token `{}` detected",
    tok.token_value
  );

  this->diag_eng.report(diag);
}
