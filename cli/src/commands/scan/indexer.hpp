#pragma once
#include <ether/nodes/node_expr.hpp>
#include <ether/nodes/node_visitor.hpp>
#include <ether/types/types.hpp>

#include <cstddef>
#include <string>
#include <vector>

// One addressable point in the source that the editor can ask about: an
// identifier occurrence, or the name of a declaration. Positions are 1-based
// line / 1-based column, matching the lexer's Token.
struct IndexEntry {
  std::string name;
  std::string kind;        // Function, FuncParam, Binding, Constant, Type, ...
  size_t      line   = 0;
  size_t      column = 0;
  size_t      length = 0;

  // Rendered inferred type, substitutions applied. Empty when the node never
  // got a type (parse error, poisoned branch, or a pass that does not type it).
  std::string type;

  // Where the symbol this occurrence refers to was declared. Zero when the
  // resolver could not bind it, which is also how the LSP decides that
  // go-to-definition has nothing to offer.
  size_t def_line   = 0;
  size_t def_column = 0;

  // True when this entry *is* the declaration site. Drives documentSymbol.
  bool is_definition = false;
};

// Walks the typed AST and collects every identifier occurrence and declaration
// name. Run this *after* the type checker has unified, and hand it the
// resulting substitution map so rendered types are solved rather than raw type
// variables.
class LspIndexer final : public Visitor {
public:
  explicit LspIndexer(const Subst* substitutions = nullptr)
    : substitutions(substitutions) {}

  [[nodiscard]] auto entries() const -> const std::vector<IndexEntry>& {
    return collected;
  }

  void visit(NDLiteral& expr)         override;
  void visit(NDImportDirective& expr) override;
  void visit(NDIdentifier& expr)      override;
  void visit(NDLetBindExpr& expr)     override;
  void visit(NDConstExpr& expr)       override;
  void visit(NDCallExpr& expr)        override;
  void visit(NDCallChain& expr)       override;
  void visit(NDTypeDecl& expr)        override;
  void visit(NDTypeExpr& expr)        override;
  void visit(NDFuncDeclExpr& expr)    override;
  void visit(NDCaseExpr& expr)        override;
  void visit(NDBinaryExpr& expr)      override;
  void visit(NDUnaryExpr& expr)       override;
  void visit(NDScopeExpr& expr)       override;
  void visit(NDListExpr& expr)        override;
  void visit(NDTupleExpr& expr)       override;
  void visit(NDLambdaExpr& expr)      override;
  void visit(NDFuncParam& expr)       override;

private:
  const Subst* substitutions;
  std::vector<IndexEntry> collected;

  [[nodiscard]] auto render(const TypePtr&) const -> std::string;

  // Records `token` as an occurrence. `symbol` may be null, in which case the
  // entry carries no definition site.
  void record(const Token& token, const SymbolAttr* symbol, const TypePtr& type,
              bool force_definition = false);
};
