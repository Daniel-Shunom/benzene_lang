#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <ether/tokens/token_types.hpp>

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

struct SymbolAttr {
  std::string name;
  SymbolKind symbol_kind;
  TypeInfo type_info{};
  Token symbol_token;
  SymbolData symbol_data;
  std::vector<SymbolError> symbol_errors{};
};

// Scope visibility maps name -> non-owning pointer into the module's
// SymbolStorage arena. Popping a scope drops the visibility entry but does
// NOT destroy the SymbolAttr — that lives for the whole module so node
// decorations (e.g. NDIdentifier::identifier_symbol) remain valid through
// later passes (HM, codegen).
using SymTable = std::unordered_map<std::string, SymbolAttr*>;

// Owning storage for SymbolAttr objects. One arena per Module; symbols are
// allocated here and pointers handed out remain valid for the module's
// lifetime.
class SymbolStorage {
public:
  SymbolAttr* allocate(SymbolAttr&& attr) {
    auto owned = std::make_unique<SymbolAttr>(std::move(attr));
    SymbolAttr* raw = owned.get();
    storage.push_back(std::move(owned));
    return raw;
  }

  size_t size() const { return storage.size(); }

private:
  std::vector<std::unique_ptr<SymbolAttr>> storage;
};


