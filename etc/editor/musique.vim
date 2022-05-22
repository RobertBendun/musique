" Vim syntax file
" Language: Musique
" Maintainer: Robert Bendun
" Latest Change: 2022-05-22
" Filenames: *.mq

if exists("b:current_syntax")
	finish
endif

syn keyword musiqueVariableDeclaration var
syn keyword musiqueOperators * + - / < <= == >= > !=

syn match musiqueInteger display "[0-9][0-9_]*"

syn keyword musiqueConstant true false nil

syn match musiqueComment "--.*$"
syn match musiqueComment "#!.*$"

syn region musiqueBlock	matchgroup=musiqueParen start="\["   skip="|.\{-}|"			matchgroup=musiqueParen end="\]" fold transparent

let b:current_syntax = "musique"

hi def link musiqueVariableDeclaration Define
hi def link musiqueParen Delimiter
hi def link musiqueOperators Operator
hi def link musiqueComment Comment
hi def link musiqueInteger Number
hi def link musiqueConstant Constant
