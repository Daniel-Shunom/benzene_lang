#pragma once
#include "ether/ast_passes/type_check/modules/constrain.hpp"
#include "ether/ast_passes/type_check/modules/module.hpp"
#include "ether/ast_passes/type_check/modules/unify.hpp"
#include <ether/nodes/node_visitor.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/types/types.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class TypeChecker: public Visitor {
public:
  explicit TypeChecker(bool print_types = false);
  TypeChecker(bool print_types, DiagnosticEngine& diagnostics);
  void dispatch_to_modules(Node&);
  void set_print_types(bool enabled) noexcept { print_types = enabled; }
  [[nodiscard]] auto prints_types() const noexcept -> bool { return print_types; }
  [[nodiscard]] auto types() noexcept -> TypeEnvironment& { return type_environment; }
  [[nodiscard]] auto types() const noexcept -> const TypeEnvironment& {
    return type_environment;
  }
  [[nodiscard]] auto constraints() const noexcept -> const Constraints&;
  [[nodiscard]] auto substitutions() const noexcept -> const Subst&;
  void unify_constraints();
  [[nodiscard]] auto diagnostic_engine() const noexcept -> DiagnosticEngine& {
    return diag_engine;
  }
  [[nodiscard]] auto instantiate(const Scheme&) -> TypePtr;
  [[nodiscard]] auto generalize(TypePtr, SymbolAttr* excluded = nullptr) -> Scheme;
  void register_constructor_type(std::string name, TypePtr parent,
                                 TypePtr fields, std::vector<TypeVarId> quantified = {});
  [[nodiscard]] auto instantiate_constructor(const std::string& name) -> TypePtr;
  [[nodiscard]] auto constructor_scheme(const std::string& name) const -> const Scheme*;
  [[nodiscard]] auto constructor_type(const std::string& name) const
      -> TypePtr;
  [[nodiscard]] auto constructor_parent(const std::string& name) const
      -> TypePtr;
  void register_type_alias(std::string name, TypePtr target, std::vector<TypeVarId> parameters = {}) {
    aliases.declare(std::move(name), std::move(target), std::move(parameters));
  }
  [[nodiscard]] auto type_aliases() const noexcept -> const TypeAliasTable& {
    return aliases;
  }
  [[nodiscard]] auto resolve_alias(TypePtr, SourceLocation location = {}) const -> TypePtr;
  void generalize_binding(NDLetBindExpr&);
  void push_type_scope() { type_environment.push_scope(); }
  void pop_type_scope() { type_environment.pop_scope(); }
  void activate_type_symbol(SymbolAttr* symbol) { type_environment.activate(symbol); }

  void visit(NDLiteral& expr)          override;
  void visit(NDImportDirective& expr)  override;
  void visit(NDIdentifier& expr)       override;
  void visit(NDLetBindExpr& expr)      override;
  void visit(NDConstExpr& expr)        override;
  void visit(NDCallExpr& expr)         override;
  void visit(NDCallChain& expr)        override;
  void visit(NDTypeDecl& expr)         override;
  void visit(NDTypeExpr& expr)         override;
  void visit(NDFuncDeclExpr& expr)     override;
  void visit(NDCaseExpr& expr)         override;
  void visit(NDBinaryExpr& expr)       override;
  void visit(NDUnaryExpr& expr)        override;
  void visit(NDScopeExpr& expr)        override;
  void visit(NDTupleExpr& expr)        override;
  void visit(NDListExpr& expr)         override;
  void visit(NDLambdaExpr& expr)       override;
  void visit(NDFuncParam& expr)        override;

  TypeVarFactory varFactory;
private:
  auto free_type_vars(TypePtr, std::vector<TypeVarId>&) const -> void;
  std::vector<std::unique_ptr<TCModule>> modules;
  TCModule_Constrain* constrain_module = nullptr;
  std::unique_ptr<Unifier> unifier;
  TypeEnvironment type_environment;
  bool print_types = false;
  DiagnosticEngine& diag_engine;
  struct ConstructorInfo { TypePtr parent; TypePtr fields; Scheme scheme; };
  std::unordered_map<std::string, ConstructorInfo> constructors;
  TypeAliasTable aliases;
};
