#include "ether/parser/types.hpp"
#include "ether/nodes/node_expr.hpp"
#include "ether/tables/rsv_type_table.hpp"
#include "ether/tokens/token_types.hpp"
#include <ether/parser/parsers.hpp>
#include <ether/types/types.hpp>
#include <optional>

using std::nullopt;

namespace {
void report_type_error(ParserState& state, const std::string& message) {
  Diagnostic diagnostic{};
  diagnostic.level = DiagnosticLevel::Fail;
  diagnostic.phase = DiagnosticPhase::Parser;
  if (auto token = state.peek()) {
    diagnostic.location = {.line=token->line_number, .column=token->column_number};
  } else if (!state.tokens.empty()) {
    const auto& last = state.tokens.back();
    diagnostic.location = {.line=last.line_number, .column=last.column_number + last.token_value.size()};
  } else {
    diagnostic.location = {.line=1, .column=1};
  }
  diagnostic.message = message;
  state.diag_eng.report(std::move(diagnostic));
}

auto require_type_token(ParserState& state, TokenType token, const std::string& message) -> bool {
  if (match(token)(state)) {
    return true;
  }
  report_type_error(state, message);
  return false;
}

auto require_type_expression(ParserState& state, const std::string& message) -> PResult<NDTypeExpr> {
  const auto diagnostics_before = state.diag_eng.all().size();
  auto result = parse_type_expression()(state);
  if (!result && state.diag_eng.all().size() == diagnostics_before) {
    report_type_error(state, message);
  }
  return result;
}
} // namespace

auto is_allowed_type(std::string type) -> bool {
  return ReservedTypes.contains(type);
}

auto parse_type_declaration() -> Parser<NDTypeDecl> {
  static Parser<NDTypeDecl> parser = [](ParserState& state) -> PResult<NDTypeDecl> {
    ParseCheckpoint checkpoint(state);

    auto type_tok = match(TokenType::TypeKeyword)(state);
    if (!type_tok) {
      return std::nullopt;
    }

    auto type_ident = match(TokenType::Identifier)(state);
    if (!type_ident) {
      report_type_error(state, "Expected a type name after 'type'");
      return std::nullopt;
    }

    NDTypeDecl type_decl;

    std::vector<TypePtr> parameter_types;
    if (match(TokenType::LParen)(state)) {
      while(!match(TokenType::RParen)(state)) {
        auto token = match(TokenType::Identifier)(state);
        if (!token) {
          report_type_error(state, "Expected a type parameter name or ')'");
          return std::nullopt;
        }

        auto ident = std::make_unique<NDIdentifier>();
        ident->identifier = token.value();
        parameter_types.push_back(makeTypeConstructor(token->token_value, {}));
        type_decl.params.push_back(std::move(ident));

        if (match(TokenType::RParen)(state)) {
          break;
        }

        if (!require_type_token(
          state,
          TokenType::Delim,
          "Expected ',' or ')' after type parameter")
        ) {
          return std::nullopt;
        }
      }
    }

    if (match(TokenType::Eq)(state)) {
      auto target = require_type_expression(state, "Expected a type expression after '='");
      if (!target) {
        return std::nullopt;
      }
      type_decl.alias_target = std::move(*target);
      type_decl.type_identifier = type_ident.value();
      checkpoint.commit();
      return type_decl;
    }

    if (match(TokenType::LBrace)(state)) {
      type_decl.sub_types.emplace();
      while (true) {
        auto next = state.peek();
        if (next && ParserState::is_comment(next->token_type)) {
          state.advance();
          continue;
        }
        if (match(TokenType::RBrace)(state)) {
          break;
        }
        if (!next || next->token_type == TokenType::EoF ||
            next->token_type == TokenType::TypeKeyword ||
            next->token_type == TokenType::FuncStart ||
            next->token_type == TokenType::ConstantKeyword ||
            next->token_type == TokenType::LetKeyword ||
            next->token_type == TokenType::ImportKeyword) {
          report_type_error(state, "Expected '}' to close the type declaration");
          return std::nullopt;
        }
        auto member = require_type_expression(state, "Expected a subtype expression or '}' in type declaration");
        if (!member) {
          return std::nullopt;
        }
        type_decl.sub_types->push_back(std::move(*member));
      }
    }

    auto type = makeTypeConstructor(type_ident->token_value, std::move(parameter_types));
    state.type_collections.push_back(std::move(type));

    type_decl.type_identifier = type_ident.value();

    checkpoint.commit();
    return type_decl;
  };
  return parser;
}

auto parse_type_annotation() -> Parser<NDTypeExpr> {
  static Parser<NDTypeExpr> parser = [](ParserState& state) -> PResult<NDTypeExpr> {
    ParseCheckpoint checkpoint(state);

    auto colon = match(TokenType::Colon)(state);
    if (!colon) {
      return std::nullopt;
    }
    auto type_expr = require_type_expression(state, "Expected a type expression after ':'");
    if (!type_expr) {
      return std::nullopt;
    }

    checkpoint.commit();
    return type_expr;
  };
  return parser;
}

auto parse_type_expression() -> Parser<NDTypeExpr> {
  static Parser<NDTypeExpr> parser = [](ParserState& state) -> PResult<NDTypeExpr> {
    ParseCheckpoint checkpoint(state);

    auto tok = state.peek();
    if (!tok) {
      return std::nullopt;
    }

    NDTypeExpr type_expr;
    const auto start = state.pos;
    if (tok->token_type == TokenType::Identifier) {
      auto texpr = h_parse_type_expr_ident()(state);
      if (!texpr) {
        return std::nullopt;
      }
      type_expr.parsed_type = std::move(texpr.value());
    } else if (tok->token_type == TokenType::LambdaKeyword) {
      auto texpr = h_parse_type_expr_lambda()(state);
      if (!texpr) {
        return std::nullopt;
      }
      type_expr.parsed_type = std::move(texpr.value());
    } else if (tok->token_type == TokenType::NilLiteral) {
      state.advance();
      type_expr.parsed_type = makeNil();
    } else {
      return std::nullopt;
    }

    for (auto i = start; i < state.pos; ++i) {
      const auto& token = state.tokens[i];
      if (token.token_type != TokenType::Identifier) continue;
      if (i + 1 < state.pos && state.tokens[i + 1].token_type == TokenType::Colon) continue;
      type_expr.names.push_back({token, nullptr,
          i + 1 < state.pos && state.tokens[i + 1].token_type == TokenType::LParen});
    }
    checkpoint.commit();
    return type_expr;
  };
  return parser;
}

auto h_parse_type_expr_ident() -> Parser<TypePtr> {
  static Parser<TypePtr> parser = [](ParserState& state) -> PResult<TypePtr> {
    ParseCheckpoint checkpoint(state);
    auto ident = match(TokenType::Identifier)(state);
    if (!ident) {
      return std::nullopt;
    }

    TypePtr type_expr;
    if (match(TokenType::LParen)(state)) {

      // Choose the grammar before parsing so failed alternatives cannot
      // leave diagnostics behind on otherwise valid unlabelled arguments.
      const bool labelled = state.pos + 1 < state.tokens.size() &&
        state.tokens[state.pos].token_type == TokenType::Identifier &&
        state.tokens[state.pos + 1].token_type == TokenType::Colon;
      if (labelled) {
        auto tagged_type_exprs = h_parse_type_expr_ident_wtagged_params(ident->token_value)(state);
        if (!tagged_type_exprs) {
          return std::nullopt;
        }
        type_expr = std::move(tagged_type_exprs.value());
      } else {
        std::vector<TypePtr> arguments;
        while (!match(TokenType::RParen)(state)) {
          auto argument = require_type_expression(state, "Expected a type argument or closing ')'");
          if (!argument) {
            return std::nullopt;
          }
          arguments.push_back(std::move(argument->parsed_type));
          if (match(TokenType::RParen)(state)) {
            break;
          }
          if (!require_type_token(state, TokenType::Delim,
                                  "Expected ',' or ')' after type argument")) {
            return std::nullopt;
          }
        }
        type_expr = makeTypeConstructor(ident->token_value, arguments);
      }

    } else {
      type_expr = makeTypeConstructor(ident.value().token_value, {});
    }


    checkpoint.commit();
    return type_expr;
  };

  return parser;
}

auto h_parse_type_expr_ident_wtagged_params(std::string ident_name) -> Parser<TypePtr> {
  Parser<TypePtr> parser = [ident_name](ParserState& state) -> PResult<TypePtr> {
    ParseCheckpoint checkpoint(state);

    std::vector<TypePtr> types;

    while (!match(TokenType::RParen)(state)) {
      auto ident = match(TokenType::Identifier)(state);
      if (!ident) {
        report_type_error(state, "Expected a field name or ')' in labelled type arguments");
        return std::nullopt;
      }

      auto colon = match(TokenType::Colon)(state);
      if (!colon) {
        report_type_error(state, "Expected ':' after type argument label");
        return std::nullopt;
      }

      auto typ = require_type_expression(state, "Expected a type expression after field label and ':'");
      if (!typ) {
        return std::nullopt;
      }

      auto type_expr = std::move(typ->parsed_type);
      auto tagged_type = std::make_shared<Type>(
          TypeConstructor{ident->token_value, {type_expr}, true});

      types.push_back(std::move(tagged_type));

      if (match(TokenType::RParen)(state)) {
        break;
      }
      if (!require_type_token(state, TokenType::Delim,
                              "Expected ',' or ')' after labelled type argument")) {
        return std::nullopt;
      }
    }

    checkpoint.commit();
    TypePtr type_expr = makeTypeConstructor(ident_name, std::move(types));
    return type_expr;

  };

  return parser;
}

auto h_parse_type_expr_lambda() -> Parser<TypePtr> {
  static Parser<TypePtr> parser = [](ParserState& state) -> PResult<TypePtr> {
    ParseCheckpoint checkpoint(state);
    if (!match(TokenType::LambdaKeyword)(state)) {
      return std::nullopt;
    }

    auto lparen = match(TokenType::LParen)(state);
    if (!lparen) {
      report_type_error(state, "Expected '(' after 'Fn'");
      return std::nullopt;
    }

    std::vector<TypePtr> signature;
    while (!match(TokenType::RParen)(state)) {
      auto parameter = require_type_expression(state, "Expected a parameter type or ')' in function type");
      if (!parameter) {
        return std::nullopt;
      }
      signature.push_back(std::move(parameter->parsed_type));
      if (match(TokenType::RParen)(state)) {
        break;
      }
      if (!require_type_token(state, TokenType::Delim,
                              "Expected ',' or ')' after function parameter type")) {
        return std::nullopt;
      }
    }

    auto ret_sign = match(TokenType::RtnTypeOp)(state);
    if (!ret_sign) {
      report_type_error(state, "Expected ':>' before function return type");
      return std::nullopt;
    }

    auto ret_type_expr = require_type_expression(state, "Expected a return type after ':>'");
    if (!ret_type_expr) {
      return std::nullopt;
    }

    checkpoint.commit();
    return makeFunc(std::move(signature), std::move(ret_type_expr->parsed_type));
  };

  return parser;
}
