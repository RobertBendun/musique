" Vim syntax file
" Language: Musique
" Maintainer: Robert Bendun
" Latest Change: 2022-05-22
" Filenames: *.mq

if exists("b:current_syntax")
	finish
endif

syn keyword musiqueVariableDeclaration :=
syn keyword musiqueOperators * + - / < <= == >= > != . ** and or

syn match musiqueParameterSplitter display "|"
syn match musiqueExpressionDelimiter display ";"

syn match musiqueInteger display "[0-9][0-9_]*"

syn keyword musiqueConstant true false nil

syn keyword musiqueDefaultBuiltins bpm call ceil chord down flat floor fold for hash if incoming instrument len max min mix note_off note_on nprimes oct par partition permute pgmchange play program_change range reverse rotate round shuffle sim sort try typeof uniq unique up update scan map
syn keyword musiqueLinuxBuiltins say

syn match musiqueComment "--.*$"
syn match musiqueComment "#!.*$"

syn region musiqueComment start="----*" end="----*"

syn region musiqueBlock	matchgroup=musiqueParen start="\["   skip="|.\{-}|"			matchgroup=musiqueParen end="\]" fold transparent

let b:current_syntax = "musique"

hi def link musiqueVariableDeclaration Define
hi def link musiqueParameterSplitter Delimiter
hi def link musiqueExpressionDelimiter Delimiter
hi def link musiqueParen Delimiter
hi def link musiqueOperators Operator
hi def link musiqueComment Comment
hi def link musiqueInteger Number
hi def link musiqueConstant Constant
hi def link musiqueDefaultBuiltins Function
hi def link musiqueLinuxBuiltins Function
