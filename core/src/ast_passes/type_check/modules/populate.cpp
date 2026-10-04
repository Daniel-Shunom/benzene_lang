#include "ether/ast_passes/type_check/modules/populate.hpp"
#include "ether/nodes/node_expr.hpp"
#include "ether/tokens/token_types.hpp"
#include "ether/types/types.hpp"
#include <algorithm>
#include <ether/ast_passes/type_check/type_check.hpp>
#include <ranges>

void TCModule_Populate::visit(NDImportDirective& expr) { }

void TCModule_Populate::visit(NDTypeDecl& expr) {
  if (expr.alias_target) {
    expr.alias_target->accept(*this);
  }
  if (expr.sub_types) {
    for (auto& member : *expr.sub_types) {
      member.accept(*this);
    }
  }
}

void TCModule_Populate::visit(NDFuncParam& expr) {
  if (expr.param_type) {
    expr.param_type->accept(*this);
    expr.inferred_type = expr.param_type.value().inferred_type;
    expr.identifier.inferred_type = expr.param_type.value().inferred_type;
  } else {
    expr.inferred_type = context.varFactory();
    expr.identifier.inferred_type = expr.inferred_type;
  }
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
  } else {
    expr.inferred_type = context.varFactory();
  }
}

void TCModule_Populate::visit(NDLetBindExpr& expr) {
  // Todo. Allow type annoations into node
  expr.identifier->accept(*this);
  expr.bound_value->accept(*this);
  expr.inferred_type = expr.identifier->inferred_type;
}

void TCModule_Populate::visit(NDConstExpr& expr) {
  expr.identifier->accept(*this);
  expr.bound_value->accept(*this);
  expr.inferred_type = expr.identifier->inferred_type;
}

void TCModule_Populate::visit(NDCallExpr& expr) {
  expr.inferred_type =  context.varFactory();
  expr.identifier->accept(*this);
  for (auto& arg: expr.args) {
    arg->accept(*this);
  }
}

void TCModule_Populate::visit(NDCallChain& expr) {
  expr.inferred_type = context.varFactory();
  for (auto& call: expr.calls) {
    call->accept(*this);
  }
}

void TCModule_Populate::visit(NDFuncDeclExpr& expr) {
  auto param_types = expr.func_params
    | std::views::transform([&](NDFuncParam& param) -> TypePtr {
      param.accept(*this);
      return param.inferred_type;
    })
    | std::ranges::to<std::vector<TypePtr>>();

  if (expr.return_type) {
    expr.return_type->accept(*this);
    expr.inferred_type = makeFunc(param_types, expr.return_type.value().inferred_type);
    for (auto& exp: expr.func_body) {
      exp->accept(*this);
    }
  } else {
    if (expr.func_body.empty()) {
      expr.inferred_type = makeFunc(param_types, context.varFactory());
      return;
    }

    auto& last_expr = expr.func_body.back();
    last_expr->accept(*this);
    expr.inferred_type = makeFunc(param_types, last_expr->inferred_type);

    for (auto& exp: expr.func_body
      | std::views::reverse
      | std::views::drop(1)
      | std::views::reverse
    ) {
      exp->accept(*this);
    }
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
  auto param_types = expr.func_params
    | std::views::transform([&](NDFuncParam& param) -> TypePtr {
      param.accept(*this);
      return param.inferred_type;
    })
    | std::ranges::to<std::vector<TypePtr>>();

  if (expr.return_type) {
    expr.return_type->accept(*this);
    expr.inferred_type = makeFunc(param_types, expr.return_type.value().inferred_type);
    for (auto& exp: expr.func_body) {
      exp->accept(*this);
    }
  } else {
    if (expr.func_body.empty()) {
      expr.inferred_type = makeFunc(param_types, context.varFactory());
      return;
    }

    auto& last_expr = expr.func_body.back();
    last_expr->accept(*this);
    expr.inferred_type = makeFunc(param_types, last_expr->inferred_type);

    for (auto& exp: expr.func_body
      | std::views::reverse
      | std::views::drop(1)
      | std::views::reverse
    ) {
      exp->accept(*this);
    }
  }
}

void TCModule_Populate::visit(NDTypeExpr& expr) {
  // Type-expression semantics are not implemented in this pass yet.
  expr.inferred_type = expr.parsed_type;
}
