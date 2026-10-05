" Vim syntax file for Benzene.
"
" This is the fallback layer. When the language server is attached its semantic
" tokens take priority and colour identifiers by what the compiler resolved
" them to; these rules are what you see before that happens, and in buffers the
" server never attaches to.

if exists("b:current_syntax")
  finish
endif

" `Cmt` runs to end of line, or wraps a brace-delimited block. Defined first so
" keywords inside a comment do not match.
syn region  benzeneComment start="\<Cmt\>\s*{" end="}" contains=@Spell
syn match   benzeneComment "\<Cmt\>.*$" contains=@Spell

syn keyword benzeneKeyword   func end case default let const type
syn keyword benzeneInclude   Load
syn keyword benzeneLambda    Fn
syn keyword benzeneBoolean   True False
syn keyword benzeneConstant  Nil

syn match   benzeneNumber    "\<\d\+\>"
syn match   benzeneFloat     "\<\d\+\.\d*\>"
syn region  benzeneString    start=+"+ skip=+\\.+ end=+"+ oneline contains=benzeneEscape
syn match   benzeneEscape    +\\[\\"nrt0]+ contained

" `|=>` and `:>` before their single-character prefixes, matching how the
" lexer itself resolves them.
syn match   benzeneOperator  "|=>\|:>\|==\|\~=\|<=\|>=\|&&\|||\|[-+*/%<>=\~]"

" A name directly after `:` or `:>` is a type annotation. The parser stores
" these verbatim rather than resolving them, so position is all there is to go
" on here too.
syn match   benzeneType      ":\s*\zs\u\w*"
syn match   benzeneType      ":>\s*\zs\u\w*"

syn match   benzeneFunction  "\<\h\w*\ze\s*("
syn match   benzeneDelimiter "[(){}\[\],.]"

hi def link benzeneComment   Comment
hi def link benzeneKeyword   Keyword
hi def link benzeneInclude   Include
hi def link benzeneLambda    Keyword
hi def link benzeneBoolean   Boolean
hi def link benzeneConstant  Constant
hi def link benzeneNumber    Number
hi def link benzeneFloat     Float
hi def link benzeneString    String
hi def link benzeneEscape    SpecialChar
hi def link benzeneOperator  Operator
hi def link benzeneType      Type
hi def link benzeneFunction  Function
hi def link benzeneDelimiter Delimiter

let b:current_syntax = "benzene"
