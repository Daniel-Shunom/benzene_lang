# Benzene Grammar

This document specifies the surface grammar of Benzene as currently implemented
by the `ether` front-end (lexer + parser). It is descriptive — it tracks the
parser, not a future ideal — and should be updated alongside parser changes.

Notation: EBNF-ish.
- `A | B` — alternatives
- `A?`    — optional
- `A*`    — zero or more
- `A+`    — one or more
- `"x"`   — literal text (token)
- `<x>`   — terminal token class produced by the lexer
- All whitespace and comments are skipped between tokens.

## 1. Lexical structure

### 1.1 Tokens

```
<integer>     ::= digit+
<float>       ::= digit+ "." digit*
<string>      ::= '"' string-char* '"'
<ut-string>   ::= '"' string-char* (newline | EOF)        ; unterminated
<identifier>  ::= alpha (alpha | digit | "_")*
<import-mod>  ::= (alpha | "_" | ".")+                    ; emitted after Load
<unknown>     ::= any single char not consumed elsewhere  ; reported by lexer
```

`string-char` accepts the standard escapes `\\ \" \n \t \r \0`. Any other
escape character is taken literally.

### 1.2 Keywords

```
Load   Cmt   const   let   func   end   case   default
True   False   Nil
```

`Cmt` introduces a comment and produces no tokens:

```
Cmt <text-to-newline>
Cmt "{" <text-with-balanced-content> "}"
```

### 1.3 Operators and punctuation

Listed roughly by binding tightness.

```
unary    ::= "-" | "~"
mul      ::= "*" | "/"            ; "%" is lexed but not yet wired in expressions
add      ::= "+" | "-"
cmp      ::= "<" | "<=" | ">" | ">="
eq       ::= "==" | "~="
and-op   ::= "&&"
or-op    ::= "||"
pipe     ::= "|=>"
arrow    ::= ":>"                 ; return-type / case-arrow
assign   ::= "="
punct    ::= "(" | ")" | "{" | "}" | "[" | "]" | ":" | "," | "."
```

The lexer matches multi-character operators before their single-character
prefixes (`>=` before `>`, `==` before `=`, etc.).

## 2. Top level

```
program       ::= top-expr*

top-expr      ::= import-stmt
                | const-decl
                | let-decl
                | func-decl
                | value-expr
```

Note: `let-decl` and `value-expr` are syntactically accepted at the top level
but are flagged by the symbol resolver. See §6.

## 3. Declarations

### 3.1 Imports

```
import-stmt   ::= "Load" <import-mod>
```

### 3.2 Constants

```
const-decl    ::= "const" <identifier> type-annot? "=" literal
```

`const` values must be literals (no expressions). Constants are only valid at
module scope.

### 3.3 Let bindings

```
let-decl      ::= "let" <identifier> type-annot? "=" value-expr
```

`let` is only valid inside a function body or inside a scoped expression (which
is itself nested in a function body).

### 3.4 Type annotations

```
type-annot    ::= ":" <identifier>
```

Type identifiers are not interpreted by the parser; they are stored verbatim
and surfaced to later passes (type-checker).

### 3.5 Function declarations

```
func-decl     ::= "func" <identifier> "(" param-list? ")" return-type? body "end"

param-list    ::= param ("," param)*
param         ::= <identifier> type-annot?

return-type   ::= ":>" <identifier>

body          ::= top-expr*       ; same shapes as top-expr; resolver applies
                                  ; function-scope rules
```

## 4. Expressions

### 4.1 Value expressions

```
value-expr    ::= pipe-chain
                | binary-expr
                | case-expr

pipe-chain    ::= call-expr ( "|=>" call-expr )+

call-expr     ::= <identifier> "(" arg-list? ")"

arg-list      ::= primary-expr ("," primary-expr)*
```

A call followed by no `|=>` is parsed via `binary-expr` (which descends to
`primary-expr`); the dedicated pipe-chain arm exists so that the trailing
`|=>` is not orphaned.

### 4.2 Binary expressions and precedence

From lowest to highest precedence (left-associative at every level):

```
or-expr       ::= and-expr   ( "||"        and-expr   )*
and-expr      ::= eq-expr    ( "&&"        eq-expr    )*
eq-expr       ::= cmp-expr   ( ("=="|"~=") cmp-expr   )*
cmp-expr      ::= add-expr   ( cmp         add-expr   )*
add-expr      ::= mul-expr   ( ("+"|"-")   mul-expr   )*
mul-expr      ::= unary-expr ( ("*"|"/")   unary-expr )*
unary-expr    ::= ("-"|"~")? primary-expr
```

### 4.3 Primary expressions

```
primary-expr  ::= call-expr
                | scoped-expr
                | <integer> | <float> | <string> | <ut-string>
                | "True" | "False" | "Nil"
                | <identifier>

scoped-expr   ::= "{" top-expr* "}"
```

A scoped expression is a block of zero or more expressions whose value is the
last expression. Scoped expressions are valid inside function bodies and
inside other scoped expressions.

### 4.4 Case expressions

```
case-expr     ::= "case" value-expr ":" case-branch+ "end"

case-branch   ::= value-expr ":>" value-expr
```

Currently only the first pattern in a branch is collected; multi-pattern
branches are reserved for a future revision.

## 5. Comments

Comments produce no tokens.

```
single-line   ::= "Cmt" <chars-until-newline>
multi-line    ::= "Cmt" "{" <any-chars-incl-newlines> "}"
```

A `\`` inside a multi-line comment escapes the next character.

## 6. Scope rules (resolver, not parser)

The parser accepts more shapes than the language permits at any given scope.
The symbol resolver (`SymbolResolver`) enforces the following:

| Construct              | Allowed scopes                                              |
| ---------------------- | ----------------------------------------------------------- |
| `Load` import          | Module                                                       |
| `const`                | Module, Application                                          |
| `let`                  | FunctionExpression, ScopedExpression                         |
| `func`                 | Module, FunctionExpression, ScopedExpression                 |
| scoped expression `{}` | FunctionExpression, ScopedExpression                         |
| `case`                 | FunctionExpression, ScopedExpression                         |
| call expression        | CaseExpression, FunctionExpression, ScopedExpression         |
| identifier reference   | Module, FunctionExpression, ScopedExpression                 |
| literal                | Module, FunctionExpression, ScopedExpression                 |

Violations mark the offending node `is_poisoned` and emit a `Resolver`-phase
diagnostic. The parser does not attempt any structural rewrite based on these
rules.

## 7. Reserved or partially-implemented forms

The following are recognized by the lexer but not yet wired into the parser
or are accepted in syntax only:

- `%` (PercentOp) is lexed but not used in any precedence level.
- `default` is lexed but the case-expression parser does not yet consume it.
- `[` and `]` (LBrac, RBrac) are lexed but no list/array syntax is parsed.
- Multi-pattern case branches are not yet supported.

These are kept reserved so future revisions don't have to break tokenization.
