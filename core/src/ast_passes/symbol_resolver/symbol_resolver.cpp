#include "ether/nodes/node_expr.hpp"
#include <ether/ast_passes/symbol_resolver/symbol_resolver.hpp>
#include <ether/types/type_printer.hpp>
#include <memory>

void SymbolResolver::visit(NDFuncParam& expr) {
  expr.identifier.accept(*this);
  if (expr.param_type) {
    expr.param_type->accept(*this);
  }
}

void SymbolResolver::visit(NDImportDirective& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  // todo: source files from include
  if ( cscope_type && (cscope_type != ScopeType::Module)) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Warn;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.import_directive.column_number;
    diag.location.line = expr.import_directive.line_number;
    diag.message = "Import statements are only allowed at the top of modules";

    this->diag_eng.report(diag);
    return;
  }
}

void SymbolResolver::visit(NDLiteral& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::LambdaExpression
    && cscope_type != ScopeType::CaseExpression
    && cscope_type != ScopeType::Module
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Warn;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.literal.column_number;
    diag.location.line = expr.literal.line_number;
    diag.message = std::format(
      "Literal value `{}` not in allowed scope",
      expr.literal.token_value
    );

    this->diag_eng.report(diag);
    return;
  }
}

void SymbolResolver::visit(NDIdentifier& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::LambdaExpression
    && cscope_type != ScopeType::CaseExpression
    && cscope_type != ScopeType::Module
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier.column_number;
    diag.location.line = expr.identifier.line_number;
    diag.message = std::format(
      "Identifier `{}` not in allowed scope",
      expr.identifier.token_value
    );

    this->diag_eng.report(diag);
    return;
  }

  auto *ident_sym = this->sym_table.lookup(expr.identifier.token_value);
  if (!ident_sym) {
    expr.is_poisoned = true;
    Diagnostic diagnostic;
    diagnostic.level = DiagnosticLevel::Fail;
    diagnostic.phase = DiagnosticPhase::Resolver;
    diagnostic.location = {.line = expr.identifier.line_number, .column = expr.identifier.column_number};
    diagnostic.message = std::format("Identifier `{}` is not defined", expr.identifier.token_value);
    diag_eng.report(std::move(diagnostic));
    return;
  }
  expr.identifier_symbol = ident_sym;
}

void SymbolResolver::visit(NDLetBindExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::LambdaExpression
    && cscope_type != ScopeType::CaseExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = "`Let` expression is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  auto expr_sym = this->sym_table.declare(expr.identifier->identifier, SymbolKind::Binding);
  if (!expr_sym) {
    expr.is_poisoned = true;
    auto ident_sym = this->sym_table.lookup(expr.identifier->identifier.token_value);
    if (!ident_sym) return;

    auto dup_msg = std::format(
      "Duplicate declaration of `{}` (see Ln {}, Col {} for previous declaration)",
      expr.identifier->identifier.token_value,
      ident_sym->symbol_token.line_number,
      ident_sym->symbol_token.column_number
    );
    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = dup_msg;

    this->diag_eng.report(diag);
    return;
  }

  expr_sym->symbol_kind = SymbolKind::Binding;
  expr.identifier->identifier_symbol = expr_sym;
  if (expr.identifier->type) expr.identifier->type->accept(*this);

  BindingData binding_data;

  if (expr.identifier->type) {
    binding_data.binding_type.type_name = TypePrinter{}.print(expr.identifier->type->parsed_type);
  }

  expr.bound_value->accept(*this);
  return;
}

void SymbolResolver::visit(NDConstExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();

  if (
    cscope_type
    && cscope_type != ScopeType::Module
    && cscope_type != ScopeType::Application
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = "`Const` expression is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  auto const_sym = this->sym_table.declare(expr.identifier->identifier, SymbolKind::Constant);
  if (!const_sym) {
    expr.is_poisoned = true;
    auto ident_sym = this->sym_table.lookup(expr.identifier->identifier.token_value);
    if (!ident_sym) return;

    auto dup_msg = std::format(
      "Duplicate `const` declaration of `{}` (see Ln {}, Col {} for previous declaration)",
      expr.identifier->identifier.token_value,
      ident_sym->symbol_token.line_number,
      ident_sym->symbol_token.column_number
    );

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = dup_msg;

    this->diag_eng.report(diag);
    return;
  }
  const_sym->symbol_kind = SymbolKind::Constant;
  expr.identifier->identifier_symbol = const_sym;
  if (expr.identifier->type) expr.identifier->type->accept(*this);

  if (cscope_type == ScopeType::Module) {
    this->exports.emplace(const_sym->name, const_sym);
  }

  expr.bound_value->accept(*this);
  return;
}


void SymbolResolver::visit(NDCallExpr& expr) {
  auto ident  = expr.identifier->identifier.token_value;
  auto sym = this->sym_table.lookup(ident);

  if (!sym) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = std::format(
      "Function `{}` is not defined",
      ident
    );

    this->diag_eng.report(diag);
    return;
  }

  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::CaseExpression
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::LambdaExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.identifier->identifier.column_number;
    diag.location.line = expr.identifier->identifier.line_number;
    diag.message = "Function call is not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  expr.identifier->identifier_symbol = sym;

  for (auto& arg: expr.args) {
    arg->accept(*this);
  }

  return;
}

void SymbolResolver::visit(NDCallChain& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::CaseExpression
    && cscope_type != ScopeType::LambdaExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.line = expr.start_token.line_number;
    diag.location.column = expr.start_token.column_number;
    diag.message = "Call chain not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  for (auto& call: expr.calls) call->accept(*this);
}

void SymbolResolver::visit(NDFuncDeclExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (cscope_type && cscope_type != ScopeType::Module) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.func_identifier.column_number;
    diag.location.line = expr.func_identifier.line_number;
    diag.message = "Function declaration not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  auto func_sym = this->sym_table.declare(expr.func_identifier, SymbolKind::Function);
  if (!func_sym) {
    // Means this is a redeclaration of another function.
    expr.is_poisoned = true;
    auto ident_sym = this->sym_table.lookup(expr.func_identifier.token_value);
    if (!ident_sym) return;

    auto dup_msg = std::format(
      "Duplicate function declaration of `{}` (see Ln {}, Col {} for previous declaration)",
      expr.func_identifier.token_value,
      ident_sym->symbol_token.line_number,
      ident_sym->symbol_token.column_number
    );

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = expr.func_identifier.column_number;
    diag.location.line = expr.func_identifier.line_number;
    diag.message = dup_msg;

    this->diag_eng.report(diag);
    return;
  }

  if (cscope_type == ScopeType::Module) {
    this->exports.emplace(func_sym->name, func_sym);
  }

  ScopeGuard guard(this->sym_table, ScopeType::FunctionDeclaration);

  for (auto& arg: expr.func_params) {
    auto ptr = sym_table.declare(arg.identifier.identifier, SymbolKind::FuncParam);
    if (!ptr) {
      auto lkp = sym_table.lookup(arg.identifier.identifier.token_value);
      if (!lkp) continue;
      auto dup_msg = std::format(
        "Duplicate function parameter name `{}` (see Ln {}, Col {} for previous declaration)",
        arg.identifier.identifier.token_value,
        lkp->symbol_token.line_number,
        lkp->symbol_token.column_number
      );

      auto diag = Diagnostic();
      diag.level = DiagnosticLevel::Fail;
      diag.phase = DiagnosticPhase::Resolver;
      diag.location.column = arg.identifier.identifier.column_number;
      diag.location.line = arg.identifier.identifier.line_number;
      diag.message = dup_msg;

      this->diag_eng.report(diag);
    };

    if (!arg.param_sym) {
      arg.param_sym = ptr;
    }
    if (arg.param_type) arg.param_type->accept(*this);
  }

  if (expr.return_type) expr.return_type->accept(*this);

  for (auto& body_expr: expr.func_body) {
    body_expr->accept(*this);
  }

  FunctionData func_data;

  if (expr.return_type) {
    func_data.function_return_type.type_name = TypePrinter{}.print(expr.return_type->parsed_type);
  }

  func_sym->symbol_data = func_data;
  expr.func_sym = func_sym;
  return;
}

void SymbolResolver::visit(NDTypeDecl& type_decl) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (cscope_type && cscope_type != ScopeType::Module) {
    type_decl.is_poisoned = true;
    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::ScopeResolution;
    diag.location.column = type_decl.type_identifier.column_number;
    diag.location.line = type_decl.type_identifier.line_number;
    diag.message = "Type declarations should only exist in the top module scope.";

    this->diag_eng.report(diag);
    return;
  }

  auto declare = [&](const Token& token, SymbolKind kind) -> SymbolAttr* {
    auto* symbol = sym_table.declare(token, kind);
    if (!symbol) {
      type_decl.is_poisoned = true;
      Diagnostic diagnostic;
      diagnostic.level = DiagnosticLevel::Fail;
      diagnostic.phase = DiagnosticPhase::Resolver;
      diagnostic.location = {.line = token.line_number, .column = token.column_number};
      diagnostic.message = std::format("Duplicate type declaration or parameter `{}`", token.token_value);
      diag_eng.report(std::move(diagnostic));
    }
    return symbol;
  };
  // Make the declared type and each of its constructors visible to later
  // case-pattern resolution. Type expressions retain their full structure;
  // this only creates the symbol records.
  type_decl.type_symbol = declare(type_decl.type_identifier, SymbolKind::Type);
  if (!type_decl.type_symbol) return;

  if (type_decl.sub_types) {
    for (auto& member : *type_decl.sub_types) {
      if (member.parsed_type && member.parsed_type->isTypeConstructor()) {
        const auto& constructor = std::get<TypeConstructor>(member.parsed_type->value);
        Token token{
          .token_type = TokenType::Identifier,
          .token_value = constructor.name(),
          .line_number = type_decl.type_identifier.line_number,
          .column_number = type_decl.type_identifier.column_number
        };
        if (!member.names.empty()) token = member.names.front().token;
        if (!declare(token, SymbolKind::Type)) continue;
        constructor_arities[constructor.name()] = constructor.get_args().size();
      }
    }
  }

  ScopeGuard guard(sym_table, ScopeType::TypeDeclaration);
  for (auto& parameter : type_decl.params) {
    auto* identifier = dynamic_cast<NDIdentifier*>(parameter.get());
    if (!identifier) continue;
    identifier->identifier_symbol = declare(identifier->identifier, SymbolKind::TypeParam);
  }
  if (type_decl.alias_target) type_decl.alias_target->accept(*this);
  if (type_decl.sub_types) {
    for (auto& member : *type_decl.sub_types) member.accept(*this);
  }
}

void SymbolResolver::visit(NDLambdaExpr& lambda) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::LambdaExpression
    && cscope_type != ScopeType::CaseExpression
  ) {
    lambda.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.column = lambda.lambda_start.column_number;
    diag.location.line = lambda.lambda_start.line_number;
    diag.message = "Function declaration not in valid scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::FunctionDeclaration);

  for (auto& arg: lambda.func_params) {
    auto ptr = sym_table.declare(arg.identifier.identifier, SymbolKind::FuncParam);
    if (!ptr) {
      auto lkp = sym_table.lookup(arg.identifier.identifier.token_value);
      if (!lkp) continue;
      auto dup_msg = std::format(
        "Duplicate function parameter name `{}` (see Ln {}, Col {} for previous declaration)",
        arg.identifier.identifier.token_value,
        lkp->symbol_token.line_number,
        lkp->symbol_token.column_number
      );

      auto diag = Diagnostic();
      diag.level = DiagnosticLevel::Fail;
      diag.phase = DiagnosticPhase::Resolver;
      diag.location.column = arg.identifier.identifier.column_number;
      diag.location.line = arg.identifier.identifier.line_number;
      diag.message = dup_msg;

      this->diag_eng.report(diag);
    };

    if (!arg.param_sym) arg.param_sym = ptr;
  }

  for (auto& body_expr: lambda.func_body) body_expr->accept(*this);
  return;
}

void SymbolResolver::visit(NDScopeExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::LambdaExpression
    && cscope_type != ScopeType::CaseExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.line = expr.open_brace.line_number;
    diag.location.column = expr.open_brace.column_number;
    diag.message = "Scoped expression is not allowed in current scope";

    this->diag_eng.report(diag);
    return;
  }

  ScopeGuard guard(this->sym_table, ScopeType::ScopedExpression);
  for (auto& scope_expr: expr.expressions) scope_expr->accept(*this);
}

void SymbolResolver::visit(NDListExpr& expr) {
  for (auto& value: expr.values) value->accept(*this);
}

void SymbolResolver::visit(NDTupleExpr& expr) {
  for (auto& value: expr.values) value->accept(*this);
}

void SymbolResolver::visit(NDCaseExpr& expr) {
  auto cscope_type = this->sym_table.get_current_scope_type();
  if (
    cscope_type
    && cscope_type != ScopeType::FunctionDeclaration
    && cscope_type != ScopeType::ScopedExpression
    && cscope_type != ScopeType::LambdaExpression
    && cscope_type != ScopeType::CaseExpression
  ) {
    expr.is_poisoned = true;

    auto diag = Diagnostic();
    diag.level = DiagnosticLevel::Fail;
    diag.phase = DiagnosticPhase::Resolver;
    diag.location.line = expr.case_keyword.line_number;
    diag.location.column = expr.case_keyword.column_number;
    diag.message = "Case expression not allowed in current scope";

    this->diag_eng.report(diag);
    return;
  }

  for (auto& condition: expr.conditions) condition->accept(*this);
  for (auto& [case_cond, ret_expr]: expr.branches) {
    ScopeGuard guard(this->sym_table, ScopeType::CaseExpression);
    for (auto& conds: case_cond) resolve_pattern(*conds);
    ret_expr->accept(*this);
  }
}

void SymbolResolver::resolve_pattern(Node& pattern) {
  auto fail = [&](const Token& token, std::string message) {
    pattern.is_poisoned = true;
    Diagnostic diagnostic;
    diagnostic.level = DiagnosticLevel::Fail;
    diagnostic.phase = DiagnosticPhase::Resolver;
    diagnostic.location = {.line = token.line_number, .column = token.column_number};
    diagnostic.message = std::move(message);
    diag_eng.report(std::move(diagnostic));
  };
  if (auto* identifier = dynamic_cast<NDIdentifier*>(&pattern)) {
    const auto& token = identifier->identifier;
    if (token.token_value == "_") {
      identifier->is_wildcard_pattern = true;
      return;
    }
    auto* symbol = sym_table.lookup(token.token_value);
    if (symbol && symbol->symbol_kind == SymbolKind::Type
        && constructor_arities.contains(token.token_value)) {
      if (constructor_arities.at(token.token_value) != 0) {
        fail(token, std::format("Constructor `{}` requires field patterns", token.token_value));
      } else identifier->identifier_symbol = symbol;
      return;
    }
    if (symbol && symbol->symbol_kind == SymbolKind::Constant) {
      identifier->identifier_symbol = symbol;
      return;
    }
    identifier->identifier_symbol = sym_table.declare(token, SymbolKind::Binding);
    if (!identifier->identifier_symbol) {
      fail(token, std::format("Duplicate pattern binding `{}`", token.token_value));
    }
    return;
  }
  if (auto* call = dynamic_cast<NDCallExpr*>(&pattern)) {
    const auto& token = call->identifier->identifier;
    auto* symbol = sym_table.lookup(token.token_value);
    auto arity = constructor_arities.find(token.token_value);
    if (!symbol) {
      fail(token, std::format("Function `{}` is not defined", token.token_value));
      return;
    }
    if (!symbol || symbol->symbol_kind != SymbolKind::Type || arity == constructor_arities.end()) {
      fail(token, std::format("`{}` is not a pattern constructor", token.token_value));
      return;
    }
    call->identifier->identifier_symbol = symbol;
    if (arity->second != call->args.size()) {
      fail(token, std::format("Constructor `{}` expects {} fields, but got {}",
                            token.token_value, arity->second, call->args.size()));
      return;
    }
    for (auto& argument : call->args) resolve_pattern(*argument);
    return;
  }
  if (auto* list = dynamic_cast<NDListExpr*>(&pattern)) {
    for (auto& value : list->values) resolve_pattern(*value);
    return;
  }
  if (auto* tuple = dynamic_cast<NDTupleExpr*>(&pattern)) {
    for (auto& value : tuple->values) resolve_pattern(*value);
    return;
  }
  pattern.accept(*this);
}

void SymbolResolver::visit(NDBinaryExpr& expr) {
  expr.lhs->accept(*this);
  expr.rhs->accept(*this);
}

void SymbolResolver::visit(NDUnaryExpr& expr) {
  expr.rhs->accept(*this);
}

void SymbolResolver::visit(NDTypeExpr& expr) {
  for (auto& name : expr.names) {
    name.symbol = sym_table.lookup(name.token.token_value);
    const auto& value = name.token.token_value;
    const bool builtin = value == "Int" || value == "Float" || value == "String"
        || value == "Bool" || value == "List" || value == "Tuple"
        || value == "Set" || value == "Dict" || value == "Option" || value == "Result";
    const bool invalid_parameter = name.symbol
        && name.symbol->symbol_kind == SymbolKind::TypeParam && name.applied;
    const bool unknown = !builtin && (!name.symbol
        || (name.symbol->symbol_kind != SymbolKind::Type
            && name.symbol->symbol_kind != SymbolKind::TypeParam));
    if (!invalid_parameter && (!unknown
        || sym_table.get_current_scope_type() != ScopeType::TypeDeclaration)) continue;
    expr.is_poisoned = true;
    Diagnostic diagnostic;
    diagnostic.level = DiagnosticLevel::Fail;
    diagnostic.phase = DiagnosticPhase::Resolver;
    diagnostic.location = {.line = name.token.line_number, .column = name.token.column_number};
    diagnostic.message = invalid_parameter
        ? std::format("Type parameter `{}` cannot take type arguments", value)
        : std::format("Type `{}` is not defined; generic parameters must be declared by the parent type", value);
    diag_eng.report(std::move(diagnostic));
  }
}
