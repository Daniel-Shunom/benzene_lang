#pragma once
#include "parser_err.hpp"
#include <optional>
#include <iostream>
#include <cstdio>
#include <functional>
#include <string>
#include "../nodes/node_expr.hpp"
#include "../tokens/token_types.hpp"
#include "../diagnostics/diagnostic_eng.hpp"

struct ParserState {
  ParserState(DiagnosticEngine& eng)
  : diag_eng(eng) {};

  std::vector<Token> tokens;

  std::vector<std::string> expr_captures;

  size_t pos{};

  bool logs_on = false;

  void activate_logs() {
    this->logs_on = true;
  }

  void reset_pos(size_t at) {
    if (at < tokens.size()) pos = at;
  }

  bool is_at_end() {
    return pos >= tokens.size();
  }

  void set_state(std::vector<Token> tokens) {
    this->tokens = tokens;
  }

  std::optional<Token> peek() { 
    if (is_at_end()) return std::nullopt;
    return tokens[pos];
  }

  bool is_comment(TokenType type) {
    return type == TokenType::MLComment
      || type == TokenType::SLComment
      || type == TokenType::UTComment;
  }

  Token advance() {
    if (!this->is_at_end()) return this->tokens[pos++];
    return tokens.back();
  }

  void skip_until(TokenType type) {
    while(!is_at_end()) {
      if(auto p = peek(); p->token_type == type) {
        return;
      }
      this->advance();
    }
  }

  DiagnosticEngine& diag_eng;
};

template<typename T>
using PResult = std::optional<T>;

template<typename T>
using Parser = std::function<PResult<T>(ParserState&)>;


template<typename A, typename F>
auto map(Parser<A> p, F f)
  -> Parser<std::invoke_result_t<F, A>>
{
  using B = std::invoke_result_t<F, A>;

  return [=](ParserState& state) -> PResult<B> {
    size_t start = state.pos;

    auto r = p(state);
    if (!r) {
      state.reset_pos(start);
      return std::nullopt;
    }

    return f(std::move(*r));
  };
}


template<typename T>
Parser<std::vector<T>> seq(std::vector<Parser<T>> parsers) {
  return [=](ParserState& state) -> PResult<std::vector<T>> {
    size_t start = state.pos;
    std::vector<T> out;

    for (auto& p : parsers) {
      auto r = p(state);
      if (!r) {
        state.pos = start;
        return std::nullopt;
      }
      out.push_back(std::move(*r));
    }

    return out;
  };
}

template<typename T>
Parser<T> choice(std::vector<Parser<T>> parsers) {
  return [=](ParserState& state) -> PResult<T> {
    size_t start = state.pos;

    for (auto& p: parsers) {
      auto r = p(state);
      if(r) return r;
      state.reset_pos(start);
      continue;
    }

    return std::nullopt;
  };
}

template<typename A, typename B>
Parser<B> bind(Parser<A> p, std::function<Parser<B>(A)> f) {
  return [=](ParserState& state) -> PResult<B> {
    size_t start = state.pos;

    auto r = p(state);
    if (!r) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto next = f(std::move(*r));
    auto out = next(state);
    if (!out) {
      state.reset_pos(start);
    }

    return out;
  };
}

template<typename T>
PResult<T> run(Parser<T> t, ParserState& state) {
  return t(state);
}

struct ParseCheckpoint {
  ParserState& state;
  size_t start;
  bool committed = false;

  ParseCheckpoint(ParserState& s)
    : state(s), start(s.pos) {}

  void commit() {
    committed = true;
  }

  ~ParseCheckpoint() {
    if (!committed) {
      state.reset_pos(start);
    }
  }
};


template<typename T>
std::optional<T> optional(Parser<T> p, ParserState& state) {
  ParseCheckpoint ck(state);
  if (auto r = p(state)) {
    ck.commit();
    return r;
  }
  return std::nullopt;
}

std::optional<Token> inline expect(
  ParserState& state,
  TokenType type,
  ParseErrorType err_type,
  const std::string& message
) {
  auto tok = state.peek();
  if (!tok) return std::nullopt;

  if (tok->token_type == type) {
    state.advance();
      return tok;
  }

  auto diag = Diagnostic();
  diag.location.line = tok->line_number;
  diag.location.column = tok->column_number;
  diag.phase = DiagnosticPhase::Parser;
  diag.level = DiagnosticLevel::Fail;
  diag.message = message;

  state.diag_eng.report(diag);
  return std::nullopt;
}

template<typename T>
PResult<T> inline expect_wp(
  ParserState& state,
  Parser<T> parser,
  ParseErrorType err_type,
  const std::string& message
) {
  auto res = parser(state);
  if (!res) {
    if (!state.peek()) return std::nullopt;
    auto tok = state.peek();

    auto diag = Diagnostic();
    diag.location.line = tok->line_number;
    diag.location.column = tok->column_number;
    diag.phase = DiagnosticPhase::Parser;
    diag.level = DiagnosticLevel::Fail;
    diag.message = message;

    state.diag_eng.report(diag);
    return std::nullopt;
  }

  return res;
}
