#include "print.hpp"

namespace {
  constexpr auto RESET   = "\033[0m";
  constexpr auto BOLD    = "\033[1m";
  constexpr auto DIM     = "\033[2m";

  constexpr auto RED     = "\033[31m";
  constexpr auto YELLOW  = "\033[33m";
  constexpr auto CYAN    = "\033[36m";
  constexpr auto BLUE    = "\033[34m";
  constexpr auto GREEN   = "\033[32m";
}

std::string TreePrinter::prefix() const {
  std::string p;
  for (size_t i = 0; i + 1 < last_stack.size(); ++i) {
    p += last_stack[i]
      ? std::string("    ")
      : std::string(DIM) + BLUE + "│   " + RESET;
  }
  return p;
}

std::string TreePrinter::connector() const {
  if (last_stack.empty()) return "";
  return std::string(DIM) + BLUE
       + (last_stack.back() ? "└── " : "├── ")
       + RESET;
}

void TreePrinter::emit_line(const std::string& content) {
  out << prefix() << connector() << content << '\n';
}

std::string TreePrinter::type_header(const std::string& type_name, bool is_poisoned) {
  std::string s = std::string(BOLD) + CYAN + type_name + RESET;
  if (is_poisoned) {
    s += std::string(" ") + BOLD + RED + "[POISONED]" + RESET;
  }
  return s;
}

void TreePrinter::child_field(const std::string& label, Node& child, bool is_last) {
  enter_child(is_last);
  emit_line(std::string(DIM) + label + RESET);
  enter_child(true);
  child.accept(*this);
  leave_child();
  leave_child();
}

void TreePrinter::leaf_field(const std::string& label, const std::string& value, bool is_last) {
  enter_child(is_last);
  emit_line(
    std::string(DIM) + label + ":" + RESET + " " +
    std::string(YELLOW) + value + RESET
  );
  leave_child();
}

void TreePrinter::visit(NDLiteral& n) {
  emit_line(type_header("Literal", n.is_poisoned));
  leaf_field("value", n.literal.token_value, true);
}

void TreePrinter::visit(NDImportDirective& n) {
  emit_line(type_header("ImportDirective", n.is_poisoned));
  leaf_field("module", n.import_directive.token_value, true);
}

void TreePrinter::visit(NDIdentifier& n) {
  emit_line(type_header("Identifier", n.is_poisoned));
  leaf_field("name", n.identifier.token_value, true);
}

void TreePrinter::visit(NDUnaryExpr& n) {
  emit_line(type_header("UnaryExpr", n.is_poisoned));
  if (n.op) {
    leaf_field("op", n.op->token_value, false);
  }
  child_field("rhs", *n.rhs, true);
}

void TreePrinter::visit(NDBinaryExpr& n) {
  emit_line(type_header("BinaryExpr", n.is_poisoned));
  child_field("lhs", *n.lhs, false);
  leaf_field("op", n.op.token_value, false);
  child_field("rhs", *n.rhs, true);
}

void TreePrinter::visit(NDScopeExpr& n) {
  emit_line(type_header("ScopeExpr", n.is_poisoned));
  for (size_t i = 0; i < n.expressions.size(); ++i) {
    bool last = (i + 1 == n.expressions.size());
    enter_child(last);
    n.expressions[i]->accept(*this);
    leave_child();
  }
}

void TreePrinter::visit(NDLetBindExpr& n) {
  emit_line(type_header("LetBindExpr", n.is_poisoned));
  child_field("identifier", *n.identifier, false);
  child_field("value", *n.bound_value, true);
}

void TreePrinter::visit(NDConstExpr& n) {
  emit_line(type_header("ConstExpr", n.is_poisoned));
  child_field("identifier", *n.identifier, false);
  child_field("literal", n.literal, true);
}

void TreePrinter::visit(NDCallExpr& n) {
  emit_line(type_header("CallExpr", n.is_poisoned));
  child_field("callee", *n.identifier, n.args.empty());
  if (!n.args.empty()) {
    enter_child(true);
    emit_line(std::string(DIM) + "args" + RESET);
    for (size_t i = 0; i < n.args.size(); ++i) {
      bool last = (i + 1 == n.args.size());
      enter_child(last);
      n.args[i]->accept(*this);
      leave_child();
    }
    leave_child();
  }
}

void TreePrinter::visit(NDCallChain& n) {
  emit_line(type_header("CallChain", n.is_poisoned));
  for (size_t i = 0; i < n.calls.size(); ++i) {
    bool last = (i + 1 == n.calls.size());
    enter_child(last);
    n.calls[i]->accept(*this);
    leave_child();
  }
}

void TreePrinter::visit(NDFuncDeclExpr& n) {
  emit_line(type_header("FuncDecl", n.is_poisoned));
  bool has_return = n.return_type.has_value();
  bool has_params = !n.func_params.empty();
  bool has_body = !n.func_body.empty();

  leaf_field("name", n.func_identifier.token_value, !has_return && !has_params && !has_body);

  if (has_return) {
    leaf_field("return_type", n.return_type->token_value, !has_params && !has_body);
  }

  if (has_params) {
    enter_child(!has_body);
    emit_line(std::string(DIM) + "params" + RESET);
    for (size_t i = 0; i < n.func_params.size(); ++i) {
      const auto& p = n.func_params[i];
      bool last = (i + 1 == n.func_params.size());
      std::string text =
        std::string(GREEN) + p.param_token.token_value + RESET;
      if (p.param_type) {
        text += std::string(DIM) + " : " + RESET +
                std::string(YELLOW) + p.param_type->token_value + RESET;
      }
      enter_child(last);
      emit_line(text);
      leave_child();
    }
    leave_child();
  }

  if (has_body) {
    enter_child(true);
    emit_line(std::string(DIM) + "body" + RESET);
    for (size_t i = 0; i < n.func_body.size(); ++i) {
      bool last = (i + 1 == n.func_body.size());
      enter_child(last);
      n.func_body[i]->accept(*this);
      leave_child();
    }
    leave_child();
  }
}

void TreePrinter::visit(NDCaseExpr& n) {
  emit_line(type_header("CaseExpr", n.is_poisoned));
  bool has_branches = !n.branches.empty();

  enter_child(!has_branches);
  emit_line(std::string(DIM) + "conditions" + RESET);
  for (size_t i = 0; i < n.conditions.size(); ++i) {
    bool last = (i + 1 == n.conditions.size());
    enter_child(last);
    n.conditions[i]->accept(*this);
    leave_child();
  }
  leave_child();

  if (has_branches) {
    enter_child(true);
    emit_line(std::string(DIM) + "branches" + RESET);
    for (size_t i = 0; i < n.branches.size(); ++i) {
      const auto& b = n.branches[i];
      bool last = (i + 1 == n.branches.size());
      enter_child(last);
      emit_line(std::string(BOLD) + CYAN + "Branch" + RESET);
      enter_child(false);
      emit_line(std::string(DIM) + "pattern" + RESET);
      for (size_t j = 0; j < b.pattern.size(); ++j) {
        bool plast = (j + 1 == b.pattern.size());
        enter_child(plast);
        b.pattern[j]->accept(*this);
        leave_child();
      }
      leave_child();
      child_field("result", *b.result, true);
      leave_child();
    }
    leave_child();
  }
}
