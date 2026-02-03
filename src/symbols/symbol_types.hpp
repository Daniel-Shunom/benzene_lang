#pragma once
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "../lexer/token_types.hpp"

enum class SymbolKind {
  Function,
  Binding,
  Constant,
  Module,
  Type,
};

struct TypeInfo {
  std::string type_name{};
};

enum class SymbolErrorType {
  NotAllowedInScope,
  Duplicate,
  OtherError,
};

struct SymbolError {
  SymbolErrorType error_type;
  std::vector<Token> offending_tokens;
};

struct Symbol {
  std::string name;
  SymbolKind symbol_kind;
  std::optional<TypeInfo> type_info = std::nullopt;
  Token symbol_token;
  std::vector<SymbolError> symbol_errors{};
};

using SymTable = std::unordered_map<std::string, std::unique_ptr<Symbol>>;
