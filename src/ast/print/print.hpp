
#pragma once
#include "../../tokens/token_types.hpp"
#include "../../nodes/node_visitor.hpp"
#include "../../nodes/node_expr.hpp"
#include <iostream>
#include <string>

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
  size_t indent = 0;
// In print.hpp
  void push();
  void pop();
  void line(const std::string& text);

  void print_token(const Token&);
  std::string escape_json_string(const std::string& s);
  std::string poison_mark(bool is_poisoned);
};

