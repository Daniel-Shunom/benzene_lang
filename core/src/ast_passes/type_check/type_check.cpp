#include "ether/ast_passes/type_check/modules/constrain.hpp"
#include "ether/ast_passes/type_check/modules/populate.hpp"
#include "ether/ast_passes/type_check/modules/unify.hpp"
#include "ether/nodes/node_expr.hpp"
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ether/types/type_printer.hpp>
#include <iostream>
#include <memory>
#include <algorithm>
#include <unordered_set>

namespace {
  auto default_diagnostics() -> DiagnosticEngine& {
    static DiagnosticEngine diagnostics;
    return diagnostics;
  }
}

TypeChecker::TypeChecker(bool print_types)
  : TypeChecker(print_types, default_diagnostics()) {}

TypeChecker::TypeChecker(bool print_types, DiagnosticEngine& diagnostics)
  : print_types(print_types), diag_engine(diagnostics) {
  modules.push_back(std::make_unique<TCModule_Populate>(*this));
  auto constrain = std::make_unique<TCModule_Constrain>(*this);
  constrain_module = constrain.get();
  modules.push_back(std::move(constrain));
  unifier = std::make_unique<Unifier>(diag_engine);
}

auto TypeChecker::constraints() const noexcept -> const Constraints& {
  static const Constraints empty;
  return constrain_module ? constrain_module->constraints : empty;
}

auto TypeChecker::substitutions() const noexcept -> const Subst& {
  static const Subst empty;
  return unifier ? unifier->substitutions() : empty;
}

void TypeChecker::unify_constraints() {
  if (unifier && constrain_module) {
    unifier->solve(constrain_module->constraints);
  }
}

namespace {
  auto clone_type(const TypePtr& type,
                  const std::unordered_map<TypeVarId, TypePtr>& replacements)
      -> TypePtr {
    if (!type) return nullptr;
    if (type->isTypeVar()) {
      auto id = std::get<TypeVar>(type->value).get_id();
      if (auto it = replacements.find(id); it != replacements.end()) return it->second;
      return type;
    }
    if (type->isFunctionType()) {
      const auto& fn = std::get<FunctionType>(type->value);
      std::vector<TypePtr> ps;
      for (const auto& p : fn.get_param_types()) ps.push_back(clone_type(p, replacements));
      return makeFunc(std::move(ps), clone_type(fn.get_return_type(), replacements));
    }
    if (type->isTypeConstructor()) {
      const auto& c = std::get<TypeConstructor>(type->value);
      std::vector<TypePtr> args;
      for (const auto& a : c.get_args()) args.push_back(clone_type(a, replacements));
      return makeTypeConstructor(c.name(), args);
    }
    if (type->isPmtType()) {
      const auto& p = std::get<PmtType>(type->value);
      std::vector<TypeField> fields;
      for (const auto& f : p.get_fields()) fields.emplace_back(f.name(), clone_type(f.get_type(), replacements));
      return std::make_shared<Type>(PmtType{p.get_name(), std::move(fields)});
    }
    return type;
  }
}

auto TypeChecker::instantiate(const Scheme& scheme) -> TypePtr {
  std::unordered_map<TypeVarId, TypePtr> replacements;
  for (auto id : scheme.quantified) replacements.emplace(id, varFactory());
  return clone_type(scheme.type, replacements);
}

auto TypeChecker::free_type_vars(TypePtr type, std::vector<TypeVarId>& out) const -> void {
  if (!type) return;
  if (type->isTypeVar()) { out.push_back(std::get<TypeVar>(type->value).get_id()); return; }
  if (type->isFunctionType()) {
    const auto& f = std::get<FunctionType>(type->value);
    for (const auto& p : f.get_param_types()) free_type_vars(p, out);
    free_type_vars(f.get_return_type(), out); return;
  }
  if (type->isTypeConstructor()) {
    for (const auto& a : std::get<TypeConstructor>(type->value).get_args()) free_type_vars(a, out);
    return;
  }
  if (type->isPmtType()) {
    for (const auto& f : std::get<PmtType>(type->value).get_fields()) free_type_vars(f.get_type(), out);
  }
}

auto TypeChecker::generalize(TypePtr type, SymbolAttr* excluded) -> Scheme {
  type = unifier ? unifier->apply(type) : type;
  std::vector<TypeVarId> vars, env_vars;
  free_type_vars(type, vars);
  for (const auto& [symbol, scheme] : type_environment.active_bindings()) {
    if (symbol == excluded) continue;
    std::vector<TypeVarId> present;
    free_type_vars(unifier ? unifier->apply(scheme.type) : scheme.type, present);
    for (auto q : scheme.quantified) present.erase(std::remove(present.begin(), present.end(), q), present.end());
    env_vars.insert(env_vars.end(), present.begin(), present.end());
  }
  std::sort(vars.begin(), vars.end());
  vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
  std::sort(env_vars.begin(), env_vars.end());
  env_vars.erase(std::unique(env_vars.begin(), env_vars.end()), env_vars.end());
  vars.erase(std::remove_if(vars.begin(), vars.end(), [&](auto id) {
    return std::binary_search(env_vars.begin(), env_vars.end(), id);
  }), vars.end());
  return Scheme{std::move(vars), std::move(type)};
}

void TypeChecker::register_constructor_type(std::string name, TypePtr parent,
                                            TypePtr fields) {
  constructors[std::move(name)] = ConstructorInfo{std::move(parent), std::move(fields)};
}

auto TypeChecker::constructor_type(const std::string& name) const -> TypePtr {
  if (auto it = constructors.find(name); it != constructors.end()) {
    return it->second.fields;
  }
  return nullptr;
}

auto TypeChecker::constructor_parent(const std::string& name) const -> TypePtr {
  if (auto it = constructors.find(name); it != constructors.end()) {
    return it->second.parent;
  }
  return nullptr;
}

void TypeChecker::visit(NDLiteral& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDImportDirective& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDIdentifier& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDLetBindExpr& node) {
  dispatch_to_modules(node);
}

void TypeChecker::generalize_binding(NDLetBindExpr& node) {
  unify_constraints();
  if (!node.identifier || !node.identifier->identifier_symbol) return;
  auto* symbol = node.identifier->identifier_symbol;
  auto type = unifier->apply(node.identifier->inferred_type);
  type_environment.bind(symbol, generalize(type, symbol));
  node.identifier->inferred_type = type;
  node.inferred_type = type;
}
void TypeChecker::visit(NDConstExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDCallExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDCallChain& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDTypeDecl& node) {dispatch_to_modules(node); }
void TypeChecker::visit(NDFuncDeclExpr& node) {
  dispatch_to_modules(node);
  unify_constraints();
  if (node.func_sym) {
    auto type = unifier->apply(node.inferred_type);
    type_environment.bind(node.func_sym, generalize(type, node.func_sym));
    node.inferred_type = type;
  }
}
void TypeChecker::visit(NDCaseExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDBinaryExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDUnaryExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDScopeExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDTupleExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDListExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDLambdaExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDFuncParam& node) { dispatch_to_modules(node); }

void TypeChecker::dispatch_to_modules(Node& node) {
  for (auto& module: modules) {
    node.accept(*module);
  }

  if (print_types && node.inferred_type) {
    std::cout << "[typecheck] " << TypePrinter{true}.print(node.inferred_type)
              << '\n';
  }
}

void TypeChecker::visit(NDTypeExpr& node) { dispatch_to_modules(node); }
