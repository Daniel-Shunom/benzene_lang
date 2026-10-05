#include <ether/nodes/node_expr.hpp>
#include <ether/nodes/node_visitor.hpp>

void NDLiteral::accept(Visitor& visitor) { visitor.visit(*this); }

void NDImportDirective::accept(Visitor& visitor) { visitor.visit(*this); }

void NDIdentifier::accept(Visitor& visitor) { visitor.visit(*this); }

void NDFuncParam::accept(Visitor& visitor) { visitor.visit(*this); }

void NDLetBindExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDConstExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDCallExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDCallChain::accept(Visitor& visitor) { visitor.visit(*this); }

void NDFuncDeclExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDTypeDecl::accept(Visitor& visitor) { visitor.visit(*this); }

void NDTypeExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDCaseExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDBinaryExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDUnaryExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDScopeExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDListExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDTupleExpr::accept(Visitor& visitor) { visitor.visit(*this); }

void NDLambdaExpr::accept(Visitor& visitor) { visitor.visit(*this); }
