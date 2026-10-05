#include <ether/ast_passes/lsp_index/lsp_index.hpp>

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

auto LspIndexer::signature(const Token& name,
                           const std::vector<NDFuncParam>& params,
                           const std::optional<NDTypeExpr>& returns,
                           const TypePtr& inferred) const -> std::string {
  std::string text = name.token_value + "(";

  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0) {
      text += ", ";
    }
    text += params[i].identifier.identifier.token_value;

    // Prefer what the user wrote; fall back to what was inferred for it.
    const TypePtr& param_type = params[i].param_type && params[i].param_type->parsed_type
      ? params[i].param_type->parsed_type
      : params[i].inferred_type;
    if (auto rendered = render(param_type); !rendered.empty()) {
      text += ": " + rendered;
    }
  }

  text += ")";

  // What the user wrote wins; otherwise show what was inferred for the return.
  std::string result = returns && returns->parsed_type
    ? render(returns->parsed_type)
    : render(function_return(inferred));

  if (!result.empty()) {
    text += " :> " + result;
  }
  return text;
}

void LspIndexer::record(const Token& token, const SymbolAttr* symbol,
                        const TypePtr& type, bool force_definition,
                        bool annotated, std::string detail) {
  IndexEntry entry;
  entry.name      = token.token_value;
  entry.line      = token.line_number;
  entry.column    = token.column_number;
  entry.length    = token.token_value.size();
  entry.type      = render(type);
  entry.annotated = annotated;
  entry.detail    = std::move(detail);

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

void LspIndexer::visit(NDLiteral&) {}

void LspIndexer::visit(NDImportDirective& expr) {
  record(expr.import_directive, nullptr, expr.inferred_type);
}

void LspIndexer::visit(NDTypeExpr&) {}

void LspIndexer::visit(NDTypeDecl& expr) {
  record(expr.type_identifier, nullptr, expr.inferred_type, true);
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
  record(expr.func_identifier, expr.func_sym, expr.inferred_type, true,
         expr.return_type.has_value() && expr.return_type->parsed_type != nullptr,
         signature(expr.func_identifier, expr.func_params, expr.return_type,
                   expr.inferred_type));

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
