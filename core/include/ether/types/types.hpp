#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

struct Type;

using TypeVarId = size_t;

using TypePtr = std::shared_ptr<Type>;

struct Scheme {
  std::vector<TypeVarId> quantified{};
  TypePtr type;
};

using Env = std::unordered_map<std::string, Scheme>;

using Subst = std::unordered_map<TypeVarId, TypePtr>;

class TypeTable{
public:
  void new_type_scope();
  void pop_type_scope();

  [[nodiscard]]
  Scheme* lookup(const std::string&);

  [[nodiscard]]
  Scheme* declare(const std::string&, Scheme);

private:
  std::vector<Env> type_scopes;
};

struct TypeScopeGuard {
  TypeScopeGuard(TypeTable& t) : table(t) {
    table.new_type_scope();
  }

  ~TypeScopeGuard() {
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
  TypeField(std::string field, TypePtr type)
  : field(field), type(type) {}

  bool operator == (const TypeField& type_field) const {
    return type_field.field == field
      && type_field.type == type;
  }

private:
  std::string field;
  TypePtr type;
};

struct PmtType{
  PmtType() = delete;
  PmtType(std::string name, std::vector<TypeField> fields)
  : name(name), type_fields(fields) {}

  bool operator == (const PmtType& type) const {
    return type.name == name
      && type.type_fields == type_fields;
  }

private:
  std::string name;
  std::vector<TypeField> type_fields;
};

struct TypeConstructor {
  TypeConstructor(std::string type, const std::vector<TypePtr>& params = {})
  : type(type), args(std::move(params)){};
  TypeConstructor() = delete;

  bool operator == (const TypeConstructor& tconstr) const {
    return tconstr.type == type
      && tconstr.args == args;
  }

  const std::string& name() const;

  const std::vector<TypePtr>& get_args() const;

private:
  std::string type;
  std::vector<TypePtr> args;
};

struct TypeVar {
  TypeVar() = delete;
  TypeVar(const size_t id) : id(id) {}

  bool operator == (const TypeVar& type) const {
    return type.id == id;
  }

  [[nodiscard]]
  size_t get_id() const;

private:
  size_t id;
};

struct Type {
  Type() = delete;
  Type(BaseType b) : value(b) {}
  Type(TypeConstructor b) : value(b) {}
  Type(TypeVar b) : value(b) {}
  Type(PmtType b) : value(b) {}


  bool operator == (const Type& type) const {
    return type.value == value;
  }

  const bool isBaseType() const;

  const bool isTypeVar() const;

  const bool isPmtType() const;

  const bool isTypeConstructor() const;

  std::variant<
    BaseType,
    PmtType,
    TypeConstructor,
    TypeVar
  > value;
};

struct TypeVarFactory {
  [[nodiscard("Type vars must be used!")]]
  TypeVar operator () () {
    return TypeVar(counter++);
  }

private:
  size_t counter{0};
};

TypePtr makeFunc(TypePtr from, TypePtr to);

TypePtr makeList(TypePtr type);

TypePtr makeTuple(std::vector<TypePtr> args);

TypePtr makeSet(TypePtr type);

TypePtr makeDict(TypePtr key, TypePtr value);

const TypePtr makeInt();

const TypePtr makeFloat();

const TypePtr makeString();

const TypePtr makeBool();

const TypePtr makeNil();
