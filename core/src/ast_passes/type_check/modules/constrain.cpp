#include "ether/ast_passes/type_check/modules/constrain.hpp"
#include "ether/ast_passes/type_check/type_check.hpp"
#include "ether/types/types.hpp"

void TCModule_Constrain::visit(NDLiteral& expr) {
  Constraint constraint({
    .lhs=expr.inferred_type,
    .rhs=expr.inferred_type
  });
  this->constraints.push_back(constraint);
}

void TCModule_Constrain::visit(NDImportDirective& expr) {
  // Do nothing?
}

void TCModule_Constrain::visit(NDIdentifier& expr) {
  // Population has already resolved this occurrence to either a monomorphic
  // type or a fresh instantiation of its scheme. Population runs before
  // constraint generation, however, so a let scheme may have become
  // polymorphic since that pass. Instantiate it at the use site here.
  if (auto* scheme = context.types().lookup(expr.identifier_symbol);
      scheme && !scheme->quantified.empty()) {
    expr.inferred_type = context.instantiate(*scheme);
  }
}

void TCModule_Constrain::visit(NDLetBindExpr& expr) {
  expr.bound_value->accept(*this);
  Constraint constraint({
    .lhs=expr.identifier->inferred_type,
    .rhs=expr.bound_value->inferred_type
  });
  this->constraints.push_back(constraint);
  context.generalize_binding(expr);
}

void TCModule_Constrain::visit(NDConstExpr& expr) {
  expr.bound_value->accept(*this);
  Constraint constraint({
    .lhs=expr.identifier->inferred_type,
    .rhs=expr.bound_value->inferred_type
  });
  this->constraints.push_back(constraint);
}

void TCModule_Constrain::visit(NDCallExpr& expr) {
  expr.identifier->accept(*this);

  std::vector<TypePtr> call_args;
  for (auto& arg: expr.args) {
    arg->accept(*this);
    call_args.push_back(arg->inferred_type);
  }

  if (expr.identifier->identifier_symbol
      && expr.identifier->identifier_symbol->symbol_kind == SymbolKind::Type) {
    return;
  }

  Constraint constraint({
    .lhs = expr.identifier->inferred_type,
    .rhs = makeFunc(std::move(call_args), expr.inferred_type)
  });

  this->constraints.push_back(constraint);
}

void TCModule_Constrain::visit(NDCallChain& expr) {
  for (auto& call: expr.calls) {
    call->accept(*this);
  }
}

void TCModule_Constrain::visit(NDTypeDecl& expr) {
  if (expr.alias_target) {
    expr.alias_target->accept(*this);
  }
  if (expr.sub_types) {
    for (auto& member : *expr.sub_types) {
      member.accept(*this);
    }
  }
}

void TCModule_Constrain::visit(NDFuncDeclExpr& expr) {
  context.push_type_scope();
  std::vector<TypePtr> param_types;
  for (auto& param : expr.func_params) {
    param.accept(*this);
    context.activate_type_symbol(param.param_sym);
    param_types.push_back(param.inferred_type);
  }

  for (auto& body_expr : expr.func_body) {
    body_expr->accept(*this);
  }

  if (expr.return_type && !expr.func_body.empty()) {
    constraints.push_back({
      .lhs = expr.return_type->inferred_type,
      .rhs = expr.func_body.back()->inferred_type
    });
  }

  constraints.push_back({
    .lhs = expr.inferred_type,
    .rhs = makeFunc(param_types, expr.return_type.value().inferred_type)
  });
  context.pop_type_scope();
}

void TCModule_Constrain::visit(NDCaseExpr& expr) {
  if (expr.is_poisoned) return;
  for (auto& condition : expr.conditions) {
    condition->accept(*this);
  }

  for (auto& branch : expr.branches) {
    for (size_t i = 0; i < branch.pattern.size(); ++i) {
      branch.pattern[i]->accept(*this);

      if (i < expr.conditions.size()) {
        constraints.push_back({
          .lhs = expr.conditions[i]->inferred_type,
          .rhs = branch.pattern[i]->inferred_type
        });
      }
    }

    branch.result->accept(*this);
    constraints.push_back({
      .lhs = expr.inferred_type,
      .rhs = branch.result->inferred_type
    });
  }
}

void TCModule_Constrain::visit(NDBinaryExpr& expr) {
  expr.lhs->accept(*this);
  expr.rhs->accept(*this);

  const auto add_constraint = [this](TypePtr lhs, TypePtr rhs) -> void {
    constraints.push_back({.lhs = std::move(lhs), .rhs = std::move(rhs)});
  };

  using Op = TokenType;
  switch (expr.op.token_type) {
    case Op::AndOp:
    case Op::OrOp:
      add_constraint(expr.lhs->inferred_type, makeBool());
      add_constraint(expr.rhs->inferred_type, makeBool());
      add_constraint(expr.inferred_type, makeBool());
      break;

    case Op::Gt:
    case Op::Ge:
    case Op::Lt:
    case Op::Le:
    case Op::Eq:
    case Op::NtEq:
    case Op::EqEq:
      add_constraint(expr.lhs->inferred_type, expr.rhs->inferred_type);
      add_constraint(expr.inferred_type, makeBool());
      break;

    case Op::PlusOp:
    case Op::MinusOp:
    case Op::MultiplyOp:
    case Op::DivideOp:
    case Op::PercentOp:
      add_constraint(expr.lhs->inferred_type, expr.rhs->inferred_type);
      add_constraint(expr.inferred_type, expr.lhs->inferred_type);
      break;

    default:
      add_constraint(expr.inferred_type, expr.lhs->inferred_type);
      break;
  }
}

void TCModule_Constrain::visit(NDUnaryExpr& expr) {
  expr.rhs->accept(*this);

  if (!expr.op) {
    constraints.push_back({
      .lhs = expr.inferred_type,
      .rhs = expr.rhs->inferred_type
    });
    return;
  }

  if (expr.op->token_type == TokenType::NotOp) {
    constraints.push_back({.lhs = expr.rhs->inferred_type, .rhs = makeBool()});
    constraints.push_back({.lhs = expr.inferred_type, .rhs = makeBool()});
  } else {
    constraints.push_back({
      .lhs = expr.inferred_type,
      .rhs = expr.rhs->inferred_type
    });
  }
}

void TCModule_Constrain::visit(NDScopeExpr& expr) {
  context.push_type_scope();
  for (auto& expr: expr.expressions) {
    expr->accept(*this);
  }

  if (!expr.expressions.empty()) {
    auto& last_expr = expr.expressions.back();
    constraints.push_back({
      .lhs = expr.inferred_type,
      .rhs = last_expr->inferred_type
    });
  }
  context.pop_type_scope();
}

void TCModule_Constrain::visit(NDTupleExpr& expr) {
  std::vector<TypePtr> tuple_vals;
  for (auto& val: expr.values) {
    val->accept(*this);
    tuple_vals.push_back(val->inferred_type);
  }

  this->constraints.push_back({
    .lhs = expr.inferred_type,
    .rhs = makeTuple(std::move(tuple_vals))
  });
}

void TCModule_Constrain::visit(NDListExpr& expr) {
  for (auto& val: expr.values) {
    val->accept(*this);
  }

  if (expr.values.empty()) {
    return;
  }

  const auto* list_type = std::get_if<TypeConstructor>(&expr.inferred_type->value);
  if (!list_type || list_type->get_args().size() != 1) {
    return;
  }

  const auto& element_type = list_type->get_args().front();
  for (auto& val : expr.values) {
    constraints.push_back({
      .lhs = val->inferred_type,
      .rhs = element_type
    });
  }

}

void TCModule_Constrain::visit(NDLambdaExpr& expr) {
  context.push_type_scope();
  std::vector<TypePtr> param_types;
  for (auto& param : expr.func_params) {
    param.accept(*this);
    context.activate_type_symbol(param.param_sym);
    param_types.push_back(param.inferred_type);
  }

  for (auto& body_expr : expr.func_body) {
    body_expr->accept(*this);
  }

  if (expr.return_type && !expr.func_body.empty()) {
    constraints.push_back({
      .lhs = expr.return_type->inferred_type,
      .rhs = expr.func_body.back()->inferred_type
    });
  }

  constraints.push_back({
    .lhs = expr.inferred_type,
    .rhs = makeFunc(param_types, expr.return_type.value().inferred_type)
  });
  context.pop_type_scope();
}

void TCModule_Constrain::visit(NDFuncParam& expr) {
  if (expr.param_type) {
    expr.param_type->accept(*this);
  }
  context.activate_type_symbol(expr.param_sym);
}

void TCModule_Constrain::visit(NDTypeExpr& expr) {
  // Type-expression semantics are not implemented in this pass yet.
  // We're probably gonna do nothing?
}
