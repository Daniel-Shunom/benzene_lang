#include <ether/types/types.hpp>
#include <memory>
#include <string>
#include <variant>
#include <vector>

size_t TypeVar::get_id() const {
  return id;
}

const bool Type::isBaseType() const {
  return std::holds_alternative<BaseType>(value);
}

const bool Type::isTypeVar() const {
  return std::holds_alternative<TypeVar>(value);
}

const bool Type::isPmtType() const {
  return std::holds_alternative<PmtType>(value);
}

const bool Type::isTypeConstructor() const {
  return std::holds_alternative<TypeConstructor>(value);
}

const std::string& TypeConstructor::name() const {
  return type;
}

const std::vector<TypePtr>& TypeConstructor::get_args() const {
  return args;
}

TypePtr makeFunc(TypePtr from, TypePtr to) {
  return std::make_shared<Type>(TypeConstructor("Fn", {from, to}));
}

TypePtr makeList(TypePtr type) {
  return std::make_shared<Type>(TypeConstructor("List", {type}));
}

TypePtr makeTuple(std::vector<TypePtr> args) {
  return std::make_shared<Type>(TypeConstructor("Tuple", args));
}

TypePtr makeSet(TypePtr type) {
  return std::make_shared<Type>(TypeConstructor("Set", {type}));
}

TypePtr makeDict(TypePtr key, TypePtr value) {
  return std::make_shared<Type>(TypeConstructor("Dict", {key, value}));
}

const TypePtr makeInt() {
  return std::make_shared<Type>(BaseType::Int);
}

const TypePtr makeFloat() {
  return std::make_shared<Type>(BaseType::Float);
}

const TypePtr makeString() {
  return std::make_shared<Type>(BaseType::String);
}

const TypePtr makeBool() {
  return std::make_shared<Type>(BaseType::Bool);
}

const TypePtr makeNil() {
  return std::make_shared<Type>(BaseType::Nil);
}
