#include <ether/types/type_printer.hpp>
#include <unordered_set>

namespace {
auto paint(const std::string& text, const char* color, bool enabled) -> std::string {
  return enabled ? std::string(color) + text + "\033[0m" : text;
}

constexpr auto TYPE = "\033[36m";
constexpr auto FUNCTION = "\033[35m";
constexpr auto FIELD = "\033[32m";
constexpr auto VARIABLE = "\033[34m";
constexpr auto PUNCTUATION = "\033[90m";
constexpr auto ARROW = "\033[33m";
constexpr auto MISSING = "\033[31m";

auto render(const Type* type, std::unordered_set<const Type*>& active, bool color,
            const Subst* substitutions) -> std::string {
  if (!type) return paint("<unset>", MISSING, color);
  if (!active.insert(type).second) return paint("<recursive>", MISSING, color);

  if (substitutions) {
    if (const auto* variable = std::get_if<TypeVar>(&type->value)) {
      if (auto found = substitutions->find(variable->get_id());
          found != substitutions->end()) {
        active.erase(type);
        return render(found->second.get(), active, color, substitutions);
      }
    }
  }

  std::string result;
  if (const auto* base = std::get_if<BaseType>(&type->value)) {
    switch (*base) {
      case BaseType::Int: result = "Int"; break;
      case BaseType::Float: result = "Float"; break;
      case BaseType::String: result = "String"; break;
      case BaseType::Bool: result = "Bool"; break;
      case BaseType::Nil: result = "Nil"; break;
    }
    result = paint(result, TYPE, color);
  } else if (const auto* variable = std::get_if<TypeVar>(&type->value)) {
    result = paint("'t" + std::to_string(variable->get_id()), VARIABLE, color);
  } else if (const auto* constructor = std::get_if<TypeConstructor>(&type->value)) {
    const auto& args = constructor->get_args();
    result = constructor->name();
    const bool function = result == "Fn" && !args.empty();
    result = paint(result, function ? FUNCTION : TYPE, color);
    if (function || !args.empty()) {
      result += paint("(", PUNCTUATION, color);
      const auto count = function ? args.size() - 1 : args.size();
      for (size_t i = 0; i < count; ++i) {
        if (i) result += paint(", ", PUNCTUATION, color);
        result += render(args[i].get(), active, color, substitutions);
      }
      result += paint(")", PUNCTUATION, color);
      if (function) result += paint(" :> ", ARROW, color) + render(args.back().get(), active, color, substitutions);
    }
  } else if (const auto* function = std::get_if<FunctionType>(&type->value)) {
    result = paint("Fn", FUNCTION, color) + paint("(", PUNCTUATION, color);
    const auto& params = function->get_param_types();
    for (size_t i = 0; i < params.size(); ++i) {
      if (i) result += paint(", ", PUNCTUATION, color);
      result += render(params[i].get(), active, color, substitutions);
    }
    result += paint(")", PUNCTUATION, color) + paint(" :> ", ARROW, color) +
              render(function->get_return_type().get(), active, color, substitutions);
  } else if (const auto* fields = std::get_if<PmtType>(&type->value)) {
    result = paint(fields->get_name(), TYPE, color) + paint("(", PUNCTUATION, color);
    for (size_t i = 0; i < fields->get_fields().size(); ++i) {
      if (i) result += paint(", ", PUNCTUATION, color);
      const auto& field = fields->get_fields()[i];
      result += paint(field.name(), FIELD, color) + paint(": ", PUNCTUATION, color) + render(field.get_type().get(), active, color, substitutions);
    }
    result += paint(")", PUNCTUATION, color);
  }
  active.erase(type);
  return result;
}
} // namespace

auto TypePrinter::print(const TypePtr& type) const -> std::string {
  std::unordered_set<const Type*> active;
  return render(type.get(), active, use_color, substitutions);
}

auto TypePrinter::print(const Type& type) const -> std::string {
  std::unordered_set<const Type*> active;
  return render(&type, active, use_color, substitutions);
}
