#pragma once
#include "ether/ast_passes/type_check/modules/constrain.hpp"
#include "ether/ast_passes/type_check/modules/module.hpp"
#include "ether/ast_passes/type_check/modules/unify.hpp"
#include <ether/nodes/node_visitor.hpp>
#include <ether/nodes/node_expr.hpp>
#include <ether/diagnostics/diagnostic_eng.hpp>
#include <ether/types/types.hpp>
#include <memory>
#include <vector>

class TypeChecker: public Visitor {
public:
  explicit TypeChecker(bool print_types = false);
  TypeChecker(bool print_types, DiagnosticEngine& diagnostics);
  void dispatch_to_modules(Node&);
  void set_print_types(bool enabled) noexcept { print_types = enabled; }
  [[nodiscard]] auto prints_types() const noexcept -> bool { return print_types; }
  [[nodiscard]] auto types() noexcept -> TypeEnvironment& { return type_environment; }
  [[nodiscard]] auto constraints() const noexcept -> const Constraints&;
  [[nodiscard]] auto substitutions() const noexcept -> const Subst&;
  void unify_constraints();
  [[nodiscard]] auto diagnostic_engine() const noexcept -> DiagnosticEngine& {
    return diag_engine;
  }

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
  std::vector<std::unique_ptr<TCModule>> modules;
  TCModule_Constrain* constrain_module = nullptr;
  std::unique_ptr<Unifier> unifier;
  TypeEnvironment type_environment;
  bool print_types = false;
  DiagnosticEngine& diag_engine;
};
