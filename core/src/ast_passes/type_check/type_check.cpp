#include "ether/ast_passes/type_check/modules/constrain.hpp"
#include "ether/ast_passes/type_check/modules/populate.hpp"
#include "ether/ast_passes/type_check/modules/unify.hpp"
#include "ether/nodes/node_expr.hpp"
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ether/types/type_printer.hpp>
#include <iostream>
#include <memory>

TypeChecker::TypeChecker(bool print_types) : print_types(print_types) {
  modules.push_back(std::make_unique<TCModule_Populate>(*this));
  modules.push_back(std::make_unique<TCModule_Constrain>(*this));
  modules.push_back(std::make_unique<TCModule_Unify>(*this));
}

void TypeChecker::visit(NDLiteral& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDImportDirective& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDIdentifier& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDLetBindExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDConstExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDCallExpr& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDCallChain& node) { dispatch_to_modules(node); }
void TypeChecker::visit(NDTypeDecl& node) {dispatch_to_modules(node); }
void TypeChecker::visit(NDFuncDeclExpr& node) { dispatch_to_modules(node); }
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
