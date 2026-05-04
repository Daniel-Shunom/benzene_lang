#pragma once

#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/lexer/lexer.hpp>
#include <ether/module/module.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/parser/parser_types.hpp>
#include <ether/parser/parsers.hpp>
#include <ether/tokens/token_types.hpp>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ether::test {

inline std::vector<Token> lex_all(const std::string& source, DiagnosticEngine& diag) {
  Lexer lex(source, diag);
  lex.scan_tokens();
  return lex.get_tokens();
}

inline std::vector<Token> lex_all(const std::string& source) {
  DiagnosticEngine diag;
  return lex_all(source, diag);
}

// Strips trailing EoF — most lexer assertions are easier without it.
inline std::vector<Token> lex_no_eof(const std::string& source) {
  auto toks = lex_all(source);
  if (!toks.empty() && toks.back().token_type == TokenType::EoF) {
    toks.pop_back();
  }
  return toks;
}

inline Token make_tok(TokenType t, std::string value = {}, size_t line = 1, size_t col = 1) {
  return Token{ .token_type = t, .token_value = std::move(value), .line_number = line, .column_number = col };
}

inline ParserState make_state(std::vector<Token> toks, DiagnosticEngine& diag) {
  // Always append a trailing EoF — parser_types peek() relies on this for
  // bounded behavior in many parsers.
  if (toks.empty() || toks.back().token_type != TokenType::EoF) {
    toks.push_back(make_tok(TokenType::EoF, "", 0, 0));
  }
  ParserState state(diag);
  state.set_state(std::move(toks));
  return state;
}

// RAII redirect of std::cout — diagnostic engine prints to cout, and the
// printer visitor does too. Tests that exercise either path can use this to
// keep ctest output clean.
class CoutSink {
public:
  CoutSink() : prev(std::cout.rdbuf(buf.rdbuf())) {}
  ~CoutSink() { std::cout.rdbuf(prev); }
  std::string str() const { return buf.str(); }
private:
  std::stringstream buf;
  std::streambuf* prev;
};

// Runs the full module pipeline against a source string and applies any
// supplied visitors in the order given. Returns the populated Module so
// callers can inspect diagnostics, AST, and exports.
struct PipelineResult {
  std::unique_ptr<Module> module;
  CoutSink sink;
};

inline std::unique_ptr<Module> run_pipeline(const std::string& source,
                                            std::vector<Visitor*> visitors,
                                            const std::string& path = "<test>") {
  auto mod = std::make_unique<Module>(path, source);
  mod->generate_ast();
  for (auto* v : visitors) mod->attach_visitor(*v);
  mod->apply_visitors();
  return mod;
}

inline size_t count_token_type(const std::vector<Token>& toks, TokenType type) {
  return static_cast<size_t>(std::count_if(toks.begin(), toks.end(),
    [type](const Token& t) { return t.token_type == type; }));
}

}  // namespace ether::test
