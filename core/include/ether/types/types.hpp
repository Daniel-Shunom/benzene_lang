#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

struct Type;
struct SymbolAttr;

using TypeVarId = size_t;

using TypePtr = std::shared_ptr<Type>;

[[nodiscard]]
auto type_ptr_equal(const TypePtr& lhs, const TypePtr& rhs) noexcept -> bool;

struct Scheme {
  std::vector<TypeVarId> quantified;
  TypePtr type;
};

using SymbolTypeScope = std::unordered_map<SymbolAttr*, Scheme>;

class TypeEnvironment {
public:
  TypeEnvironment() {
    push_scope();
  }

  void push_scope() {
    scopes.emplace_back();
  }

  void pop_scope() {
    if (scopes.size() > 1) {
      scopes.pop_back();
    }
  }

  void bind(SymbolAttr* symbol, Scheme scheme) {
    if (symbol) {
      // Keep a symbol-keyed record after a lexical scope is popped.  The
      // resolver has already attached the exact SymbolAttr to each use, so
      // later type-checking passes can recover the declaration's type without
      // having to recreate the population pass's scope walk.
      scopes.back()[symbol] = scheme;
      bindings[symbol] = std::move(scheme);
    }
  }

  [[nodiscard]]
  auto lookup(SymbolAttr* symbol) noexcept -> Scheme* {
    if (!symbol) {
      return nullptr;
    }

    for (auto scope = scopes.rbegin(); scope != scopes.rend(); ++scope) {
      if (auto found = scope->find(symbol); found != scope->end()) {
        return &found->second;
      }
    }

    if (auto found = bindings.find(symbol); found != bindings.end()) {
      return &found->second;
    }

    return nullptr;
  }

private:
  std::vector<SymbolTypeScope> scopes;
  std::unordered_map<SymbolAttr*, Scheme> bindings;
};

using Env = std::unordered_map<std::string, Scheme>;

using Subst = std::unordered_map<TypeVarId, TypePtr>;

class TypeTable{
public:
  void new_type_scope();
  void pop_type_scope();

  [[nodiscard]]
  auto lookup(const std::string&) noexcept -> Scheme*;

  [[nodiscard]]
  auto declare(const std::string&, Scheme) noexcept -> Scheme*;

private:
  std::vector<Env> type_scopes;
};

struct TypeScopeGuard {
   TypeScopeGuard(TypeTable& tbl) noexcept : table(tbl) {
    table.new_type_scope();
  }

  ~TypeScopeGuard() noexcept {
    table.pop_type_scope();
  }

private:
  TypeTable& table;
};

enum class BaseType {
  Int,
  Float,
  String,
  Bool,
  Nil // Apparently this represents a unit type.
};

struct TypeField {
  TypeField() = delete;
  TypeField(std::string field, TypePtr type) noexcept
  : field(std::move(field)), type(std::move(type)) {}

  [[nodiscard]] auto name() const noexcept -> const std::string& { return field; }
  [[nodiscard]] auto get_type() const noexcept -> const TypePtr& { return type; }

  auto operator == (const TypeField& type_field) const noexcept -> bool {
    return type_field.field == field
      && type_ptr_equal(type_field.type, type);
  }

private:
  std::string field;
  TypePtr type;
};

struct PmtType{
  PmtType() = delete;
  PmtType(std::string name, std::vector<TypeField> fields) noexcept
  : name(std::move(name)), type_fields(std::move(fields)) {}

  [[nodiscard]] auto get_name() const noexcept -> const std::string& { return name; }
  [[nodiscard]] auto get_fields() const noexcept -> const std::vector<TypeField>& { return type_fields; }

  auto operator == (const PmtType& type) const noexcept -> bool {
    return type.name == name
      && type.type_fields == type_fields;
  }

private:
  std::string name;
  std::vector<TypeField> type_fields;
};

struct TypeConstructor {
  TypeConstructor(std::string type, const std::vector<TypePtr>& params = {}) noexcept
  : type(std::move(type)), args(std::move(params)){};
  TypeConstructor() = delete;

  auto operator == (const TypeConstructor& tconstr) const noexcept -> bool {
    if (tconstr.type != type || tconstr.args.size() != args.size()) {
      return false;
    }
    for (size_t i = 0; i < args.size(); ++i) {
      if (!type_ptr_equal(tconstr.args[i], args[i])) {
        return false;
      }
    }
    return true;
  }

  auto operator | (std::vector<TypePtr>& params) -> TypeConstructor {
    return TypeConstructor{"", params};
  }

  [[nodiscard]] auto name() const noexcept -> const std::string&;

  [[nodiscard]] auto get_args() const noexcept -> const std::vector<TypePtr>&;

private:
  std::string type;
  std::vector<TypePtr> args;
};

struct FunctionType {
  FunctionType(std::vector<TypePtr> params, TypePtr rtn_type) noexcept
    : param_types(std::move(params)), return_type(std::move(rtn_type)) {}
  FunctionType() = delete;

  auto operator == (const FunctionType& ftype) const noexcept -> bool {
    if (ftype.param_types.size() != param_types.size()) {
      return false;
    }
    for (size_t i = 0; i < param_types.size(); ++i) {
      if (!type_ptr_equal(ftype.param_types[i], param_types[i])) {
        return false;
      }
    }
    return type_ptr_equal(ftype.return_type, return_type);
  }

  [[nodiscard]] auto get_param_types() const noexcept
      -> const std::vector<TypePtr>& { return param_types; }
  [[nodiscard]] auto get_return_type() const noexcept
      -> const TypePtr& { return return_type; }

private:
  std::vector<TypePtr> param_types;
  TypePtr return_type;
};

struct TypeVar {
  TypeVar() = delete;
  TypeVar(const size_t id_val) noexcept : id(id_val) {}

  auto operator == (const TypeVar& type) const -> bool {
    return type.id == id;
  }

  [[nodiscard]]
  auto get_id() const noexcept -> size_t;

private:
  size_t id;
};

struct Type {
  Type() = delete;
  Type(BaseType type) : value(type) {}
  Type(TypeConstructor type) : value(type) {}
  Type(TypeVar type) : value(type) {}
  Type(PmtType type) : value(type) {}
  Type(FunctionType type) : value(type) {}


  auto operator == (const Type& type) const noexcept -> bool {
    return type.value == value;
  }

  [[nodiscard]]
  auto isBaseType() const noexcept -> bool;

  [[nodiscard]]
  auto isTypeVar() const noexcept -> bool;

  [[nodiscard]]
  auto isPmtType() const noexcept -> bool;

  [[nodiscard]]
  auto isTypeConstructor() const noexcept -> bool;

  [[nodiscard]]
  auto isFunctionType() const noexcept -> bool;

  std::variant<
    BaseType,
    PmtType,
    TypeConstructor,
    FunctionType,
    TypeVar
  > value;
};

struct TypeVarFactory {
  [[nodiscard("Type vars must be used!")]]
  auto operator () () noexcept -> TypePtr {
    return std::make_shared<Type>(TypeVar{counter++});
  }

private:
  size_t counter{0};
};

auto makeTypeConstructor(std::string name, const std::vector<TypePtr>& types) noexcept -> TypePtr;

auto makeFunc(std::vector<TypePtr> from, TypePtr into) noexcept -> TypePtr;

auto makeFunc(TypePtr from, TypePtr into) noexcept -> TypePtr;

auto makeList(TypePtr type) noexcept -> TypePtr;

auto makeTuple(std::vector<TypePtr> args) noexcept -> TypePtr;

auto makeSet(TypePtr type) noexcept -> TypePtr;

auto makeDict(TypePtr key, TypePtr value) noexcept -> TypePtr;

auto makeInt() noexcept -> TypePtr;

auto makeFloat() noexcept -> TypePtr;

auto makeString() noexcept -> TypePtr;

auto makeBool() noexcept -> TypePtr;

auto makeNil() noexcept -> TypePtr;
