#pragma once
#include <ether/nodes/node_expr.hpp>
#include <ether/nodes/node_visitor.hpp>
#include <ether/types/types.hpp>

#include <cstddef>
#include <optional>
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

  // True when the declaration carried an explicit type annotation. The editor
  // uses this to decide where an inferred type is worth showing inline: an
  // annotation the user already wrote does not need repeating.
  bool annotated = false;

  // Human-readable signature, e.g. `identity(x: Int) :> Int`. Only populated
  // for functions; the solved `type` is what everything else displays.
  std::string detail;

  // A function's return type on its own. Kept apart from `type` so the editor
  // never has to recover it by splitting the rendered `Fn(...) :> R`, which a
  // higher-order parameter makes ambiguous.
  std::string returns;

  // Name token of the function enclosing this entry, or zero at module scope.
  // Lets the editor nest an outline without reconstructing scopes itself.
  size_t scope_line   = 0;
  size_t scope_column = 0;
};

// Walks the typed AST and collects every identifier occurrence and declaration
// name. Run this *after* the type checker has unified, and hand it the
// resulting substitution map so rendered types are solved rather than raw type
// variables.
//
// This is what the language server is built on: `ether scan` serialises the
// result, and the editor answers hover, go-to-definition, rename, completion
// and inlay hints out of it without re-deriving anything.
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

  // Name tokens of the functions currently being walked, innermost last.
  // Functions nest, so this is a stack rather than a single current scope.
  std::vector<const Token*> scopes;

  [[nodiscard]] auto render(const TypePtr&) const -> std::string;

  // Follows a type variable through the substitution map to whatever it was
  // solved to. Returns the input unchanged when it is not a variable.
  [[nodiscard]] auto resolve(const TypePtr&) const -> TypePtr;

  // The return type of a function type, in either representation the model
  // uses: a `FunctionType`, or a `Fn` constructor whose last argument is the
  // return. Null when the type is not a function.
  [[nodiscard]] auto function_return(const TypePtr&) const -> TypePtr;

  // Whether a type is anything more definite than an unsolved variable.
  [[nodiscard]] auto is_solved(const TypePtr&) const -> bool;

  // The best answer available for what a function returns.
  [[nodiscard]] auto effective_return(const NDFuncDeclExpr&) const -> TypePtr;

  // Renders `func(a: Int, b: Int) :> Int` from a declaration's own syntax,
  // falling back to the solved type for anything left unannotated.
  [[nodiscard]] auto signature(const Token& name,
                               const std::vector<NDFuncParam>& params,
                               const TypePtr& returns) const -> std::string;

  // Renders `Alias = Int` or `Data { Integer, Custom(Int) }` from a type
  // declaration's own syntax, so hovering one says what it actually declares.
  [[nodiscard]] auto describe_type(const NDTypeDecl&) const -> std::string;

  // Records `token` as an occurrence. `symbol` may be null, in which case the
  // entry carries no definition site.
  //
  // `kind_override` names the kind for declarations the resolver binds but does
  // not hand back: a type declaration gets a symbol, yet `NDTypeDecl` has no
  // field to hold the pointer, so the node cannot say what it is.
  void record(const Token& token, const SymbolAttr* symbol, const TypePtr& type,
              bool force_definition = false, bool annotated = false,
              std::string detail = {}, std::string kind_override = {},
              std::string returns = {});
};
