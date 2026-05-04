#pragma once
#include "../../tokens/token_types.hpp"
#include "../../nodes/node_visitor.hpp"
#include "../../nodes/node_expr.hpp"
#include <iostream>
#include <string>
#include <vector>

class TreePrinter final : public Visitor {
public:
  explicit TreePrinter(std::ostream& out = std::cout)
    : out(out) {}

  void visit(NDLiteral&) override;
  void visit(NDImportDirective&) override;
  void visit(NDIdentifier&) override;
  void visit(NDLetBindExpr&) override;
  void visit(NDConstExpr&) override;
  void visit(NDCallExpr&) override;
  void visit(NDCallChain&) override;
  void visit(NDFuncDeclExpr&) override;
  void visit(NDCaseExpr&) override;
  void visit(NDBinaryExpr&) override;
  void visit(NDUnaryExpr&) override;
  void visit(NDScopeExpr&) override;

private:
  std::ostream& out;

  // For each ancestor depth, true if this branch was the last child of its
  // parent. Drives the rendering of `│  ` vs `   ` trail segments and the
  // `├──` vs `└──` connector at the current depth.
  std::vector<bool> last_stack;

  std::string prefix() const;
  std::string connector() const;
  void emit_line(const std::string& content);

  void enter_child(bool is_last) { last_stack.push_back(is_last); }
  void leave_child() { last_stack.pop_back(); }

  // Print a labeled subtree: "├── label" then the child as its only sub-node.
  void child_field(const std::string& label, Node& child, bool is_last);

  // Print a leaf field: "├── label: value".
  void leaf_field(const std::string& label, const std::string& value, bool is_last);

  std::string type_header(const std::string& type_name, bool is_poisoned);
};
