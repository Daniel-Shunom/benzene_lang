#include "ether/ast_passes/type_check/modules/populate.hpp"
#include "ether/nodes/node_expr.hpp"
#include "ether/tokens/token_types.hpp"
#include "ether/types/types.hpp"
#include <algorithm>
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ranges>
#include <functional>

void TCModule_Populate::visit(NDImportDirective& expr) { }

void TCModule_Populate::visit(NDTypeDecl& expr) {
  if (expr.is_poisoned) return;
  std::unordered_map<std::string, TypePtr> parameters;
  std::vector<TypePtr> parent_args;
  std::vector<TypeVarId> quantified;
  for (auto& parameter : expr.params) {
    auto* identifier = dynamic_cast<NDIdentifier*>(parameter.get());
    if (!identifier) continue;
    auto variable = context.varFactory();
    parameters.emplace(identifier->identifier.token_value, variable);
    parent_args.push_back(variable);
    quantified.push_back(std::get<TypeVar>(variable->value).get_id());
    identifier->inferred_type = variable;
    if (identifier->identifier_symbol) {
      context.types().bind(identifier->identifier_symbol, Scheme{{}, variable});
    }
  }
  std::function<TypePtr(TypePtr)> resolve = [&](TypePtr type) -> TypePtr {
    if (!type) return nullptr;
    if (type->isTypeConstructor()) {
      const auto& constructor = std::get<TypeConstructor>(type->value);
      if (constructor.is_field_label()) return resolve(constructor.get_args().front());
      if (auto it = parameters.find(constructor.name()); it != parameters.end()) return it->second;
      std::vector<TypePtr> args;
      for (const auto& arg : constructor.get_args()) args.push_back(resolve(arg));
      return context.resolve_alias(makeTypeConstructor(constructor.name(), args),
          {.line = expr.type_identifier.line_number, .column = expr.type_identifier.column_number});
    }
    if (type->isFunctionType()) {
      const auto& function = std::get<FunctionType>(type->value);
      std::vector<TypePtr> args;
      for (const auto& arg : function.get_param_types()) args.push_back(resolve(arg));
      return makeFunc(std::move(args), resolve(function.get_return_type()));
    }
    if (type->isPmtType()) {
      const auto& product = std::get<PmtType>(type->value);
      std::vector<TypeField> fields;
      for (const auto& field : product.get_fields()) fields.emplace_back(field.name(), resolve(field.get_type()));
      return std::make_shared<Type>(PmtType{product.get_name(), std::move(fields)});
    }
    return type;
  };
  const auto parent = makeTypeConstructor(expr.type_identifier.token_value, parent_args);
  expr.inferred_type = parent;
  if (expr.alias_target) {
    if (expr.alias_target->is_poisoned) return;
    expr.alias_target->inferred_type = resolve(expr.alias_target->parsed_type);
    expr.inferred_type = expr.alias_target->inferred_type;
    context.register_type_alias(expr.type_identifier.token_value,
                                expr.alias_target->inferred_type, quantified);
  }
  if (expr.sub_types) {
    for (auto& member : *expr.sub_types) {
      if (member.is_poisoned) continue;
      if (member.parsed_type && member.parsed_type->isTypeConstructor()) {
        const auto& constructor = std::get<TypeConstructor>(member.parsed_type->value);
        std::vector<TypePtr> fields;
        for (const auto& arg : constructor.get_args()) fields.push_back(resolve(arg));
        member.inferred_type = makeTypeConstructor(constructor.name(), fields);
        context.register_constructor_type(constructor.name(), parent,
                                           member.inferred_type, quantified);
      }
    }
  }
}

void TCModule_Populate::visit(NDFuncParam& expr) {
  TypePtr type;
  if (expr.param_type) {
    expr.param_type->accept(*this);
    type = expr.param_type.value().inferred_type;
  } else {
    type = context.varFactory();
  }

  expr.inferred_type = type;
  expr.identifier.inferred_type = type;
  context.types().bind(expr.param_sym, Scheme{{}, type});
  if (expr.param_sym && expr.param_type) expr.param_sym->declared_type = expr.param_type->parsed_type;
}

void TCModule_Populate::visit(NDLiteral& expr) {
  switch (expr.literal.token_type) {
    case TokenType::FloatLiteral:
      expr.inferred_type = makeFloat();
    break;

    case TokenType::IntegerLiteral:
      expr.inferred_type = makeInt();
    break;

    case TokenType::StringLiteral:
      expr.inferred_type = makeString();
    break;

    case TokenType::TrueLiteral:
    case TokenType::FalseLiteral:
      expr.inferred_type = makeBool();
    break;

    case TokenType::NilLiteral:
      expr.inferred_type = makeNil();
    break;

    default: expr.inferred_type = context.varFactory();
  }
}

void TCModule_Populate::visit(NDIdentifier& expr) {
  if (expr.is_wildcard_pattern) {
    expr.inferred_type = context.varFactory();
    return;
  }
  if (expr.identifier_symbol && expr.identifier_symbol->symbol_kind == SymbolKind::Type) {
    if (auto instance = context.instantiate_constructor(expr.identifier.token_value)) {
      const auto& function = std::get<FunctionType>(instance->value);
      if (function.get_param_types().empty()) {
        expr.inferred_type = function.get_return_type();
        return;
      }
    }
  }
  if (expr.type) {
    expr.type->accept(*this);
    expr.inferred_type = expr.type->inferred_type;
    if (expr.identifier_symbol) expr.identifier_symbol->declared_type = expr.type->parsed_type;
    context.types().bind(expr.identifier_symbol, Scheme{{}, expr.inferred_type});
  } else if (auto* scheme = context.types().lookup(expr.identifier_symbol)) {
    expr.inferred_type = context.instantiate(*scheme);
  } else {
    expr.inferred_type = context.varFactory();
    context.types().bind(expr.identifier_symbol, Scheme{{}, expr.inferred_type});
  }
}

void TCModule_Populate::visit(NDLetBindExpr& expr) {
  expr.bound_value->accept(*this);

  TypePtr binding_type;
  if (expr.identifier->type) {
    expr.identifier->accept(*this);
    binding_type = expr.identifier->inferred_type;
  } else {
    binding_type = context.varFactory();
    expr.identifier->inferred_type = binding_type;
  }

  context.types().bind(
    expr.identifier->identifier_symbol,
    Scheme{{}, binding_type}
  );
  expr.inferred_type = expr.identifier->inferred_type;
}

void TCModule_Populate::visit(NDConstExpr& expr) {
  expr.bound_value->accept(*this);

  TypePtr binding_type;
  if (expr.identifier->type) {
    expr.identifier->accept(*this);
    binding_type = expr.identifier->inferred_type;
  } else {
    binding_type = context.varFactory();
    expr.identifier->inferred_type = binding_type;
  }

  context.types().bind(
    expr.identifier->identifier_symbol,
    Scheme{{}, binding_type}
  );
  expr.inferred_type = expr.identifier->inferred_type;
}

void TCModule_Populate::visit(NDCallExpr& expr) {
  expr.inferred_type =  context.varFactory();
  expr.identifier->accept(*this);
  for (auto& arg: expr.args) {
    arg->accept(*this);
  }
  if (expr.identifier->identifier_symbol
      && expr.identifier->identifier_symbol->symbol_kind == SymbolKind::Type) {
    if (auto instance = context.instantiate_constructor(expr.identifier->identifier.token_value)) {
      expr.identifier->inferred_type = instance;
      const auto& function = std::get<FunctionType>(instance->value);
      expr.inferred_type = function.get_return_type();
      // The enclosing constructor pattern has the declared parent type. Its
      // positional arguments inherit the constructor's field types.
      for (size_t i = 0; i < expr.args.size() && i < function.get_param_types().size(); ++i) {
        auto* identifier = dynamic_cast<NDIdentifier*>(expr.args[i].get());
        // Only a pattern binder inherits the field type directly. An ordinary
        // argument keeps its own type and is checked against the field below.
        if (identifier && identifier->identifier_symbol
            && identifier->identifier_symbol->symbol_kind == SymbolKind::Binding
            && identifier->identifier_symbol->symbol_token.line_number == identifier->identifier.line_number
            && identifier->identifier_symbol->symbol_token.column_number == identifier->identifier.column_number) {
          identifier->inferred_type = function.get_param_types()[i];
          if (identifier->identifier_symbol) {
            context.types().bind(identifier->identifier_symbol, Scheme{{}, identifier->inferred_type});
          }
        }
      }
    }
  }
}

void TCModule_Populate::visit(NDCallChain& expr) {
  expr.inferred_type = context.varFactory();
  for (auto& call: expr.calls) {
    call->accept(*this);
  }
}

void TCModule_Populate::visit(NDFuncDeclExpr& expr) {
  context.types().push_scope();

  auto param_types = expr.func_params
    | std::views::transform([&](NDFuncParam& param) -> TypePtr {
      param.accept(*this);
      return param.inferred_type;
    })
    | std::ranges::to<std::vector<TypePtr>>();

  TypePtr return_type;
  if (expr.return_type) {
    expr.return_type->accept(*this);
    return_type = expr.return_type->inferred_type;
  } else {
    return_type = context.varFactory();
  }

  if (!expr.return_type) {
    NDTypeExpr rtn_type_expr;
    rtn_type_expr.inferred_type = return_type;
    expr.return_type = std::move(rtn_type_expr);
  }

  expr.inferred_type = makeFunc(param_types, return_type);
  if (expr.func_sym) {
    std::vector<TypePtr> declared_params;
    bool annotated = expr.return_type && expr.return_type->parsed_type;
    for (const auto& parameter : expr.func_params) {
      if (parameter.param_type && parameter.param_type->parsed_type) {
        annotated = true;
        declared_params.push_back(parameter.param_type->parsed_type);
      } else {
        declared_params.push_back(parameter.inferred_type);
      }
    }
    if (annotated) {
      expr.func_sym->declared_type = makeFunc(std::move(declared_params),
          expr.return_type->parsed_type ? expr.return_type->parsed_type : return_type);
    }
    context.types().bind(expr.func_sym, Scheme{{}, expr.inferred_type});
  }

  for (auto& exp : expr.func_body) {
    exp->accept(*this);
  }

  context.types().pop_scope();
  if (expr.func_sym) {
    context.types().bind(expr.func_sym, Scheme{{}, expr.inferred_type});
  }
}
void TCModule_Populate::visit(NDCaseExpr& expr) {
  expr.inferred_type = context.varFactory();
  for (auto& condition: expr.conditions) {
    condition->accept(*this);
  }

  if (!expr.is_poisoned) {
    for (auto& branch : expr.branches) {
      context.push_type_scope();
      std::ranges::for_each(branch.pattern, [&](auto& ptn) -> void {
        ptn->accept(*this);
      });
      branch.result->accept(*this);
      context.pop_type_scope();
    }
  }
}

void TCModule_Populate::visit(NDBinaryExpr& expr) {
  expr.inferred_type = context.varFactory();
  expr.lhs->accept(*this);
  expr.rhs->accept(*this);
}

void TCModule_Populate::visit(NDUnaryExpr& expr) {
  expr.inferred_type = context.varFactory();
  expr.rhs->accept(*this);
}

void TCModule_Populate::visit(NDScopeExpr& expr) {
  context.types().push_scope();

  if (!expr.expressions.empty()) {
    auto& last_expr = expr.expressions.back();
    last_expr->accept(*this);

    expr.inferred_type = last_expr->inferred_type;

    std::ranges::for_each(
      std::views::reverse(expr.expressions)
      | std::views::drop(1)
      | std::views::reverse,
      [&] (auto& exp) -> void { exp->accept(*this); }
    );
  } else {
    expr.inferred_type = makeNil();
  }

  context.types().pop_scope();
}

void TCModule_Populate::visit(NDTupleExpr& expr) {
  for (auto& exp: expr.values) {
    exp->accept(*this);
  }

  expr.inferred_type = makeTuple(
    expr.values
    | std::views::transform([&] (const auto& val) -> TypePtr { return val->inferred_type; })
    | std::ranges::to<std::vector<TypePtr>>()
  );
}

void TCModule_Populate::visit(NDListExpr& expr) {
  if (!expr.values.empty()) {
    auto& first_val = expr.values.front();
    first_val->accept(*this);

    expr.inferred_type = makeList(first_val->inferred_type);

    for (auto& val: expr.values | std::views::drop(1)) {
      val->accept(*this);
    }

  } else {
    expr.inferred_type = makeList(context.varFactory());
  }
}

void TCModule_Populate::visit(NDLambdaExpr& expr) {
  context.types().push_scope();

  auto param_types = expr.func_params
    | std::views::transform([&](NDFuncParam& param) -> TypePtr {
      param.accept(*this);
      return param.inferred_type;
    })
    | std::ranges::to<std::vector<TypePtr>>();

  TypePtr return_type;
  if (expr.return_type) {
    expr.return_type->accept(*this);
    return_type = expr.return_type->inferred_type;
  } else {
    return_type = context.varFactory();
  }

  if (!expr.return_type) {
    NDTypeExpr rtn_type_expr;
    rtn_type_expr.inferred_type = return_type;
    expr.return_type = std::move(rtn_type_expr);
  }

  expr.inferred_type = makeFunc(param_types, return_type);
  for (auto& exp : expr.func_body) {
    exp->accept(*this);
  }

  context.types().pop_scope();
}

void TCModule_Populate::visit(NDTypeExpr& expr) {
  // Type-expression semantics are not implemented in this pass yet.
  if (expr.parsed_type) {
    SourceLocation location{};
    if (!expr.names.empty()) {
      location = {.line = expr.names.front().token.line_number,
                  .column = expr.names.front().token.column_number};
    }
    expr.inferred_type = context.resolve_alias(expr.parsed_type, location);
  }
}
