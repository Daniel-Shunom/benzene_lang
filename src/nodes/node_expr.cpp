#include "node_expr.hpp"
#include "node_visitor.hpp"

void NDLiteral::accept(Visitor& v) { v.visit(*this); }

void NDImportDirective::accept(Visitor& v) { v.visit(*this); }

void NDIdentifier::accept(Visitor& v) { v.visit(*this); }

void NDLetBindExpr::accept(Visitor& v) { v.visit(*this); }

void NDConstExpr::accept(Visitor& v) { v.visit(*this); }

void NDCallExpr::accept(Visitor& v) { v.visit(*this); }

void NDCallChain::accept(Visitor& v) { v.visit(*this); }

void NDFuncDeclExpr::accept(Visitor& v) { v.visit(*this); }

void NDCaseExpr::accept(Visitor& v) { v.visit(*this); }

void NDBinaryExpr::accept(Visitor& v) { v.visit(*this); }

void NDUnaryExpr::accept(Visitor& v) { v.visit(*this); }

void NDScopeExpr::accept(Visitor& v) { v.visit(*this); }
