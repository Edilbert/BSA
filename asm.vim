" Vim syntax file
" Language:	xa 6502 cross assembler
" Maintainer:	Clemens Kirchgatterer <clemens@thf.ath.cx>
" Last Change:	2003 May 03

" For version 5.x: Clear all syntax items
" For version 6.x: Quit when a syntax file was already loaded
if version < 600
  syntax clear
elseif exists("b:current_syntax")
  finish
endif

syn case ignore

" Opcodes
syn match a65Opcode	"\<PHP\($\|\s\)"
syn match a65Opcode	"\<PLA\($\|\s\)"
syn match a65Opcode	"\<PLX\($\|\s\)"
syn match a65Opcode	"\<PLY\($\|\s\)"
syn match a65Opcode	"\<PLZ\($\|\s\)"
syn match a65Opcode	"\<SEC\($\|\s\)"
syn match a65Opcode	"\<CLD\($\|\s\)"
syn match a65Opcode	"\<SED\($\|\s\)"
syn match a65Opcode	"\<CLI\($\|\s\)"
syn match a65Opcode	"\<BVC\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BVS\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BCS\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BCC\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<DEY\($\|\s\)"
syn match a65Opcode	"\<DEZ\($\|\s\)"
syn match a65Opcode	"\<INZ\($\|\s\)"
syn match a65Opcode	"\<DEC\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<DEW\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<INW\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<CMP\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<CPX\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<CPZ\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BIT\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<ROL\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<ROR\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<ASL\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<ASW\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<ROW\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<TAZ\($\|\s\)"
syn match a65Opcode	"\<TZA\($\|\s\)"
syn match a65Opcode	"\<MAP\($\|\s\)"
syn match a65Opcode	"\<EOM\($\|\s\)"
syn match a65Opcode	"\<TXA\($\|\s\)"
syn match a65Opcode	"\<TYA\($\|\s\)"
syn match a65Opcode	"\<TSX\($\|\s\)"
syn match a65Opcode	"\<TXS\($\|\s\)"
syn match a65Opcode	"\<LDA\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LDX\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LDY\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LDZ\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<STA\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<PLP\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BRK\($\|\s\)"
syn match a65Opcode	"\<RTI\($\|\s\)"
syn match a65Opcode	"\<NOP\($\|\s\)"
syn match a65Opcode	"\<SEI\($\|\s\)"
syn match a65Opcode	"\<CLV\($\|\s\)"
syn match a65Opcode	"\<PHA\($\|\s\)"
syn match a65Opcode	"\<PHX\($\|\s\)"
syn match a65Opcode	"\<BRA\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<JMP\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<JSR\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<RTS\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<CPY\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BNE\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BEQ\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BMI\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LSR\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<INX\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<INY\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<INC\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<ADC\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<SBC\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<AND\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<ORA\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<STX\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<STY\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<STZ\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<EOR\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<DEX\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BPL\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<CLC\($\|\s\)"
syn match a65Opcode	"\<PHY\($\|\s\)"
syn match a65Opcode	"\<PHZ\($\|\s\)"
syn match a65Opcode	"\<TRB\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BBR.\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<BBS.\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<RMB.\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<SMB.\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<TAY\($\|\s\)"
syn match a65Opcode	"\<TAX\($\|\s\)"
syn match a65Opcode	"\<LBNE\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LBEQ\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LBMI\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LBPL\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LBVC\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LBVS\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LBCS\($\|\s\)" nextgroup=a65Address
syn match a65Opcode	"\<LBCC\($\|\s\)" nextgroup=a65Address

" Addresses
"syn match a65Address	"\s*!\=$[0-9A-F]\{2}\($\|\s\)"
"syn match a65Address	"\s*($[0-9A-F]\{2})\($\|\s\)"

syn match a65Address	"(.*,X)"
syn match a65Address	"(.*),Y"
syn match a65Address	"(.*),Z"
syn match a65Address	"\[.*\],Z"
syn match a65Address	",X"

" Numbers
syn match a65Number	"'.'"
syn match a65Number	"\<[0-9]*\>"
syn match a65Number	"$[0-9A-F]*\>"
syn match a65Number	"#[0-9]*\>"
syn match a65Number	"#'.'"
syn match a65Number	"#$[0-9A-F]*\>"
syn match a65Number	"#&[0-7]*\>"
syn match a65Number	"#%[01]*\>"

syn case match

" Types
syn match a65Type	"\(^\|\s\)\.BYTE\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.WORD\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.QUAD\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.REAL\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.FILL\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.byt\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.word\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.asc\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.dsb\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.fopt\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.text\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.data\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.bss\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.zero\($\|\s\)"
syn match a65Type	"\(^\|\s\)\.align\($\|\s\)"

" Blocks
syn match a65Section	"\(^\|\s\)\.(\($\|\s\)"
syn match a65Section	"\(^\|\s\)\.)\($\|\s\)"

" Strings
syn match a65String	"\".*\""

" Programm Counter
syn region a65PC	start="\*=" end="\>" keepend

" HI/LO Byte
syn region a65HiLo	start="#[<>]" end="$\|\s" contains=a65Comment keepend

" Comments
syn keyword a65Todo	TODO XXX FIXME BUG contained
syn match   a65Comment	";.*"hs=s contains=a65Todo
syn match   a65Comment	"\*\*"hs=s contains=a65Comment
syn region  a65Comment	start="/\*" end="\*/" contains=a65Todo,a65Comment

" Preprocessor
syn region a65PreProc	start="^#" end="$" contains=a65Comment,a65Continue
syn match  a65End			excludenl /end$/ contained
syn match  a65Continue	"\\$" contained

" Define the default highlighting.
" For version 5.7 and earlier: only when not done already
" For version 5.8 and later: only when an item doesn't have highlighting yet
if version >= 508 || !exists("did_a65_syntax_inits")
  if version < 508
    let did_a65_syntax_inits = 1
    command -nargs=+ HiLink hi link <args>
  else
    command -nargs=+ HiLink hi def link <args>
  endif

  HiLink a65Section	Special
  HiLink a65Address	Special
  HiLink a65Comment	Comment
  HiLink a65PreProc	PreProc
  HiLink a65Number	Number
  HiLink a65String	String
  HiLink a65Type	Statement
  HiLink a65Opcode	Type
  HiLink a65PC		Error
  HiLink a65Todo	Todo
  HiLink a65HiLo	Number

  delcommand HiLink
endif

let b:current_syntax = "asm"
