#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "../tokens/token_types.hpp"

enum class SymbolKind {
  UnResolved,
  Function,
  FuncParam,
  Binding,
  Constant,
  Module,
  Type,
};

/* Symbol Data structs. Used in storing information about certain symbols*/
struct TypeData {
  // We should store other information here. For instance,
  // user defined types should be referenced in definitions 
  // somewhere. Perhaps we will keep a table that refrences,
  // user generated types.
  std::string type_name;
};

struct FuncParamData {
  size_t index;
  std::string param_name;
  TypeData param_type;
};

struct FunctionData {
  std::vector<FuncParamData> function_params;
  TypeData funtion_return_type;
};

struct BindingData {
  TypeData binding_type;
};

struct ConstantData {
  TypeData constant_type;
};

using SymbolData = std::variant<
  TypeData,
  BindingData,
  ConstantData,
  FunctionData,
  FuncParamData
>;

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
  TypeInfo type_info{};
  Token symbol_token;
  SymbolData symbol_data;
  std::vector<SymbolError> symbol_errors{};
};

using SymTable = std::unordered_map<std::string, std::unique_ptr<Symbol>>;


