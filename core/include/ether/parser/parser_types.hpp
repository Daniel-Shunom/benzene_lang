#pragma once
#include <ether/parser/parser_err.hpp>
#include <optional>
#include <cstdio>
#include <functional>
#include <string>
#include <ether/nodes/node_expr.hpp>
#include <ether/tokens/token_types.hpp>
#include <ether/diagnostics/diagnostic_eng.hpp>

enum class ScopeStackType {
  Function,
  Lambda,
  CaseExpr,
};

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

  void reset_pos(size_t old) {
    if (old < tokens.size()) {
      pos = old;
    }
  }

  [[nodiscard]] auto is_at_end() const -> bool {
    return pos >= tokens.size();
  }

  void set_state(std::vector<Token> tokens) {
    this->tokens = tokens;
  }

  auto peek() -> std::optional<Token> {
    if (is_at_end()) {
      return std::nullopt;
    }
    return tokens[pos];
  }

  static auto is_comment(TokenType type) -> bool {
    return type == TokenType::MLComment
      || type == TokenType::SLComment
      || type == TokenType::UTComment;
  }

  auto advance() -> Token {
    if (!this->is_at_end()) {
      return this->tokens[pos++];
    }
    return tokens.back();
  }

  void skip_until(TokenType type) {
    while(!is_at_end()) {
      if(auto tok = peek(); tok->token_type == type) {
        return;
      }
      this->advance();
    }
  }

  std::vector<ScopeStackType> stack;
  DiagnosticEngine& diag_eng;
};

template<typename T>
using PResult = std::optional<T>;

template<typename T>
using Parser = std::function<PResult<T>(ParserState&)>;


template<typename A, typename F>
auto map(Parser<A> parser, F func)
  -> Parser<std::invoke_result_t<F, A>>
{
  using B = std::invoke_result_t<F, A>;

  return [=](ParserState& state) -> PResult<B> {
    size_t start = state.pos;

    auto ret = parser(state);
    if (!ret) {
      state.reset_pos(start);
      return std::nullopt;
    }

    return func(std::move(*ret));
  };
}


template<typename T>
auto seq(std::vector<Parser<T>> parsers) -> Parser<std::vector<T>> {
  return [=](ParserState& state) -> PResult<std::vector<T>> {
    size_t start = state.pos;
    std::vector<T> out;

    for (auto& parser : parsers) {
      auto ret = parser(state);
      if (!ret) {
        state.pos = start;
        return std::nullopt;
      }
      out.push_back(std::move(*ret));
    }

    return out;
  };
}

template<typename T>
auto choice(std::vector<Parser<T>> parsers) -> Parser<T> {
  return [=](ParserState& state) -> PResult<T> {
    size_t start = state.pos;

    for (auto& parser: parsers) {
      auto ret = parser(state);
      if(ret) {
        return ret;
      }
      state.reset_pos(start);
    }

    return std::nullopt;
  };
}

template<typename A, typename B>
auto bind(Parser<A> parser, std::function<Parser<B>(A)> func) -> Parser<B> {
  return [=](ParserState& state) -> PResult<B> {
    size_t start = state.pos;

    auto ret = parser(state);
    if (!ret) {
      state.reset_pos(start);
      return std::nullopt;
    }

    auto next = func(std::move(*ret));
    auto out = next(state);
    if (!out) {
      state.reset_pos(start);
    }

    return out;
  };
}

template<typename T>
auto run(Parser<T> parser, ParserState& state) -> PResult<T> {
  return parser(state);
}

// Defers resolving `factory` until first invocation. Use at recursion points
// where memoized parsers reference each other to avoid static-init cycles.
template<typename T>
auto lazy(Parser<T> (*factory)()) -> Parser<T> {
  return [factory](ParserState& state) -> PResult<T> {
    static const Parser<T> cached = factory();
    return cached(state);
  };
}

struct ParseCheckpoint {
  ParserState& state;
  size_t start;
  bool committed = false;

  ParseCheckpoint(ParserState& stt)
    : state(stt), start(stt.pos) {}

  void commit() {
    committed = true;
  }

  ~ParseCheckpoint() {
    if (!committed) {
      state.reset_pos(start);
    }
  }

};

struct ScopeStackGuard {
  ParserState& state;
  size_t start;

  ScopeStackType type;

  ScopeStackGuard(ParserState& state, ScopeStackType type)
  : state(state), start(state.pos), type(type) {
    state.stack.push_back(type);
  }

  ~ScopeStackGuard() {
    if (!state.stack.empty()) {
      auto recent = state.stack.back();
      if (type == recent) {
        state.stack.pop_back();
        return;
      };
      state.reset_pos(start);
      return;
    }
  }
};


template<typename T>
auto optional(Parser<T> parser, ParserState& state) -> std::optional<T> {
  ParseCheckpoint checkpoint(state);
  if (auto ret = parser(state)) {
    checkpoint.commit();
    return ret;
  }
  return std::nullopt;
}

auto inline expect(
  ParserState& state,
  TokenType type,
  ParseErrorType err_type,
  const std::string& message
) -> std::optional<Token> {
  auto tok = state.peek();
  if (!tok) {
    return std::nullopt;
  }

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
auto inline expect_wp(
  ParserState& state,
  Parser<T> parser,
  ParseErrorType err_type,
  const std::string& message
) -> PResult<T> {
  auto res = parser(state);
  if (!res) {
    if (!state.peek()) {
      return std::nullopt;
    }
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
