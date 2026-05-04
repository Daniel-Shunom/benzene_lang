#pragma once

#include <string>
#include <vector>
enum class DiagnosticLevel {
  Note,
  Warn,
  Fail,
};

enum class DiagnosticPhase {
  Tokenizer,
  Lexer,
  Parser,
  Resolver,
  TypeChecker,
  CodeGen,
};

struct SourceLocation {
  size_t line;
  size_t column;
};

struct Diagnostic {
  DiagnosticLevel level;
  DiagnosticPhase phase;
  SourceLocation  location;
  std::string     message;
  std::vector<Diagnostic> related;
};
