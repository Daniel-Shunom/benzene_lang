#pragma once
#include <ether/tokens/token_types.hpp>
#include <ether/nodes/node_visitor.hpp>
#include <ether/nodes/node_expr.hpp>
#include <iostream>
#include <string>
#include <vector>

class TreePrinter final : public Visitor {
public:
  explicit TreePrinter(std::ostream& out = std::cout)
    : out(out) {}

  void visit(NDLiteral& expr)           override;
  void visit(NDImportDirective& expr)   override;
  void visit(NDIdentifier& expr)        override;
  void visit(NDLetBindExpr& expr)       override;
  void visit(NDConstExpr& expr)         override;
  void visit(NDCallExpr& expr)          override;
  void visit(NDCallChain& expr)         override;
  void visit(NDFuncDeclExpr& expr)      override;
  void visit(NDCaseExpr& expr)          override;
  void visit(NDBinaryExpr& expr)        override;
  void visit(NDUnaryExpr& expr)         override;
  void visit(NDScopeExpr& expr)         override;
  void visit(NDListExpr& expr)          override;
  void visit(NDTupleExpr& expr)         override;
  void visit(NDLambdaExpr& expr)        override;
  void visit(NDFuncParam& expr)         override;

private:
  std::ostream& out;

  // For each ancestor depth, true if this branch was the last child of its
  // parent. Drives the rendering of `│  ` vs `   ` trail segments and the
  // `├──` vs `└──` connector at the current depth.
  std::vector<bool> last_stack;

  [[nodiscard]] auto prefix() const -> std::string;
  [[nodiscard]] auto connector() const -> std::string;
  void emit_line(const std::string& content);

  void enter_child(bool is_last) { last_stack.push_back(is_last); }
  void leave_child() { last_stack.pop_back(); }

  // Print a labeled subtree: "├── label" then the child as its only sub-node.
  void child_field(const std::string& label, Node& child, bool is_last);

  // Print a leaf field: "├── label: value".
  void leaf_field(const std::string& label, const std::string& value, bool is_last);

  auto type_header(const std::string& type_name, bool is_poisoned) -> std::string;
};
