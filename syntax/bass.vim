syntax clear

syntax keyword opcodes nop add sub mul div mod
syntax keyword opcodes move load store push pop
syntax keyword opcodes println print
syntax keyword opcodes cmp jump jumpl jumpg jumpz call return

" r[0-7] must be followed by [a-zA-Z0-9_]
" this prevents registers being highlighted as labels
syntax match  label     /\v<[a-zA-Z_][a-zA-Z0-9_]*>/
syntax match  register  /\v<r[0-7]>/
syntax region string    start=/"/ end=/"/ contains=@Spell
syntax match  char      /\v'((.)|\\n)'/
syntax match  comment   /\v;.*$/
syntax match  number    /\v#\d+|\@\d+|\@r\d+/

highlight link opcodes  Keyword
highlight link myNumber Number
highlight link string   String
highlight link char     String
highlight link register Number
highlight link label    Identifier
highlight link comment  Comment

