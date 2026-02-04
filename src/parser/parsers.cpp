#include "parsers.hpp"

ASTParser parse_let_bind_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::LetKeyword),
    p_parse_match(TokenType::Identifier),
    parse_expr()
  });
}

ASTParser parse_literal_expr() {
  return p_parse_options({
    p_parse_match(TokenType::IntegerLiteral),
    p_parse_match(TokenType::FloatLiteral),
    p_parse_match(TokenType::StringLiteral),
    p_parse_match(TokenType::UTStringLiteral),
    p_parse_match(TokenType::NilLiteral),
    p_parse_match(TokenType::TrueLiteral),
    p_parse_match(TokenType::FalseLiteral),
  });
}

ASTParser parse_identifier_expr() {
  return p_parse_match(TokenType::Identifier);
}

ASTParser parse_constant_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::ConstantKeyword),
    p_parse_match(TokenType::Identifier),
    p_parse_options({
      parse_literal_expr(),
      parse_expr()
    })
  });
}

ASTParser parse_func_invoc_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::Identifier),
    p_parse_match(TokenType::RParen),
    p_parse_options({
      p_parse_match(TokenType::Identifier),
      parse_expr(),
    }),
    p_parse_match(TokenType::LParen)
  });
}

// incomplete, but will come back later.
ASTParser parse_func_params() {
  return p_parse_options({
    p_parse_seq({
      p_parse_match(TokenType::Identifier), 
      p_parse_match(TokenType::Colon),
      p_parse_match(TokenType::Identifier)
    }),
    p_parse_match(TokenType::Identifier)
  });
}

ASTParser parse_func_decl_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::FuncStart),
    p_parse_match(TokenType::Identifier),
    p_parse_match(TokenType::RParen),
    parse_func_params(),
    p_parse_match(TokenType::LParen),
    parse_func_decl_body(),
    p_parse_match(TokenType::EndStmt)
  });
}

ASTParser m_parse_unaryop_expr() {
  return p_parse_seq({
    p_parse_match(TokenType::MinusOp),
    m_parse_primary()
  });
}

ASTParser m_parse_mulplicative() {
  return m_parse_chain_left(
    m_parse_unaryop_expr(), 
    p_parse_options({
      p_parse_match(TokenType::MultiplyOp),
      p_parse_match(TokenType::DivideOp)
    })
  );
}

ASTParser m_parse_additive() {
  return m_parse_chain_left(
    m_parse_mulplicative(),
    p_parse_options({
      p_parse_match(TokenType::PlusOp),
      p_parse_match(TokenType::MinusOp)
    })
  );
}

ASTParser m_parse_primary() {
  return p_parse_options({
    parse_literal_expr(),
    parse_identifier_expr(),
    p_parse_seq({
      p_parse_match(TokenType::LParen),
      parse_expr(),
      p_parse_match(TokenType::RParen)
    })
  });
}


ASTParser parse_expr() {
  return p_parse_options({
    parse_let_bind_expr(),
    parse_constant_expr(),
    parse_func_decl_expr(),
    m_parse_additive()
  });
}
