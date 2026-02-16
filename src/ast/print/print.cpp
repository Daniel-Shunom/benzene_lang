#include "print.hpp"

void TreePrinter::push() { indent += 2; }
void TreePrinter::pop()  { indent -= 2; }

void TreePrinter::line(const std::string& text) {
  out << std::string(indent, ' ') << text << '\n';
}

void TreePrinter::print_token(const Token& tok) {
  out << tok.token_value;
}

std::string TreePrinter::poison_mark(bool is_poisoned) {
  return is_poisoned ? " [POISONED]" : "";
}

void TreePrinter::visit(NDLiteral& n) {
  line("Literal" + poison_mark(n.is_poisoned));
  push();
  line("value: " + n.literal.token_value);
  pop();
}

void TreePrinter::visit(NDImportDirective& n) {
  line("ImportDirective" + poison_mark(n.is_poisoned));
  push();
  line("module: " + n.import_directive.token_value);
  pop();
}

void TreePrinter::visit(NDIdentifier& n) {
  line("Identifier" + poison_mark(n.is_poisoned));
  push();
  line("name: " + n.identifier.token_value);
  pop();
}

void TreePrinter::visit(NDUnaryExpr& n) {
  line("UnaryExpr" + poison_mark(n.is_poisoned));
  push();
  if (n.op) {
    line("op: " + n.op->token_value);
  }
  line("rhs:");
  push();
  n.rhs->accept(*this);
  pop();
  pop();
}

void TreePrinter::visit(NDBinaryExpr& n) {
  line("BinaryExpr" + poison_mark(n.is_poisoned));
  push();
  line("lhs:");
  push();
  n.lhs->accept(*this);
  pop();
  line("op: " + n.op.token_value);
  line("rhs:");
  push();
  n.rhs->accept(*this);
  pop();
  pop();
}

void TreePrinter::visit(NDScopeExpr& n) {
  line("ScopeExpr" + poison_mark(n.is_poisoned));
  push();
  for (auto& expr : n.expressions) {
    expr->accept(*this);
  }
  pop();
}

void TreePrinter::visit(NDLetBindExpr& n) {
  line("LetBindExpr" + poison_mark(n.is_poisoned));
  push();
  line("identifier:");
  push();
  n.identifier->accept(*this);
  pop();
  line("value:");
  push();
  n.bound_value->accept(*this);
  pop();
  pop();
}

void TreePrinter::visit(NDConstExpr& n) {
  line("ConstExpr" + poison_mark(n.is_poisoned));
  push();
  line("identifier:");
  push();
  n.identifier->accept(*this);
  pop();
  line("literal:");
  push();
  visit(n.literal);
  pop();
  pop();
}

void TreePrinter::visit(NDCallExpr& n) {
  line("CallExpr" + poison_mark(n.is_poisoned));
  push();
  line("callee:");
  push();
  n.identifier->accept(*this);
  pop();
  line("args:");
  push();
  for (auto& arg : n.args) {
    arg->accept(*this);
  }
  pop();
  pop();
}

void TreePrinter::visit(NDCallChain& n) {
  line("CallChain" + poison_mark(n.is_poisoned));
  push();
  for (auto& call : n.calls) {
    call->accept(*this);
  }
  pop();
}

void TreePrinter::visit(NDFuncDeclExpr& n) {
  line("FuncDecl" + poison_mark(n.is_poisoned));
  push();
  line("name: " + n.func_identifier.token_value);
  if (n.return_type) {
    line("return_type: " + n.return_type->token_value);
  }
  line("params:");
  push();
  for (auto& p : n.func_params) {
    line(p.param_token.token_value + (p.param_type ? " : " + p.param_type->token_value : ""));
  }
  pop();
  line("body:");
  push();
  for (auto& expr : n.func_body) {
    expr->accept(*this);
  }
  pop();
  pop();
}

void TreePrinter::visit(NDCaseExpr& n) {
  line("CaseExpr" + poison_mark(n.is_poisoned));
  push();
  line("conditions:");
  push();
  for (auto& c : n.conditions) {
    c->accept(*this);
  }
  pop();
  line("branches:");
  push();
  for (auto& b : n.branches) {
    line("Branch:");
    push();
    line("pattern:");
    push();
    for (auto& p : b.pattern) {
      p->accept(*this);
    }
    pop();
    line("result:");
    push();
    b.result->accept(*this);
    pop();
    pop();
  }
  pop();
  pop();
}
