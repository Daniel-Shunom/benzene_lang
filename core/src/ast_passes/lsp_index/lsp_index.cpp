#include <ether/ast_passes/lsp_index/lsp_index.hpp>

#include <ether/ast_passes/type_check/type_check.hpp>

#include <ether/symbols/symbol_types.hpp>
#include <ether/types/type_printer.hpp>

#include <utility>
#include <variant>

namespace {

auto kind_to_string(SymbolKind kind) -> std::string {
  switch (kind) {
    case SymbolKind::Function:   return "Function";
    case SymbolKind::FuncParam:  return "FuncParam";
    case SymbolKind::Binding:    return "Binding";
    case SymbolKind::Constant:   return "Constant";
    case SymbolKind::Module:     return "Module";
    case SymbolKind::Type:       return "Type";
    case SymbolKind::UnResolved: return "UnResolved";
  }
  return "UnResolved";
}

}  // namespace

auto LspIndexer::render(const TypePtr& type) const -> std::string {
  if (!type) {
    return {};
  }
  return TypePrinter{false, substitutions}.print(type);
}

auto LspIndexer::resolve(const TypePtr& type) const -> TypePtr {
  TypePtr current = type;
  if (!substitutions) {
    return current;
  }

  // Bounded rather than `while`: a substitution map built from a failed
  // unification can contain a cycle, and this runs on every keystroke.
  for (int hops = 0; current && hops < 64; ++hops) {
    const auto* variable = std::get_if<TypeVar>(&current->value);
    if (!variable) {
      break;
    }
    auto found = substitutions->find(variable->get_id());
    if (found == substitutions->end()) {
      break;
    }
    current = found->second;
  }
  return current;
}

auto LspIndexer::function_return(const TypePtr& type) const -> TypePtr {
  TypePtr resolved = resolve(type);
  if (!resolved) {
    return nullptr;
  }

  if (const auto* function = std::get_if<FunctionType>(&resolved->value)) {
    return function->get_return_type();
  }

  if (const auto* constructor = std::get_if<TypeConstructor>(&resolved->value)) {
    if (constructor->name() == "Fn" && !constructor->get_args().empty()) {
      return constructor->get_args().back();
    }
  }

  return nullptr;
}

auto LspIndexer::span_length(const Token& token) -> size_t {
  // The lexer stores a string literal's contents without its quotes, but the
  // editor highlights and selects the quotes too.
  switch (token.token_type) {
    case TokenType::StringLiteral:   return token.token_value.size() + 2;
    case TokenType::UTStringLiteral: return token.token_value.size() + 1;
    default:                         return token.token_value.size();
  }
}

auto LspIndexer::known_type(const SymbolAttr* symbol,
                            const std::string& name) const -> TypePtr {
  if (!checker) {
    return nullptr;
  }

  // What the symbol was bound to. This is the answer for an occurrence whose
  // own node was left open by unification.
  if (symbol) {
    const auto& bindings = checker->types().all_bindings();
    // The map is keyed by non-const pointer; the lookup does not mutate it.
    auto found = bindings.find(const_cast<SymbolAttr*>(symbol));
    if (found != bindings.end() && is_solved(found->second.type)) {
      return found->second.type;
    }
  }

  // A constructor builds its declared type, which is the useful thing to say
  // about `Wrap` in `Wrap(1)`.
  if (auto parent = checker->constructor_parent(name)) {
    return parent;
  }

  // An alias stands for its target. Reported as the target so that hovering
  // `Count` says `Int`, while anything *annotated* `Count` keeps saying
  // `Count` -- that is what the user wrote, and what the checker carries.
  if (auto target = checker->type_aliases().lookup(name)) {
    return target;
  }

  return nullptr;
}

auto LspIndexer::is_solved(const TypePtr& type) const -> bool {
  TypePtr resolved = resolve(type);
  return resolved && !std::holds_alternative<TypeVar>(resolved->value);
}

auto LspIndexer::effective_return(const NDFuncDeclExpr& decl) const -> TypePtr {
  // What the user wrote always wins.
  if (decl.return_type && decl.return_type->parsed_type) {
    return decl.return_type->parsed_type;
  }

  if (auto solved = function_return(decl.inferred_type); is_solved(solved)) {
    return solved;
  }

  // Unification left the return open. A function's value is its body's last
  // expression, so that node is the better answer when it has one -- this
  // reads a type off the tree rather than deriving one, and only ever fills a
  // hole the checker left, so it cannot contradict it.
  if (!decl.func_body.empty()) {
    const auto& tail = decl.func_body.back();
    if (tail && is_solved(tail->inferred_type)) {
      return resolve(tail->inferred_type);
    }
  }

  return function_return(decl.inferred_type);
}

auto LspIndexer::signature(const Token& name,
                           const std::vector<NDFuncParam>& params,
                           const TypePtr& returns) const -> std::string {
  std::string text = name.token_value + "(";

  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0) {
      text += ", ";
    }
    text += params[i].identifier.identifier.token_value;

    // Prefer what the user wrote; fall back to what was inferred for it.
    const TypePtr& param_type =
      params[i].param_type && params[i].param_type->parsed_type
        ? params[i].param_type->parsed_type
        : params[i].inferred_type;
    if (auto rendered = render(param_type); !rendered.empty()) {
      text += ": " + rendered;
    }
  }

  text += ")";

  if (auto rendered = render(returns); !rendered.empty()) {
    text += " :> " + rendered;
  }
  return text;
}

auto LspIndexer::describe_type(const NDTypeDecl& decl) const -> std::string {
  const std::string& name = decl.type_identifier.token_value;

  if (decl.alias_target) {
    return name + " = " + render(decl.alias_target->parsed_type);
  }

  if (!decl.sub_types) {
    // `type Foo` with no body: a declaration and nothing more.
    return name;
  }

  std::string text = name + " {";
  for (size_t i = 0; i < decl.sub_types->size(); ++i) {
    text += i ? ", " : " ";
    text += render((*decl.sub_types)[i].parsed_type);
  }
  text += decl.sub_types->empty() ? "}" : " }";
  return text;
}

void LspIndexer::record(const Token& token, const SymbolAttr* symbol,
                        const TypePtr& type, bool force_definition,
                        bool annotated, std::string detail,
                        std::string kind_override, std::string returns) {
  IndexEntry entry;
  entry.name      = token.token_value;
  entry.line      = token.line_number;
  entry.column    = token.column_number;
  entry.length    = span_length(token);
  entry.type      = render(type);

  // The node's own type is the first answer, but unification can leave an
  // occurrence open even where the checker knows the symbol perfectly well.
  //
  // A module is the exception: an import names a module, not a value. The
  // checker deliberately does not type one, and nothing here should invent a
  // type by looking the path up in a table it has no business matching.
  if (entry.kind != "Module" && !is_solved(type)) {
    if (auto better = known_type(symbol, entry.name)) {
      entry.type = render(better);
    }
  }
  entry.annotated = annotated;
  entry.detail    = std::move(detail);
  entry.returns   = std::move(returns);

  if (!scopes.empty() && scopes.back() != &token) {
    entry.scope_line   = scopes.back()->line_number;
    entry.scope_column = scopes.back()->column_number;
  }

  if (symbol) {
    entry.kind       = kind_to_string(symbol->symbol_kind);
    entry.def_line   = symbol->symbol_token.line_number;
    entry.def_column = symbol->symbol_token.column_number;
    // An occurrence sitting exactly on its own declaration token is the
    // declaration. The resolver points every use at the same SymbolAttr, so
    // this comparison is what separates "definition" from "reference".
    entry.is_definition =
      entry.def_line == entry.line && entry.def_column == entry.column;
  } else {
    entry.kind = "UnResolved";
  }

  if (!kind_override.empty()) {
    entry.kind = std::move(kind_override);
  }

  if (force_definition) {
    entry.is_definition = true;
    if (entry.def_line == 0) {
      entry.def_line   = entry.line;
      entry.def_column = entry.column;
    }
  }

  collected.push_back(std::move(entry));
}

void LspIndexer::visit(NDIdentifier& expr) {
  record(expr.identifier, expr.identifier_symbol, expr.inferred_type, false,
         expr.type.has_value() && expr.type->parsed_type != nullptr);
  if (expr.type) {
    expr.type->accept(*this);
  }
}

void LspIndexer::visit(NDLiteral& expr) {
  // Recorded as an occurrence, never a declaration. A literal has a type worth
  // showing on hover, but marking it a definition would put an inlay hint after
  // every number in the file and list each one in the outline.
  record(expr.literal, nullptr, expr.inferred_type, false, false, {}, "Literal");
}

void LspIndexer::visit(NDImportDirective& expr) {
  // No symbol is created for an import -- the resolver only scope-checks it --
  // but the token is unambiguously a module path, and saying "unresolved" would
  // tell the editor the compiler failed at something it never attempted.
  record(expr.import_directive, nullptr, nullptr, true, false, {}, "Module");
}

void LspIndexer::visit(NDTypeExpr&) {}

void LspIndexer::visit(NDTypeDecl& expr) {
  // The resolver declares a symbol for this name, but discards the pointer --
  // `NDTypeDecl` has nowhere to keep it -- so the kind has to be stated here
  // rather than read back off the node. Without this every declared type looks
  // unresolved to the editor, even though the checker resolves it fine.
  record(expr.type_identifier, nullptr, expr.inferred_type, true, false,
         describe_type(expr), "Type");
  if (expr.alias_target) {
    expr.alias_target->accept(*this);
  }
  if (expr.sub_types) {
    for (auto& sub : *expr.sub_types) {
      sub.accept(*this);
    }
  }
}

void LspIndexer::visit(NDLetBindExpr& expr) {
  if (expr.identifier) {
    expr.identifier->accept(*this);
  }
  if (expr.bound_value) {
    expr.bound_value->accept(*this);
  }
}

void LspIndexer::visit(NDConstExpr& expr) {
  if (expr.identifier) {
    expr.identifier->accept(*this);
  }
  if (expr.bound_value) {
    expr.bound_value->accept(*this);
  }
}

void LspIndexer::visit(NDCallExpr& expr) {
  if (expr.identifier) {
    expr.identifier->accept(*this);
  }
  for (auto& arg : expr.args) {
    if (arg) {
      arg->accept(*this);
    }
  }
}

void LspIndexer::visit(NDCallChain& expr) {
  for (auto& call : expr.calls) {
    if (call) {
      call->accept(*this);
    }
  }
}

void LspIndexer::visit(NDFuncParam& expr) {
  // Recorded directly rather than by descending into `identifier`: the param
  // symbol lives on the NDFuncParam, and a parameter name is always its own
  // declaration site.
  record(expr.identifier.identifier, expr.param_sym, expr.inferred_type, true,
         expr.param_type.has_value() && expr.param_type->parsed_type != nullptr);
}

void LspIndexer::visit(NDFuncDeclExpr& expr) {
  TypePtr returns = effective_return(expr);
  record(expr.func_identifier, expr.func_sym, expr.inferred_type, true,
         expr.return_type.has_value() && expr.return_type->parsed_type != nullptr,
         signature(expr.func_identifier, expr.func_params, returns), {},
         render(returns));

  // Pushed before the params and body so everything inside is attributed to
  // this function, and popped after so siblings are not.
  scopes.push_back(&expr.func_identifier);
  for (auto& param : expr.func_params) {
    param.accept(*this);
  }
  for (auto& node : expr.func_body) {
    if (node) {
      node->accept(*this);
    }
  }
  scopes.pop_back();
}

void LspIndexer::visit(NDLambdaExpr& expr) {
  // A lambda has no name token to scope by, so its contents stay attributed to
  // whatever function encloses the lambda itself.
  for (auto& param : expr.func_params) {
    param.accept(*this);
  }
  for (auto& node : expr.func_body) {
    if (node) {
      node->accept(*this);
    }
  }
}

void LspIndexer::visit(NDCaseExpr& expr) {
  for (auto& condition : expr.conditions) {
    if (condition) {
      condition->accept(*this);
    }
  }
  for (auto& branch : expr.branches) {
    for (auto& pattern : branch.pattern) {
      if (pattern) {
        pattern->accept(*this);
      }
    }
    if (branch.result) {
      branch.result->accept(*this);
    }
  }
}

void LspIndexer::visit(NDBinaryExpr& expr) {
  if (expr.lhs) {
    expr.lhs->accept(*this);
  }
  if (expr.rhs) {
    expr.rhs->accept(*this);
  }
}

void LspIndexer::visit(NDUnaryExpr& expr) {
  if (expr.rhs) {
    expr.rhs->accept(*this);
  }
}

void LspIndexer::visit(NDScopeExpr& expr) {
  for (auto& node : expr.expressions) {
    if (node) {
      node->accept(*this);
    }
  }
}

void LspIndexer::visit(NDListExpr& expr) {
  for (auto& value : expr.values) {
    if (value) {
      value->accept(*this);
    }
  }
}

void LspIndexer::visit(NDTupleExpr& expr) {
  for (auto& value : expr.values) {
    if (value) {
      value->accept(*this);
    }
  }
}
