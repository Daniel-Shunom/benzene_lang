#include "ether/nodes/node_expr.hpp"
#include <ether/ast_passes/print/print.hpp>
#include <ether/types/type_printer.hpp>

void TreePrinter::visit(NDFuncParam& expr) {
  expr.identifier.accept(*this);
  if (expr.param_type) {
    child_field("annotation", *expr.param_type, true);
  }
}

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

auto TreePrinter::prefix() const -> std::string {
  std::string p;
  for (size_t i = 0; i + 1 < last_stack.size(); ++i) {
    p += last_stack[i]
      ? std::string("    ")
      : std::string(DIM) + BLUE + "│   " + RESET;
  }
  return p;
}

auto TreePrinter::connector() const -> std::string {
  if (last_stack.empty()) {
    return "";
  }
  return std::string(DIM) + BLUE
       + (last_stack.back() ? "└── " : "├── ")
       + RESET;
}

void TreePrinter::emit_line(const std::string& content) {
  out << prefix() << connector() << content << '\n';
}

auto TreePrinter::type_header(const std::string& type_name, Node& node) -> std::string {
  std::string s = std::string(BOLD) + CYAN + type_name + RESET;
  if (show_types && node.inferred_type) {
    s += " " + std::string(DIM) + "[type: " +
         TypePrinter{true, substitutions}.print(node.inferred_type) + "]" + RESET;
  }
  if (node.is_poisoned) {
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
  emit_line(type_header("Literal", n));
  leaf_field("value", n.literal.token_value, true);
}

void TreePrinter::visit(NDImportDirective& n) {
  emit_line(type_header("ImportDirective", n));
  leaf_field("module", n.import_directive.token_value, true);
}

void TreePrinter::visit(NDIdentifier& n) {
  emit_line(type_header("Identifier", n));
  leaf_field("name", n.identifier.token_value, !n.type);
  if (n.type) {
    child_field("annotation", *n.type, true);
  }
}

void TreePrinter::visit(NDUnaryExpr& n) {
  emit_line(type_header("UnaryExpr", n));
  if (n.op) {
    leaf_field("op", n.op->token_value, false);
  }
  child_field("rhs", *n.rhs, true);
}

void TreePrinter::visit(NDBinaryExpr& n) {
  emit_line(type_header("BinaryExpr", n));
  child_field("lhs", *n.lhs, false);
  leaf_field("op", n.op.token_value, false);
  child_field("rhs", *n.rhs, true);
}

void TreePrinter::visit(NDScopeExpr& n) {
  emit_line(type_header("ScopeExpr", n));
  for (size_t i = 0; i < n.expressions.size(); ++i) {
    bool last = (i + 1 == n.expressions.size());
    enter_child(last);
    n.expressions[i]->accept(*this);
    leave_child();
  }
}

void TreePrinter::visit(NDListExpr& n) {
  emit_line(type_header("ListExpr", n));
  for (size_t i = 0; i < n.values.size(); i++) {
    bool last = (i + 1 == n.values.size());
    enter_child(last);
    n.values[i]->accept(*this);
    leave_child();
  }
}

void TreePrinter::visit(NDTupleExpr& n) {
  emit_line(type_header("TupleExpr", n));
  for (size_t i = 0; i < n.values.size(); i++) {
    bool last = (i + 1 == n.values.size());
    enter_child(last);
    n.values[i]->accept(*this);
    leave_child();
  }
}

void TreePrinter::visit(NDLetBindExpr& n) {
  emit_line(type_header("LetBindExpr", n));
  child_field("identifier", *n.identifier, false);
  child_field("value", *n.bound_value, true);
}

void TreePrinter::visit(NDConstExpr& n) {
  emit_line(type_header("ConstExpr", n));
  child_field("identifier", *n.identifier, false);
  child_field("value", *n.bound_value, true);
}

void TreePrinter::visit(NDCallExpr& n) {
  emit_line(type_header("CallExpr", n));
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
  emit_line(type_header("CallChain", n));
  for (size_t i = 0; i < n.calls.size(); ++i) {
    bool last = (i + 1 == n.calls.size());
    enter_child(last);
    n.calls[i]->accept(*this);
    leave_child();
  }
}

void TreePrinter::visit(NDFuncDeclExpr& n) {
  emit_line(type_header("FuncDecl", n));
  bool has_return = n.return_type.has_value();
  bool has_params = !n.func_params.empty();
  bool has_body = !n.func_body.empty();

  leaf_field("name", n.func_identifier.token_value, !has_return && !has_params && !has_body);

  if (has_return) {
    const auto& type = n.return_type->parsed_type
      ? n.return_type->parsed_type
      : n.return_type->inferred_type;
    leaf_field("return_type", TypePrinter{true, substitutions}.print(type), !has_params && !has_body);
  }

  if (has_params) {
    enter_child(!has_body);
    emit_line(std::string(DIM) + "params" + RESET);
    for (size_t i = 0; i < n.func_params.size(); ++i) {
      const auto& p = n.func_params[i];
      bool last = (i + 1 == n.func_params.size());
      std::string text =
        std::string(GREEN) + p.identifier.identifier.token_value + RESET;
      if (p.param_type) {
        text += std::string(DIM) + " : " + RESET +
                TypePrinter{true, substitutions}.print(p.param_type->parsed_type);
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

void TreePrinter::visit(NDTypeDecl& type_decl) {
  emit_line(type_header("TypeDecl", type_decl));
  leaf_field("type", type_decl.type_identifier.token_value, !type_decl.alias_target && !type_decl.sub_types);
  if (type_decl.alias_target) {
    child_field("alias_target", *type_decl.alias_target, !type_decl.sub_types);
  }
  if (type_decl.sub_types) {
    enter_child(true);
    emit_line(std::string(DIM) + "subtypes" + RESET);
    for (size_t i = 0; i < type_decl.sub_types->size(); ++i) {
      enter_child(i + 1 == type_decl.sub_types->size());
      (*type_decl.sub_types)[i].accept(*this);
      leave_child();
    }
    leave_child();
  }
}

void TreePrinter::visit(NDLambdaExpr& n) {
  emit_line(type_header("LambdaExpr", n));
  bool has_return = n.return_type.has_value();
  bool has_params = !n.func_params.empty();
  bool has_body = !n.func_body.empty();

  if (has_return) {
    const auto& type = n.return_type->parsed_type
      ? n.return_type->parsed_type
      : n.return_type->inferred_type;
    leaf_field("return_type", TypePrinter{true, substitutions}.print(type), !has_params && !has_body);
  }

  if (has_params) {
    enter_child(!has_body);
    emit_line(std::string(DIM) + "params" + RESET);
    for (size_t i = 0; i < n.func_params.size(); ++i) {
      const auto& p = n.func_params[i];
      bool last = (i + 1 == n.func_params.size());
      std::string text =
        std::string(GREEN) + p.identifier.identifier.token_value + RESET;
      if (p.param_type) {
        text += std::string(DIM) + " : " + RESET +
                TypePrinter{true, substitutions}.print(p.param_type->parsed_type);
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
  emit_line(type_header("CaseExpr", n));
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

void TreePrinter::visit(NDTypeExpr& expr) {
  emit_line(type_header("TypeExpr", expr));
  leaf_field("type", TypePrinter{true, substitutions}.print(expr.parsed_type), true);
}
