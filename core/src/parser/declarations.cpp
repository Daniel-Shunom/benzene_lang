#include <ether/parser/parsers.hpp>

using std::nullopt;

auto parse_import_stmt() -> Parser<NDImportDirective> {
  static Parser<NDImportDirective> parser = [](ParserState& state) -> PResult<NDImportDirective> {
    ParseCheckpoint checkpoint(state);

    auto import_kwd = match(TokenType::ImportKeyword)(state);
    if (!import_kwd) {
      return std::nullopt;
    }

    auto import_module = expect(
      state,
      TokenType::ImportModule,
      ParseErrorType::InvalidImportExpr,
      "Expected a valid module name after import directive"
    );

    if (!import_module) {
      return std::nullopt;
    }

    auto import = NDImportDirective();
    import.import_directive = import_module.value();


    checkpoint.commit();
    return import;
  };
  return parser;
}

auto parse_let_expression() -> Parser<NDLetBindExpr> {
  static Parser<NDLetBindExpr> parser = [](ParserState& state) -> PResult<NDLetBindExpr> {
    ParseCheckpoint checkpoint(state);

    auto let_tok = match(TokenType::LetKeyword)(state);
    if (!let_tok) {
      return std::nullopt;
    }

    auto ident = expect_wp(
      state,
      parse_identifier(),
      ParseErrorType::InvalidLetExpr,
      "Expected an identifier here"
    );

    if (!ident) {
      return std::nullopt;
    }

    std::optional<NDTypeExpr> ident_type;
    if (state.peek() && state.peek()->token_type == TokenType::Colon) {
      ident_type = parse_type_annotation()(state);
      if (!ident_type) {
        return std::nullopt;
      }
    }

    auto equals = expect(
      state,
      TokenType::Eq,
      ParseErrorType::InvalidLetExpr,
      "Expected `=` after identifier"
    );

    if (!equals) {
      return std::nullopt;
    }

    auto value = expect_wp(
      state,
      parse_value_expression(),
      ParseErrorType::InvalidLetExpr,
      "Expected a valid expression"
    );

    if (!value) {
      return std::nullopt;
    }

    NDLetBindExpr expr;
    expr.identifier = std::make_unique<NDIdentifier>(ident.value());
    expr.identifier->type = ident_type;
    expr.bound_value = std::move(value.value());

    checkpoint.commit();
    return expr;
  };
  return parser;
}

auto parse_const_expression() -> Parser<NDConstExpr> {
  static Parser<NDConstExpr> parser = [](ParserState& state) -> PResult<NDConstExpr> {
    ParseCheckpoint checkpoint(state);

    auto let_tok = match(TokenType::ConstantKeyword)(state);
    if (!let_tok) {
      return std::nullopt;
    }

    auto ident = expect_wp(
      state,
      parse_identifier(),
      ParseErrorType::InvalidConstExpr,
      "Expected a valid const identifier"
    );

    if (!ident) {
      return std::nullopt;
    }

    std::optional<NDTypeExpr> const_type;
    if (state.peek() && state.peek()->token_type == TokenType::Colon) {
      const_type = parse_type_annotation()(state);
      if (!const_type) {
        return std::nullopt;
      }
    }

    auto equals = expect(
      state,
      TokenType::Eq,
      ParseErrorType::InvalidLetExpr,
      "Expected `=` after identifier"
    );

    if (!equals) {
      return std::nullopt;
    }

    auto literal = expect_wp(
      state,
      choice<NDPtr>({
        map(parse_literal(), [](NDLiteral xpr) -> NDPtr{
          return std::make_unique<NDLiteral>(std::move(xpr));
        }),
        map(parse_list_expression(), [](NDListExpr xpr) -> NDPtr {
          return std::make_unique<NDListExpr>(std::move(xpr));
        }),
        map(parse_tuple_expression(), [](NDTupleExpr xpr) -> NDPtr {
          return std::make_unique<NDTupleExpr>(std::move(xpr));
        })
      }),
      ParseErrorType::InvalidConstExpr,
      "Const values can only hold primitive data and user-generated types"
    );

    if (!literal) {
      return std::nullopt;
    }

    NDConstExpr expr;
    expr.identifier = std::make_unique<NDIdentifier>(ident.value());
    expr.identifier->type = const_type;
    expr.bound_value = std::move(literal.value());


    checkpoint.commit();
    return expr;
  };
  return parser;
}
