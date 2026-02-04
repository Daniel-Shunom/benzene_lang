#pragma once
#include <optional>
#include <span>
#include <expected>
#define _GLIBCXX_USE_CONSTEXPR_FUNCTIONAL 0
#include <functional>
#include <string>
#include <vector>

template<typename St>
struct ParserState {
  std::span<St> state_list;
  size_t current_position;

  bool is_end() const {
    return this->current_position >= this->state_list.size();
  }

  void advance() {
    this->current_position++;
  }

  void set_state_list(std::vector<St> s) {
    this->state_list = s;
  }

  std::optional<St> current() {
    if (this->is_end()) return std::nullopt;
    return this->state_list[this->current_position];
  }
};

template<typename Ok, typename St>
struct ParseSuccess {
  Ok value;
  ParserState<St>& state;
};

template<typename Err>
struct ParseError {
  Err error_value;
  std::string message;
};

template<typename Ok, typename St>
using ParseResult = std::expected<ParseSuccess<Ok, St>, ParseError<St>>;

template<typename St, typename Rt>
struct Parser {
  std::function<ParseResult<Rt, St> (ParserState<St>&)> apply;
};
