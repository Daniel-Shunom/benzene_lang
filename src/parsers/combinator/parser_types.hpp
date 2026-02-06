#pragma once
#include <cstdio>
#include <functional>
#include <string>
#include "../../tokens/token_types.hpp"
#include "../../symbols/symtable.hpp"

struct ParserState {
  std::vector<Token> tokens;
  SymbolTable sym_table;
  size_t pos{};


  std::vector<std::string> logs;

  std::vector<std::string> expr_captures;

  void reset_pos(size_t at) {
    if (at < tokens.size()) pos = at;
  }

  bool is_at_end() {
    return pos >= tokens.size();
  }

  bool logs_enabled = false;

  void activivate_logs() {
    logs_enabled = true;
  }

  void log(std::string log) {
    if(not logs_enabled) return;
    auto l = std::string("[PARSER LOG DEBUG]\t"+log);
    logs.push_back(l);
  }

  void log_err(std::string log) {
    this->log("[Err]\t"+log);
  }

  void log_success(std::string log) {
    this->log("[Ok ]\t"+log);
  }

  void display_logs() {
    for (auto& log: logs) {
      std::printf("%s\n", log.data());
    }
  }

  void log_captured_expr(std::string expr) {
    if (not logs_enabled) return;
    expr_captures.push_back("[Expr Capture]\t"+expr);
  }

  void display_captured_exprs() {
    for (const auto& expr: this->expr_captures) {
      std::printf("%s\n", expr.data());
    }
  }

  void set_state(std::vector<Token> tokens) {
    this->tokens = tokens;
  }

  std::optional<Token> peek() { 
    if (is_at_end()) return Token{.token_type=TokenType::EoF};
    return tokens[pos];
  }

  bool is_comment(TokenType type) {
    return type == TokenType::MLComment
      || type == TokenType::SLComment
      || type == TokenType::UTComment;
  }

  Token advance() {
    while(!is_at_end()) {
      if (auto p = this->peek(); p && is_comment(p->token_type)) {
        pos++;
        continue;
      }

      return this->tokens[this->pos++];
    }
    return Token{.token_type=TokenType::EoF};
  }
};

template<typename T>
using PResult = std::optional<T>;

template<typename T>
using Parser = std::function<PResult<T>(ParserState&)>;

template<typename A, typename B>
Parser<B> map(Parser<A> p, std::function<B(A)> f) {
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
