#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

struct Type;

using TypeVarId = size_t;

using TypePtr = std::shared_ptr<Type>;

struct Scheme {
  std::vector<TypeVarId> quantified;
  TypePtr type;
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

struct  TypeField {
  TypeField() = delete;
  TypeField(std::string field, TypePtr type) noexcept
  : field(std::move(field)), type(std::move(type)) {}

  auto operator == (const TypeField& type_field) const noexcept -> bool {
    return type_field.field == field
      && type_field.type == type;
  }

private:
  std::string field;
  TypePtr type;
};

struct PmtType{
  PmtType() = delete;
  PmtType(std::string name, std::vector<TypeField> fields) noexcept
  : name(std::move(name)), type_fields(std::move(fields)) {}

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
    return tconstr.type == type
      && tconstr.args == args;
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


  auto operator == (const Type& type) const noexcept -> bool {
    return type.value == value;
  }

  [[nodiscard]]
  auto isBaseType() const noexcept -> const bool;

  [[nodiscard]]
  auto isTypeVar() const noexcept -> const bool;

  [[nodiscard]]
  auto isPmtType() const noexcept -> const bool;

  [[nodiscard]]
  auto isTypeConstructor() const noexcept -> const bool;

  std::variant<
    BaseType,
    PmtType,
    TypeConstructor,
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

auto makeFunc(TypePtr from, TypePtr to) noexcept -> TypePtr;

auto makeList(TypePtr type) noexcept -> TypePtr;

auto makeTuple(std::vector<TypePtr> args) noexcept -> TypePtr;

auto makeSet(TypePtr type) noexcept -> TypePtr;

auto makeDict(TypePtr key, TypePtr value) noexcept -> TypePtr;

auto makeInt() noexcept -> const TypePtr;

auto makeFloat() noexcept -> const TypePtr;

auto makeString() noexcept -> const TypePtr;

auto makeBool() noexcept -> const TypePtr;

auto makeNil() noexcept -> const TypePtr;
