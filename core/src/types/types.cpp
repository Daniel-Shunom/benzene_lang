#include <ether/types/types.hpp>
#include <memory>
#include <string>
#include <variant>
#include <vector>

auto type_ptr_equal(const TypePtr& lhs, const TypePtr& rhs) noexcept -> bool {
  if (lhs == rhs) {
    return true;
  }
  if (!lhs || !rhs) {
    return false;
  }
  return *lhs == *rhs;
}

auto TypeVar::get_id() const noexcept -> size_t {
  return id;
}

auto Type::isBaseType() const noexcept -> bool {
  return std::holds_alternative<BaseType>(value);
}

auto Type::isTypeVar() const noexcept -> bool {
  return std::holds_alternative<TypeVar>(value);
}

auto Type::isPmtType() const noexcept -> bool {
  return std::holds_alternative<PmtType>(value);
}

auto Type::isTypeConstructor() const noexcept -> bool {
  return std::holds_alternative<TypeConstructor>(value);
}

auto Type::isFunctionType() const noexcept -> bool {
  return std::holds_alternative<FunctionType>(value);
}

auto TypeConstructor::name() const noexcept -> const std::string& {
  return type;
}

auto TypeConstructor::get_args() const noexcept -> const std::vector<TypePtr>& {
  return args;
}

auto makeTypeConstructor(std::string name, const std::vector<TypePtr>& types) noexcept -> TypePtr {
  return std::make_shared<Type>(TypeConstructor{name, std::move(types)});
}

auto makeFunc(std::vector<TypePtr> from, TypePtr into) noexcept -> TypePtr {
  return std::make_shared<Type>(FunctionType(std::move(from), std::move(into)));
}

auto makeFunc(TypePtr from, TypePtr into) noexcept -> TypePtr {
  return makeFunc(std::vector<TypePtr>{std::move(from)}, std::move(into));
}

auto makeList(TypePtr type) noexcept -> TypePtr {
  return std::make_shared<Type>(TypeConstructor("List", {type}));
}

auto makeTuple(std::vector<TypePtr> args)  noexcept -> TypePtr {
  return std::make_shared<Type>(TypeConstructor("Tuple", args));
}

auto makeSet(TypePtr type) noexcept -> TypePtr {
  return std::make_shared<Type>(TypeConstructor("Set", {type}));
}

auto makeDict(TypePtr key, TypePtr value) noexcept -> TypePtr {
  return std::make_shared<Type>(TypeConstructor("Dict", {key, value}));
}

auto makeInt() noexcept -> TypePtr {
  return std::make_shared<Type>(BaseType::Int);
}

auto makeFloat() noexcept -> TypePtr {
  return std::make_shared<Type>(BaseType::Float);
}

auto makeString() noexcept -> TypePtr {
  return std::make_shared<Type>(BaseType::String);
}

auto makeBool() noexcept -> TypePtr {
  return std::make_shared<Type>(BaseType::Bool);
}

auto makeNil() noexcept -> TypePtr {
  return std::make_shared<Type>(BaseType::Nil);
}
