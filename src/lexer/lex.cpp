#include "./lex.hpp"
#include <cctype>
#include <cstdio>


TOKEN_TYPE Lex::peek() {
  if (this->pos >= this->ref_stream.size()) {
    return TOKEN_EOF;
  }

  size_t cnt = this->pos;
  char c = this->ref_stream[cnt];

  // --- Skip whitespace ---
  while (cnt < this->ref_stream.size() && 
         (this->ref_stream[cnt] == ' ' || this->ref_stream[cnt] == '\t' || 
          this->ref_stream[cnt] == '\n' || this->ref_stream[cnt] == '\r')) {
    cnt++;
  }

  if (cnt >= this->ref_stream.size()) {
    return TOKEN_EOF;
  }

  c = this->ref_stream[cnt];

  // --- Handle digits ---
  if (std::isdigit(c)) {
    return TOKEN_INTEGER_LITERAL;
  }

  // --- Handle string literal opening ---
  if (this->isStringOpen(c)) {
    return TOKEN_STRING_LITERAL;
  }

  // --- Handle operators ---
  {
    std::string buf(1, c);
    size_t lookahead = cnt + 1;

    // Check for possible multi-char operators
    while (lookahead < this->ref_stream.size()) {
      std::string next = buf + this->ref_stream[lookahead];
      if (this->isOperator(next)) {
        buf = next;
        lookahead++;
      } else {
        break;
      }
    }

    if (this->isOperator(buf)) {
      return TokenLookupTable.contains(buf)
               ? TokenLookupTable.at(buf)
               : TOKEN_UNDEF;
    }
  }

  // --- Handle symbols or identifiers ---
  if (this->isSymbol(c)) {
    std::string buf;
    while (cnt < this->ref_stream.size() && this->isSymbol(this->ref_stream[cnt])) {
      buf += this->ref_stream[cnt];
      cnt++;
    }

    // Check if keyword
    if (TokenLookupTable.contains(buf)) {
      return TokenLookupTable.at(buf);
    }

    return TOKEN_SYMBOL;
  }

  // --- Fallback ---
  return TOKEN_UNDEF;
}



Token Lex::advance() {
  // Skip all whitespace and newlines before reading
  this->skipWhitespace();

  // End of stream check
  if (this->pos >= this->ref_stream.size()) {
    return {
      .tok_type = TOKEN_EOF,
      .tok_val  = "",
      .tok_row  = this->line,
      .tok_col  = this->col
    };
  }

  char c = this->peekChar();

  // --- Handle numbers ---
  if (std::isdigit(c)) {
    return this->readNumber();
  }

  // --- Handle strings ---
  if (this->isStringOpen(c)) {
    return this->readString();
  }

  // --- Handle operators (including multi-character) ---
  {
    std::string buf(1, c);
    size_t lookahead = this->pos + 1;

    // Attempt to extend multi-character operator
    while (lookahead < this->ref_stream.size()) {
      std::string next = buf + this->ref_stream[lookahead];
      if (this->isOperator(next)) {
        buf = next;
        lookahead++;
      } else {
        break;
      }
    }

    if (this->isOperator(buf)) {
      TOKEN_TYPE type = TokenLookupTable.contains(buf)
                          ? TokenLookupTable.at(buf)
                          : TOKEN_UNDEF;

      // Advance position by operator length
      this->pos += buf.size();
      this->col += buf.size();

      return {
        .tok_type = type,
        .tok_val  = buf,
        .tok_row  = this->line,
        .tok_col  = this->col
      };
    }
  }

  // --- Handle symbols / identifiers ---
  if (this->isSymbol(c)) {
    std::string buf;
    while (this->pos < this->ref_stream.size() && this->isSymbol(this->peekChar())) {
      buf += this->peekChar();
      this->pos++;
      this->col++;
    }

    TOKEN_TYPE type = TOKEN_SYMBOL;

    // Check for keyword
    if (TokenLookupTable.contains(buf)) {
      type = TokenLookupTable.at(buf);
    }

    return {
      .tok_type = type,
      .tok_val  = buf,
      .tok_row  = this->line,
      .tok_col  = this->col
    };
  }

  // --- Fallback for undefined characters ---
  this->pos++;
  this->col++;

  return {
    .tok_type = TOKEN_UNDEF,
    .tok_val  = std::string(1, c),
    .tok_row  = this->line,
    .tok_col  = this->col
  };
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
    && (this->isSymbol(this->peekChar()))
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

  TOKEN_TYPE type = TOKEN_UNDEF;
  while (this->pos < this->ref_stream.size()) {
    char c = this->ref_stream[this->pos];

    if (c == quote) {
      type = TOKEN_STRING_LITERAL;
      this->pos++;
      this->col++;
      break;
    } else if (c == '\r' || c == '\n') {
      this->pos++;
      this->line++;
      this->col = 0;
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
    .tok_type = type,
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

bool Lex::isOperator(const std::string& ch) {
  return ch == "+"
  || ch == "-"
  || ch == "/"
  || ch == "\\"
  || ch == "="
  || ch == "*"
  || ch == "$"
  || ch == "&"
  || ch == "~"
  || ch == ">"
  || ch == "<"
  || ch == ","
  || ch == "::"
  || ch == ":>"
  || ch == "|="
  || ch == ">="
  || ch == "<="
  || ch == "=="
  || TokenLookupTable.contains(std::string() + ch);
}

bool Lex::isSymbol(char c) {
  return std::isalnum(c)
  || c == '.'
  || c == '_'
  || c == ':';
}

bool Lex::isStringOpen(char ch) {
  return ch == '\"';
}

void Lex::extractAllTokens() {
  while (true) {
    Token t = this->advance();
    this->tokens.push_back(t);

    if (t.tok_type == TOKEN_EOF) {
      break;
    }
  }
}

std::vector<Token> Lex::getTokenList() {
  return this->tokens;
}
