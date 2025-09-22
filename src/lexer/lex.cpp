#include "./lex.hpp"
#include <cctype>
#include <cstdio>

TOKEN_TYPE Lex::peek() {
  if (this->pos >= this->ref_stream.size()) {
    return TOKEN_EOF;
  }

  size_t cnt = this->pos;

  std::string buf;
  std::string last_match;
  TOKEN_TYPE tok_type = TOKEN_UNDEF;

  while (cnt < this->ref_stream.size()) {
    buf += this->ref_stream[cnt];
    if (TokenLookupTable.contains(buf)) {
      last_match = buf;
      tok_type = TokenLookupTable.at(buf);
      cnt++;
    } else {
      break;
    }
  }

  if (!last_match.empty()) {
    return tok_type;
  } else {
    return TOKEN_SYMBOL;
  }
}

Token Lex::advance() {
  this->skipWhitespace();

  char chr = this->peekChar();

  if (chr == '\n' || chr == '\r') {
    this->pos++;
    this->line++;
    this->col = 0;
  }

  if (std::isdigit(chr)) {
    return this->readNumber();
  } else if (std::isalnum(chr)) {
    return this->readSymbol();
  } else if (this->isStringOpen(chr)) {
    return this->readString();
  } else if (this->isOperator(chr)) {
    return this->readOperator();
  } else if (chr == EOF) {
    this->pos++;
    return (Token) {
      .tok_type = TOKEN_EOF,
      .tok_val  = "",
      .tok_row  = this->line,
      .tok_col  = this->col
    };
  } else {
    this->pos++;
    this->col++;
    return (Token) {
      .tok_type = TOKEN_UNDEF,
      .tok_val  = std::string(1, chr),
      .tok_row  = this->line,
      .tok_col  = this->col
    };
  }
}

char Lex::peekChar() {
  if (this->pos >= this->ref_stream.size()) {
    return EOF;
  }

  return this->ref_stream[this->pos];
}

Token Lex::readSymbol() {
  std::string buf;
  while(
    this->pos < this->ref_stream.size()
    && (std::isalnum(this->peekChar()) 
    || this->peekChar() == '_'
    || this->peekChar() == '.')
  ) {
    if (this->peekChar() == '\n' || this->peekChar() == '\r') {
      this->pos++;
      this->line++;
      this->col = 0;
    }

    buf += peekChar();

    this->pos++;
    this->col++;
  }

  TOKEN_TYPE type = TOKEN_SYMBOL;

  if (TokenLookupTable.contains(buf)) {
    type = TokenLookupTable.at(buf);
  }

  return (Token) {
    .tok_type = type,
    .tok_val  = buf,
    .tok_row  = this->line,
    .tok_col  = this->col
  };
}


Token Lex::readString() {
  char quote = this->peekChar(); 
  std::string literal;
  this->pos++;
  this->col++;

  while (this->pos < this->ref_stream.size()) {
    char c = this->ref_stream[this->pos];

    if (c == quote) {
      this->pos++;
      this->col++;
      break;
    }

    if (c == '\\') { 
      if (this->pos + 1 < this->ref_stream.size()) {
        literal += c;
        literal += this->ref_stream[this->pos + 1];
        this->pos += 2;
        this->col += 2;
      } else {
        break;
      }
    } else {
      literal += c;
      this->pos++;
      this->col++;
    }
  }

  return {
    .tok_type = TOKEN_STRING_LITERAL,
    .tok_val  = literal,
    .tok_row  = this->line,
    .tok_col  = this->col
  };
}


Token Lex::readOperator() {
  std::string buf;

  if (peekChar() == EOF) {
    this->pos++;
    return (Token) {
      .tok_type = TOKEN_EOF,
      .tok_val  = "",
      .tok_row  = this->line,
      .tok_col  = this->col
    };
  }

  buf += this->peekChar();
  TOKEN_TYPE type = TOKEN_UNDEF;

  if (TokenLookupTable.contains(buf)) {
    type = TokenLookupTable.at(buf);
    this->pos++;
    this->col++;

    if (
      TokenLookupTable.contains(buf + this->peekChar())
      && this->pos < this->ref_stream.size()
    ) {
      buf += this->peekChar();
      type = TokenLookupTable.at(buf);
      this->pos++;
      this->col++;
    }
  } else if (
    TokenLookupTable.contains(buf + this->peekChar())
    && this->pos <= this->ref_stream.size()
  ) {
    buf += this->peekChar();
    type = TokenLookupTable.at(buf);
    this->pos++;
    this->col++;
  }

  return (Token) {
    .tok_type = type,
    .tok_val  = buf,
    .tok_row  = this->line,
    .tok_col  = this->col
  };
}

Token Lex::readNumber() {
  std::string buf;

  TOKEN_TYPE type = TOKEN_INTEGER_LITERAL;
  while (
    this->pos < this->ref_stream.size()
    && std::isdigit(this->ref_stream[this->pos])
  ) {
    buf += this->peekChar();
    this->pos++;
    this->col++;
  }

  if (
    this->pos < this->ref_stream.size()
    && this->ref_stream[this->pos] == '.' 
    && std::isdigit(this->ref_stream[this->pos + 1])
  ) {
    type = TOKEN_FLOAT_LITERAL;
    buf += this->ref_stream[this->pos];
    this->pos++;
    this->col++;

    while (
      this->pos < this->ref_stream.size()
      && std::isdigit(this->peekChar())
    ) {
      buf += this->peekChar();
      this->pos++;
      this->col++;
    }
  }

  return (Token) {
    .tok_type = type,
    .tok_val  = buf,
    .tok_row  = this->line,
    .tok_col  = this->col
  };
}

void Lex::skipWhitespace() {
  while (this->pos < this->ref_stream.size()) {
    char c = this->ref_stream[this->pos];

    if (c == ' ' || c == '\t') {
      this->pos++;
      this->col++;
    } else if (c == '\n') {
      this->line++;
      this->col = 0;
      this->pos++;
    } else if (c == '\r') {
      // Handle Windows CRLF as one newline
      if (this->pos + 1 < this->ref_stream.size() && this->ref_stream[this->pos + 1] == '\n') {
        this->pos += 2;
      } else {
        this->pos++;
      }
      this->line++;
      this->col = 0;
    } else {
      break; // stop on first non-whitespace
    }
  }
}

bool Lex::isOperator(char ch) {
  return ch == '+'
  || ch == '-'
  || ch == '/'
  || ch == '\\'
  || ch == '='
  || ch == '*'
  || ch == '$'
  || ch == '&'
  || ch == '~'
  || ch == '>'
  || ch == '<'
  || TokenLookupTable.contains(std::string() + ch);
}

bool Lex::isStringOpen(char ch) {
  return ch == '\"';
}

