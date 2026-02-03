#pragma once
#include <optional>
#include <span>
#include <expected>
#include <functional>
#include <string>

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
