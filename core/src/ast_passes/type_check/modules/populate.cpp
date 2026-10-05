#include "ether/ast_passes/type_check/modules/populate.hpp"
#include "ether/nodes/node_expr.hpp"
#include "ether/tokens/token_types.hpp"
#include "ether/types/types.hpp"
#include <algorithm>
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ranges>

void TCModule_Populate::visit(NDImportDirective& expr) { }

void TCModule_Populate::visit(NDTypeDecl& expr) {
  const auto parent = makeTypeConstructor(expr.type_identifier.token_value, {});
  if (expr.alias_target) {
    expr.alias_target->accept(*this);
  }
  if (expr.sub_types) {
    for (auto& member : *expr.sub_types) {
      member.accept(*this);
      if (member.parsed_type && member.parsed_type->isTypeConstructor()) {
        const auto& constructor = std::get<TypeConstructor>(member.parsed_type->value);
        context.register_constructor_type(constructor.name(), parent,
                                           makeTypeConstructor(constructor.name(), constructor.get_args()));
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
  if (expr.type) {
    expr.inferred_type = expr.type->parsed_type;
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
    if (auto parent = context.constructor_parent(expr.identifier->identifier.token_value)) {
      expr.inferred_type = parent;
    }
    if (auto fields = context.constructor_type(expr.identifier->identifier.token_value)) {
      // The enclosing constructor pattern has the declared parent type. Its
      // positional arguments inherit the constructor's field types.
      const auto& constructor = std::get<TypeConstructor>(fields->value);
      for (size_t i = 0; i < expr.args.size() && i < constructor.get_args().size(); ++i) {
        auto field = constructor.get_args()[i];
        if (field->isTypeConstructor()) {
          const auto& labelled = std::get<TypeConstructor>(field->value);
          if (labelled.get_args().size() == 1) field = labelled.get_args().front();
        }
        expr.args[i]->inferred_type = field;
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
  for (auto& condition: expr.conditions) {
    condition->accept(*this);
  }

  if (!expr.branches.empty()) {
    auto& fst_branch = expr.branches.front();

    std::ranges::for_each(fst_branch.pattern, [&](auto& ptn) -> void {
      ptn->accept(*this);
    });
    fst_branch.result->accept(*this);

    expr.inferred_type = fst_branch.result->inferred_type;

    for (auto& branch: expr.branches | std::views::drop(1)) {
      std::ranges::for_each(branch.pattern, [&](auto& ptn) -> void {
        ptn->accept(*this);
      });
      branch.result->accept(*this);
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
    expr.inferred_type = expr.parsed_type;
  }
}
