syntax clear

syntax keyword opcodes nop add sub mul div mod
syntax keyword opcodes move load store push pop
syntax keyword opcodes println print
syntax keyword opcodes cmp jump jumpl jumpg jumpz call return

syntax match  label     /\v<[a-zA-Z_][a-zA-Z0-9_]*>/
syntax match  register  /\v<r[0-7]>/
syntax region String    start=/"/ end=/"/ contains=@Spell
syntax match  char      /\v'((.)|\\n)'/
syntax match  Comment   /\v;.*$/
syntax match  Number    /\v#\d+|\@\d+|\@r\d+/

highlight link opcodes  Keyword
highlight link char     String
highlight link register Number
highlight link label    Identifier

