#pragma once
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "../lexer/token_types.hpp"

enum class SymbolType {
  ImportExpr,
  ConstExpr,
  LetBinding,
  FuncDecl,
  FuncInvoc,
  CaseExpr,
  Type,
};



struct ImportExprInfo {
  std::string module_name{};
  std::vector<std::string> symbols{};
  bool is_qualified;
};

struct TypeInfo {
  std::string type_name{};
};

struct FuncDeclInfo {
  std::vector<std::string> params{};
  TypeInfo rtn_type_info;
};

struct FuncInvocInfo {
  std::vector<std::string> params{};
};

struct LetBindingInfo {
  TypeInfo type_info;
};

struct ConstExprInfo {
  TypeInfo type_info;
  std::string value{};
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

using SymbolInfo = std::variant<
  TypeInfo,
  FuncDeclInfo,
  FuncInvocInfo,
  LetBindingInfo,
  ConstExprInfo,
  SymbolError
>;

struct Symbol {
  SymbolType sym_type;
  SymbolInfo sym_info;
};

using SymTable = std::unordered_map<std::string, Symbol>;
