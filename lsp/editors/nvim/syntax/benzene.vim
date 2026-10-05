" Vim syntax file for Benzene.
"
" This is the fallback layer. When the language server is attached its semantic
" tokens take priority and colour identifiers by what the compiler resolved
" them to; these rules are what you see before that happens, and in buffers the
" server never attaches to.

if exists("b:current_syntax")
  finish
endif

" `Cmt` either wraps a brace-delimited block or runs to end of line.
"
" The two forms have to be kept apart. A plain `.*$` match would also match
" `Cmt {`, and since it starts at the same column it would win -- leaving the
" body and the closing brace outside the comment entirely.
"
" A backtick escapes the next character, so ``}` does not close the block.
" `skip=` has to consume both characters or the brace would still end it.
syn region  benzeneComment start="\<Cmt\>\s*{" skip=+`.+ end="}" contains=@Spell
syn match   benzeneComment "\<Cmt\>\s*$" contains=@Spell
syn match   benzeneComment "\<Cmt\>\s*[^{[:space:]].*$" contains=@Spell

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
