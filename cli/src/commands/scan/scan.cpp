#include "scan.hpp"
#include "indexer.hpp"
#include "json.hpp"
#include "files.hpp"

#include <ether/ast_passes/scope_resolution/scope_res.hpp>
#include <ether/ast_passes/symbol_resolver/symbol_resolver.hpp>
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/lexer/lexer.hpp>
#include <ether/module/module.hpp>

#include <cstdio>
#include <iostream>
#include <map>
#include <print>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

namespace {

auto level_to_string(DiagnosticLevel level) -> std::string {
  switch (level) {
    case DiagnosticLevel::Note: return "note";
    case DiagnosticLevel::Warn: return "warning";
    case DiagnosticLevel::Fail: return "error";
  }
  return "error";
}

auto phase_to_string(DiagnosticPhase phase) -> std::string {
  switch (phase) {
    case DiagnosticPhase::Tokenizer:       return "tokenizer";
    case DiagnosticPhase::Lexer:           return "lexer";
    case DiagnosticPhase::Parser:          return "parser";
    case DiagnosticPhase::Resolver:        return "resolver";
    case DiagnosticPhase::ScopeResolution: return "scope";
    case DiagnosticPhase::TypeChecker:     return "types";
    case DiagnosticPhase::CodeGen:         return "codegen";
  }
  return "unknown";
}

// Reads a length-prefixed payload from stdin: a decimal byte count on its own
// line, then exactly that many bytes of source.
//
// The count is what makes this safe to drive from a long-lived parent process:
// the child knows when the payload ends without waiting for EOF, so the
// language server never has to close and reopen the pipe between edits.
auto read_stdin_payload() -> std::string {
#ifdef _WIN32
  // Without this, the CRT rewrites CRLF on the way in and the byte count
  // stops matching what the server actually wrote.
  _setmode(_fileno(stdin), _O_BINARY);
#endif
  std::string header;
  if (!std::getline(std::cin, header)) {
    return {};
  }
  if (!header.empty() && header.back() == '\r') {
    header.pop_back();
  }

  size_t expected = 0;
  try {
    expected = static_cast<size_t>(std::stoull(header));
  } catch (...) {
    return {};
  }

  std::string source(expected, '\0');
  std::cin.read(source.data(), static_cast<std::streamsize>(expected));
  source.resize(static_cast<size_t>(std::cin.gcount()));
  return source;
}

// The lexer stores a string literal's *contents*, without the surrounding
// quotes, but the editor needs to highlight the quotes too. Every other token
// spans exactly its value.
auto token_span_length(const Token& token) -> size_t {
  switch (token.token_type) {
    case TokenType::StringLiteral:   return token.token_value.size() + 2;
    case TokenType::UTStringLiteral: return token.token_value.size() + 1;
    default:                         return token.token_value.size();
  }
}

// Token lengths keyed by start position. Diagnostics carry only a point
// (line, column); the editor wants a range to underline, and the token that
// starts at that point is the best available answer.
using SpanTable = std::map<std::pair<size_t, size_t>, size_t>;

auto span_length(const SpanTable& spans, size_t line, size_t column) -> size_t {
  auto found = spans.find({line, column});
  if (found == spans.end() || found->second == 0) {
    return 1;
  }
  return found->second;
}

void emit_diagnostic(std::ostream& out, const Diagnostic& diag,
                     const SpanTable& spans) {
  out << "{"
      << "\"line\":"     << diag.location.line
      << ",\"column\":"  << diag.location.column
      << ",\"length\":"  << span_length(spans, diag.location.line,
                                        diag.location.column)
      << ",\"severity\":" << json::quote(level_to_string(diag.level))
      << ",\"phase\":"    << json::quote(phase_to_string(diag.phase))
      << ",\"message\":"  << json::quote(diag.message)
      << ",\"related\":[";
  for (size_t i = 0; i < diag.related.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    emit_diagnostic(out, diag.related[i], spans);
  }
  out << "]}";
}

}  // namespace

int HandleScan(const ArgScan& a) {
  std::string source = a.use_stdin ? read_stdin_payload() : FileToString(a.path);

  // Lexed twice on purpose: this pass feeds semantic highlighting and the
  // diagnostic span table, and it uses a throwaway engine so the lexer's own
  // diagnostics are not double-counted when Module lexes again below.
  DiagnosticEngine token_diag;
  Lexer lexer(source, token_diag);
  lexer.scan_tokens();
  std::vector<Token> tokens = lexer.get_tokens();

  SpanTable spans;
  for (const auto& token : tokens) {
    spans.emplace(std::pair{token.line_number, token.column_number},
                  token_span_length(token));
  }

  Module mod(a.path, std::move(source));
  mod.generate_ast();

  ScopeRes scope_resolver(mod.get_symbol_storage(), mod.get_diag_engine());
  SymbolResolver resolver(mod.get_symbol_storage(), mod.get_diag_engine());
  TypeChecker type_checker(false, mod.get_diag_engine());

  mod.attach_visitor(resolver);
  mod.attach_visitor(scope_resolver);
  mod.attach_visitor(type_checker);
  mod.apply_visitors();
  type_checker.unify_constraints();

  // Indexed after unification so every rendered type is solved rather than a
  // bare type variable.
  LspIndexer indexer(&type_checker.substitutions());
  mod.apply_visitor(indexer);

  std::ostream& out = std::cout;
  out << "{\"path\":" << json::quote(a.path);

  out << ",\"tokens\":[";
  bool first = true;
  for (const auto& token : tokens) {
    if (token.token_type == TokenType::EoF) {
      continue;
    }
    if (!first) {
      out << ",";
    }
    first = false;
    out << "{\"line\":"   << token.line_number
        << ",\"column\":" << token.column_number
        << ",\"length\":" << token_span_length(token)
        << ",\"type\":"   << json::quote(token_type_to_str(token.token_type))
        << ",\"value\":"  << json::quote(token.token_value)
        << "}";
  }
  out << "]";

  out << ",\"diagnostics\":[";
  const auto& diagnostics = mod.get_diag_engine().all();
  for (size_t i = 0; i < diagnostics.size(); ++i) {
    if (i > 0) {
      out << ",";
    }
    emit_diagnostic(out, diagnostics[i], spans);
  }
  out << "]";

  out << ",\"index\":[";
  const auto& entries = indexer.entries();
  for (size_t i = 0; i < entries.size(); ++i) {
    const auto& entry = entries[i];
    if (i > 0) {
      out << ",";
    }
    out << "{\"name\":"       << json::quote(entry.name)
        << ",\"kind\":"       << json::quote(entry.kind)
        << ",\"line\":"       << entry.line
        << ",\"column\":"     << entry.column
        << ",\"length\":"     << entry.length
        << ",\"type\":"       << json::quote(entry.type)
        << ",\"defLine\":"    << entry.def_line
        << ",\"defColumn\":"  << entry.def_column
        << ",\"isDefinition\":" << (entry.is_definition ? "true" : "false")
        << "}";
  }
  out << "]}";
  out << std::endl;

  return 0;
}
