#include <ether/ast/type_check/type_check.hpp>

void TypeChecker::visit(NDLiteral&) {}
void TypeChecker::visit(NDImportDirective&) {}
void TypeChecker::visit(NDIdentifier&) {}
void TypeChecker::visit(NDLetBindExpr&) {}
void TypeChecker::visit(NDConstExpr&) {}
void TypeChecker::visit(NDCallExpr&) {}
void TypeChecker::visit(NDCallChain&) {}
void TypeChecker::visit(NDFuncDeclExpr&) {}
void TypeChecker::visit(NDCaseExpr&) {}
void TypeChecker::visit(NDBinaryExpr&) {}
void TypeChecker::visit(NDUnaryExpr&) {}
void TypeChecker::visit(NDScopeExpr&) {}
