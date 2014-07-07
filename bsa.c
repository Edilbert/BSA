/*

*********************
Black Smurf Assembler
*********************

Version:  1.2  16-Apr-2014 for 6502 / 6510 CPU's

The assembler was developed and tested on an iMAC with OSX Mountain Lion.
Using no specific options of the host system, it should run on any
computer with a standard C-compiler, e.g. Linux, Windows, Amiga OS, etc.

This assembler is a Cross-Assembler, it is run on a (modern) host system,
but produces code for target machines running a 65xx CPU, like the
Commodore, Atari, Apple II.

Compiling
=========
If your compiler is named "gcc" for example, compile with:

gcc -o bsa bsa.c

Running
=======
If you have a source code named "hello.asm", run the assembler with:

bsa hello

It will read "hello.asm" as input file and write the binary file
"hello" and the listing with cross reference "hello.lst".

Case sensitivity
================
6502 mnemonics, and pseudo opcodes are insensitive to case:

LDA lda Lda are all equivalent (Load Accumulator)

.BYTE .byte .Byte are all equivalent (define byte data)

Label and named constants are sensitive!

LDA #Cr  and LDA #CR  use different constants!
JMP Lab_10 and JMP LAB_10  jump to different targets!

Examples of pseudo opcodes (directives):
========================================
.ORG  $E000                    set program counter
  * = $E000                    set program counter
.LOAD $0401                    precede binary with a CBM load address
.STORE BASIC_ROM,$2000,"basic.rom" write binary image file "basic.rom"
.BYTE $20,"Example",0          stores a series of byte data
.WORD LAB_10, WriteTape,$0200  stores a series of word data
.QUAD 100000                   stores a 32 bit integer in CBM format
.REAL 3.1415926                stores a 40 bit real in CBM format
.FILL  N ($EA)                 fill memory with N bytes containing $EA
.FILL  $A000 - * (0)           fill memory from pc(*) upto $9FFF  
TXTTAB .BSS 2                  define TXTTAB and increase address pointer by 2
  & = $033A                    set BSS address pointer

Examples of Operands
====================
    6      = decimal constant
 $A12      = hex constant
MURX       = label or constant
"hello\r"  = ASCII string with CR at end
'hello'^   = PETSCII (Commodore) string ^ causes the last byte to be OR'ed with $80
Table_Offset + 2 * [LEN-1] = address

Arithmetic and logic operators in address calculations
======================================================
<     low byte
>     high byte
[     arithmetic bracket
]     arithmetic bracket
+     addition
-     sign or subtraction
*     multiplication or program counter (context sensitive)
/     division
$     constant is hex
%     constant is binary
!     logical negate
~     bitwise complement
&     bitwise and
|     bitwise or

User macros
===========
Example:

MACRO LDXY(Word)
   LDX Word
   LDY Word+1
ENDMAC

defines a MACRO for loading a 16bit word into X and Y

Call:

LDXY(Vector)

Generated Code:

   LDX Vector
   LDY Vector+1

Macros accept up to 10 parameter and may have any length.

Conditional assembly
====================
Example:

#if C64
   STA $D000
#else
   STA $9000
#endif

assembles the first statement if C64 is not zero and the second if zero.

Another example:

#if C64 | PLUS4          ; true if either C64 or PLUS4 is true (not zero)
   LDA #MASK
#if C64
   STA ICR_REG
#else
   STA PLUS4_ICR_REG
#endif                   ; finishes inner if
#endif                   ; finishes outer if

The maximum nesting depth is 10

For more examples see the complete operating system for C64 and VC20
(VIC64.asm.gz) on the forum "www.forum64.de".

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

char *Strcasestr(const char *s1, const char *s2)
{
   char h1[256];
   char h2[256];
   char *r;

    int i;

    for (i=0 ; i < strlen(s1)+1 ; ++i) h1[i] = tolower(s1[i]);
    for (i=0 ; i < strlen(s2)+1 ; ++i) h2[i] = tolower(s2[i]);

    r = strstr(h1,h2);
    if (r)
    {
       i = r - h1;
       r += i;
    }
    return r;
}

enum AddressType {
None,           // invalid
Impl,Accu,      // 1 byte opcodes
Rela,Imme,      // 2 byte opcodes
Zpag,Zpgx,Zpgy, // 2 byte zero page modes
Abso,Absx,Absy, // 3 byte absolute modes
Indi,Indx,Indy  //   indirect modes
};

struct s6502
{
   int  cpu;     // 0 = 6502, 1 = 65c02
   int  len;
   int  amo;
   char mne[8];
   char pre[8];
   char pos[8];
   char fmt[8];
};

struct s6502 set[256] = 
{
   {0, 1, Impl, "BRK" , "" , ""   , ""       },   // 00
   {0, 2, Indx, "ORA" , "(", ",X)", "(%s,X)" },   // 01
   {0, 0, None, "---" , "" , ""   , ""       },   // 02
   {0, 0, None, "---" , "" , ""   , ""       },   // 03
   {1, 2, Zpag, "TSB" , "" , ""   , "%s"     },   // 04
   {0, 2, Zpag, "ORA" , "" , ""   , "%s"     },   // 05
   {0, 2, Zpag, "ASL" , "" , ""   , "%s"     },   // 06
   {1, 2, Zpag, "RMB0", "" , ""   , "%s"     },   // 07
   {0, 1, Impl, "PHP" , "" , ""   , ""       },   // 08
   {0, 2, Imme, "ORA" , "#", ""   , "#%s"    },   // 09
   {0, 1, Accu, "ASL" , "A", ""   , "A"      },   // 0a
   {0, 0, None, "---" , "" , ""   , ""       },   // 0b
   {1, 3, Abso, "TSB" , "" , ""   , "%s"     },   // 0c
   {0, 3, Abso, "ORA" , "" , ""   , "%s"     },   // 0d
   {0, 3, Abso, "ASL" , "" , ""   , "%s"     },   // 0e
   {1, 3, Zpag, "BBR0", "" , ""   , "%s"     },   // 0f

   {0, 2, Rela, "BPL" , "" , ""   , "%s"     },   // 10
   {0, 2, Indy, "ORA" , "(", "),Y", "(%s),Y" },   // 11
   {1, 3, Indi, "ORA" , "(", ")"  , "(%s)"   },   // 12
   {0, 0, None, "---" , "" , ""   , ""       },   // 13
   {1, 2, Zpag, "TRB" , "" , ""   , "%s"     },   // 14
   {0, 2, Zpgx, "ORA" , "" , ",X" , "%s,X"   },   // 15
   {0, 2, Zpgx, "ASL" , "" , ",X" , "%s,X"   },   // 16
   {1, 2, None, "RMB1", "" , ""   , "%s"     },   // 17
   {0, 1, Impl, "CLC" , "" , ""   , ""       },   // 18
   {0, 3, Absy, "ORA" , "" , ",Y" , "%s,Y"   },   // 19
   {1, 1, Accu, "INC" , "A", ""   , "A"      },   // 1a
   {0, 0, None, "---" , "" , ""   , ""       },   // 1b
   {1, 3, Abso, "TRB" , "" , ""   , "%s"     },   // 1c
   {0, 3, Absx, "ORA" , "" , ",X" , "%s,X"   },   // 1d
   {0, 3, Absx, "ASL" , "" , ",X" , "%s,X"   },   // 1e
   {1, 3, Zpag, "BBR1", "" , ""   , "%s"     },   // 1f

   {0, 3, Abso, "JSR" , "" , ""   , "%s"     },   // 20
   {0, 2, Indx, "AND" , "(", ",X)", "(%s,X)" },   // 21
   {0, 0, None, "---" , "" , ""   , ""       },   // 22
   {0, 0, None, "---" , "" , ""   , ""       },   // 23
   {0, 2, Zpag, "BIT" , "" , ""   , "%s"     },   // 24
   {0, 2, Zpag, "AND" , "" , ""   , "%s"     },   // 25
   {0, 2, Zpag, "ROL" , "" , ""   , "%s"     },   // 26
   {1, 2, Zpag, "RMB2", "" , ""   , "%s"     },   // 27
   {0, 1, Impl, "PLP" , "" , ""   , ""       },   // 28
   {0, 2, Imme, "AND" , "#", ""   , "#%s"    },   // 29
   {0, 1, Accu, "ROL" , "A", ""   , "A"      },   // 2a
   {0, 0, None, "---" , "" , ""   , ""       },   // 2b
   {0, 3, Abso, "BIT" , "" , ""   , "%s"     },   // 2c
   {0, 3, Abso, "AND" , "" , ""   , "%s"     },   // 2d
   {0, 3, Abso, "ROL" , "" , ""   , "%s"     },   // 2e
   {1, 3, Zpag, "BBR2", "" , ""   , "%s"     },   // 2f

   {0, 2, Rela, "BMI" , "" , ""   , "%s"     },   // 30
   {0, 2, Indy, "AND" , "(", "),Y", "(%s),Y" },   // 31
   {1, 2, Indi, "AND" , "(", ")"  , "(%s)"   },   // 32
   {0, 0, None, "---" , "" , ""   , ""       },   // 33
   {1, 2, Zpgx, "BIT" , "" , ",X" , "%s,X"   },   // 34
   {0, 2, Zpgx, "AND" , "" , ",X" , "%s,X"   },   // 35
   {0, 2, Zpgx, "ROL" , "" , ",X" , "%s,X"   },   // 36
   {1, 2, Zpag, "RMB3", "" , ""   , "%s"     },   // 37
   {0, 1, Impl, "SEC" , "" , ""   , ""       },   // 38
   {0, 3, Absy, "AND" , "" , ",Y" , "%s,Y"   },   // 39
   {1, 1, Accu, "DEC" , "A", ""   , "A"      },   // 3a
   {0, 0, None, "---" , "" , ""   , ""       },   // 3b
   {1, 3, Absx, "BIT" , "" , ",X" , "%s,X"   },   // 3c
   {0, 3, Absx, "AND" , "" , ",X" , "%s,X"   },   // 3d
   {0, 3, Absx, "ROL" , "" , ",X" , "%s,X"   },   // 3e
   {1, 3, Zpag, "BBR3", "" , ""   , "%s"     },   // 3f

   {0, 1, Impl, "RTI" , "" , ""   , ""       },   // 40
   {0, 2, Indx, "EOR" , "(", ",X)", "(%s,X)" },   // 41
   {0, 0, None, "---" , "" , ""   , ""       },   // 42
   {0, 0, None, "---" , "" , ""   , ""       },   // 43
   {0, 0, None, "---" , "" , ""   , ""       },   // 44
   {0, 2, Zpag, "EOR" , "" , ""   , "%s"     },   // 45
   {0, 2, Zpag, "LSR" , "" , ""   , "%s"     },   // 46
   {1, 2, Zpag, "RMB4", "" , ""   , "%s"     },   // 47
   {0, 1, Impl, "PHA" , "" , ""   , ""       },   // 48
   {0, 2, Imme, "EOR" , "#", ""   , "#%s"    },   // 49
   {0, 1, Accu, "LSR" , "A", ""   , "A"      },   // 4a
   {0, 0, None, "---" , "" , ""   , ""       },   // 4b
   {0, 3, Abso, "JMP" , "" , ""   , "%s"     },   // 4c
   {0, 3, Abso, "EOR" , "" , ""   , "%s"     },   // 4d
   {0, 3, Abso, "LSR" , "" , ""   , "%s"     },   // 4e
   {1, 3, Zpag, "BBR4", "" , ""   , "%s"     },   // 4f

   {0, 2, Rela, "BVC" , "" , ""   , "%s"     },   // 50
   {0, 2, Indy, "EOR" , "(", "),Y", "(%s),Y" },   // 51
   {1, 2, Indi, "EOR" , "(", ")"  , "(%s)"   },   // 52
   {0, 0, None, "---" , "" , ""   , ""       },   // 53
   {0, 0, None, "---" , "" , ""   , ""       },   // 54
   {0, 2, Zpgx, "EOR" , "" , ",X" , "%s,X"   },   // 55
   {0, 2, Zpgx, "LSR" , "" , ",X" , "%s,X"   },   // 56
   {1, 2, Zpag, "RMB5", "" , ""   , "%s"     },   // 57
   {0, 1, Impl, "CLI" , "" , ""   , ""       },   // 58
   {0, 3, Absy, "EOR" , "" , ",Y" , "%s,Y"   },   // 59
   {1, 1, Impl, "PHY" , "" , ""   , ""       },   // 5a
   {0, 0, None, "---" , "" , ""   , ""       },   // 5b
   {0, 0, None, "---" , "" , ""   , ""       },   // 5c
   {0, 3, Absx, "EOR" , "" , ",X" , "%s,X"   },   // 5d
   {0, 3, Absx, "LSR" , "" , ",X" , "%s,X"   },   // 5e
   {1, 3, Zpag, "BBR5", "" , ""   , "%s"     },   // 5f

   {0, 1, Impl, "RTS" , "" , ""   , ""       },   // 60
   {0, 2, Indx, "ADC" , "(", ",X)", "(%s,X)" },   // 61
   {0, 0, None, "---" , "" , ""   , ""       },   // 62
   {0, 0, None, "---" , "" , ""   , ""       },   // 63
   {1, 2, Zpag, "STZ" , "" , ""   , "%s"     },   // 64
   {0, 2, Zpag, "ADC" , "" , ""   , "%s"     },   // 65
   {0, 2, Zpag, "ROR" , "" , ""   , "%s"     },   // 66
   {1, 2, Zpag, "RMB6", "" , ""   , "%s"     },   // 67
   {0, 1, Impl, "PLA" , "" , ""   , ""       },   // 68
   {0, 2, Imme, "ADC" , "#", ""   , "#%s"    },   // 69
   {0, 1, Accu, "ROR" , "A", ""   , "A"      },   // 6a
   {0, 0, None, "---" , "" , ""   , ""       },   // 6b
   {0, 3, Indi, "JMP" , "(", ")"  , "(%s)"   },   // 6c
   {0, 3, Abso, "ADC" , "" , ""   , "%s"     },   // 6d
   {0, 3, Abso, "ROR" , "" , ""   , "%s"     },   // 6e
   {1, 3, Zpag, "BBR6", "" , ""   , "%s"     },   // 6f

   {0, 2, Rela, "BVS" , "" , ""   , "%s"     },   // 70
   {0, 2, Indy, "ADC" , "(", "),Y", "(%s),Y" },   // 71
   {1, 2, Indi, "ADC" , "(", ")"  , "(%s)"   },   // 72
   {0, 0, None, "---" , "" , ""   , ""       },   // 73
   {1, 2, Zpgx, "STZ" , "" , ",X" , "%s,X"   },   // 74
   {0, 2, Zpgx, "ADC" , "" , ",X" , "%s,X"   },   // 75
   {0, 2, Zpgx, "ROR" , "" , ",X" , "%s,X"   },   // 76
   {1, 2, Zpag, "RMB7", "" , ""   , "%s"     },   // 77
   {0, 1, Impl, "SEI" , "" , ""   , ""       },   // 78
   {0, 3, Absy, "ADC" , "" , ",Y" , "%s,Y"   },   // 79
   {1, 1, Impl, "PLY" , "" , ""   , ""       },   // 7a
   {0, 0, None, "---" , "" , ""   , ""       },   // 7b
   {1, 3, Indx, "JMP" , "(", "),X", "(%s),X" },   // 7c
   {0, 3, Absx, "ADC" , "" , ",X" , "%s,X"   },   // 7d
   {0, 3, Absx, "ROR" , "" , ",X" , "%s,X"   },   // 7e
   {1, 3, Zpag, "BBR7", "" , ""   , "%s"     },   // 7f

   {1, 2, Rela, "BRA" , "" , ""   , "%s"     },   // 80
   {0, 2, Indx, "STA" , "(", ",X)", "(%s,X)" },   // 81
   {0, 0, None, "---" , "" , ""   , ""       },   // 82
   {0, 0, None, "---" , "" , ""   , ""       },   // 83
   {0, 2, Zpag, "STY" , "" , ""   , "%s"     },   // 84
   {0, 2, Zpag, "STA" , "" , ""   , "%s"     },   // 85
   {0, 2, Zpag, "STX" , "" , ""   , "%s"     },   // 86
   {1, 2, Zpag, "SMB0", "" , ""   , "%s"     },   // 87
   {0, 1, Impl, "DEY" , "" , ""   , ""       },   // 88
   {1, 2, Imme, "BIT" , "#", ""   , "#%s"    },   // 89
   {0, 1, Impl, "TXA" , "" , ""   , ""       },   // 8a
   {0, 0, None, "---" , "" , ""   , ""       },   // 8b
   {0, 3, Abso, "STY" , "" , ""   , "%s"     },   // 8c
   {0, 3, Abso, "STA" , "" , ""   , "%s"     },   // 8d
   {0, 3, Abso, "STX" , "" , ""   , "%s"     },   // 8e
   {1, 3, Zpag, "BBS0", "" , ""   , "%s"     },   // 8f

   {0, 2, Rela, "BCC" , "" , ""   , "%s"     },   // 90
   {0, 2, Indy, "STA" , "(", "),Y", "(%s),Y" },   // 91
   {1, 2, Indi, "STA" , "(", ")"  , "(%s)"   },   // 92
   {0, 0, None, "---" , "" , ""   , ""       },   // 93
   {0, 2, Zpgx, "STY" , "" , ",X" , "%s,X"   },   // 94
   {0, 2, Zpgx, "STA" , "" , ",X" , "%s,X"   },   // 95
   {0, 2, Zpgy, "STX" , "" , ",Y" , "%s,Y"   },   // 96
   {1, 2, Zpag, "SMB1", "" , ""   , "%s"     },   // 97
   {0, 1, Impl, "TYA" , "" , ""   , ""       },   // 98
   {0, 3, Absy, "STA" , "" , ",Y" , "%s,Y"   },   // 99
   {0, 1, Impl, "TXS" , "" , ""   , ""       },   // 9a
   {0, 0, None, "---" , "" , ""   , ""       },   // 9b
   {1, 3, Abso, "STZ" , "" , ""   , "%s"     },   // 9c
   {0, 3, Absx, "STA" , "" , ",X" , "%s,X"   },   // 9d
   {1, 3, Absx, "STZ" , "" , ",X" , "%s,X"   },   // 9e
   {1, 3, Zpag, "BBS1", "" , ""   , "%s"     },   // 9f

   {0, 2, Imme, "LDY" , "#", ""   , "#%s"    },   // a0
   {0, 2, Indx, "LDA" , "(", ",X)", "(%s,X)" },   // a1
   {0, 2, Imme, "LDX" , "#", ""   , "#%s"    },   // a2
   {0, 0, None, "---" , "" , ""   , ""       },   // a3
   {0, 2, Zpag, "LDY" , "" , ""   , "%s"     },   // a4
   {0, 2, Zpag, "LDA" , "" , ""   , "%s"     },   // a5
   {0, 2, Zpag, "LDX" , "" , ""   , "%s"     },   // a6
   {1, 2, Zpag, "SMB2", "" , ""   , "%s"     },   // a7
   {0, 1, Impl, "TAY" , "" , ""   , ""       },   // a8
   {0, 2, Imme, "LDA" , "#", ""   , "#%s"    },   // a9
   {0, 1, Impl, "TAX" , "" , ""   , ""       },   // aa
   {0, 0, None, "---" , "" , ""   , ""       },   // ab
   {0, 3, Abso, "LDY" , "" , ""   , "%s"     },   // ac
   {0, 3, Abso, "LDA" , "" , ""   , "%s"     },   // ad
   {0, 3, Abso, "LDX" , "" , ""   , "%s"     },   // ae
   {1, 3, Zpag, "BBS2", "" , ""   , "%s"     },   // af

   {0, 2, Rela, "BCS" , "" , ""   , "%s"     },   // b0
   {0, 2, Indy, "LDA" , "(", "),Y", "(%s),Y" },   // b1
   {1, 2, Indi, "LDA" , "(", ")"  , "(%s)"   },   // b2
   {0, 0, None, "---" , "" , ""   , ""       },   // b3
   {0, 2, Zpgx, "LDY" , "" , ",X" , "%s,X"   },   // b4
   {0, 2, Zpgx, "LDA" , "" , ",X" , "%s,X"   },   // b5
   {0, 2, Zpgy, "LDX" , "" , ",Y" , "%s,Y"   },   // b6
   {1, 2, Zpag, "SMB3", "" , ""   , "%s"     },   // b7
   {0, 1, Impl, "CLV" , "" , ""   , ""       },   // b8
   {0, 3, Absy, "LDA" , "" , ",Y" , "%s,Y"   },   // b9
   {0, 1, Impl, "TSX" , "" , ""   , ""       },   // ba
   {0, 0, None, "---" , "" , ""   , ""       },   // bb
   {0, 3, Absx, "LDY" , "" , ",X" , "%s,X"   },   // bc
   {0, 3, Absx, "LDA" , "" , ",X" , "%s,X"   },   // bd
   {0, 3, Absy, "LDX" , "" , ",Y" , "%s,Y"   },   // be
   {1, 3, Zpag, "BBS3", "" , ""   , "%s"     },   // bf

   {0, 2, Imme, "CPY" , "#", ""   , "#%s"    },   // c0
   {0, 2, Indx, "CMP" , "(", ",X)", "(%s,X)" },   // c1
   {0, 0, None, "---" , "" , ""   , ""       },   // c2
   {0, 0, None, "---" , "" , ""   , ""       },   // c3
   {0, 2, Zpag, "CPY" , "" , ""   , "%s"     },   // c4
   {0, 2, Zpag, "CMP" , "" , ""   , "%s"     },   // c5
   {0, 2, Zpag, "DEC" , "" , ""   , "%s"     },   // c6
   {1, 2, Zpag, "SMB4", "" , ""   , "%s"     },   // c7
   {0, 1, Impl, "INY" , "" , ""   , ""       },   // c8
   {0, 2, Imme, "CMP" , "#", ""   , "#%s"    },   // c9
   {0, 1, Impl, "DEX" , "" , ""   , ""       },   // ca
   {0, 0, None, "---" , "" , ""   , ""       },   // cb
   {0, 3, Abso, "CPY" , "" , ""   , "%s"     },   // cc
   {0, 3, Abso, "CMP" , "" , ""   , "%s"     },   // cd
   {0, 3, Abso, "DEC" , "" , ""   , "%s"     },   // ce
   {1, 3, Zpag, "BBS4", "" , ""   , "%s"     },   // cf

   {0, 2, Rela, "BNE" , "" , ""   , "%s"     },   // d0
   {0, 2, Indy, "CMP" , "(", "),Y", "(%s),Y" },   // d1
   {1, 2, Indi, "CMP" , "(", ")"  , "(%s)"   },   // d2
   {0, 0, None, "---" , "" , ""   , ""       },   // d3
   {0, 0, None, "---" , "" , ""   , ""       },   // d4
   {0, 2, Zpgx, "CMP" , "" , ",X" , "%s,X"   },   // d5
   {0, 2, Zpgx, "DEC" , "" , ",X" , "%s,X"   },   // d6
   {1, 2, Zpag, "SMB5", "" , ""   , "%s"     },   // d7
   {0, 1, Impl, "CLD" , "" , ""   , ""       },   // d8
   {0, 3, Absy, "CMP" , "" , ",Y" , "%s,Y"   },   // d9
   {1, 1, Impl, "PHX" , "" , ""   , ""       },   // da
   {0, 0, None, "---" , "" , ""   , ""       },   // db
   {0, 0, None, "---" , "" , ""   , ""       },   // dc
   {0, 3, Absx, "CMP" , "" , ",X" , "%s,X"   },   // dd
   {0, 3, Absx, "DEC" , "" , ",X" , "%s,X"   },   // de
   {1, 3, Zpag, "BBS5", "" , ""   , "%s"     },   // df

   {0, 2, Imme, "CPX" , "#", ""   , "#%s"    },   // e0
   {0, 2, Indx, "SBC" , "(", ",X)", "(%s,X)" },   // e1
   {0, 0, None, "---" , "" , ""   , ""       },   // e2
   {0, 0, None, "---" , "" , ""   , ""       },   // e3
   {0, 2, Zpag, "CPX" , "" , ""   , "%s"     },   // e4
   {0, 2, Zpag, "SBC" , "" , ""   , "%s"     },   // e5
   {0, 2, Zpag, "INC" , "" , ""   , "%s"     },   // e6
   {1, 2, Zpag, "SMB6", "" , ""   , "%s"     },   // e7
   {0, 1, Impl, "INX" , "" , ""   , ""       },   // e8
   {0, 2, Imme, "SBC" , "#", ""   , "#%s"    },   // e9
   {0, 1, Impl, "NOP" , "" , ""   , ""       },   // ea
   {0, 0, None, "---" , "" , ""   , ""       },   // eb
   {0, 3, Abso, "CPX" , "" , ""   , "%s"     },   // ec
   {0, 3, Abso, "SBC" , "" , ""   , "%s"     },   // ed
   {0, 3, Abso, "INC" , "" , ""   , "%s"     },   // ee
   {1, 3, Zpag, "BBS6", "" , ""   , "%s"     },   // ef

   {0, 2, Rela, "BEQ" , "" , ""   , "%s"     },   // f0
   {0, 2, Indy, "SBC" , "(", "),Y", "(%s),Y" },   // f1
   {1, 2, Indi, "SBC" , "(", ")"  , "(%s)"   },   // f2
   {0, 0, None, "---" , "" , ""   , ""       },   // f3
   {0, 0, None, "---" , "" , ""   , ""       },   // f4
   {0, 2, Zpgx, "SBC" , "" , ",X" , "%s,X"   },   // f5
   {0, 2, Zpgx, "INC" , "" , ",X" , "%s,X"   },   // f6
   {1, 2, Zpag, "SMB7", "" , ""   , "%s"     },   // f7
   {0, 1, Impl, "SED" , "" , ""   , ""       },   // f8
   {0, 3, Absy, "SBC" , "" , ",Y" , "%s,Y"   },   // f9
   {1, 1, Impl, "PLX" , "" , ""   , ""       },   // fa
   {0, 0, None, "---" , "" , ""   , ""       },   // fb
   {0, 0, None, "---" , "" , ""   , ""       },   // fc
   {0, 3, Absx, "SBC" , "" , ",X" , "%s,X"   },   // fd
   {0, 3, Absx, "INC" , "" , ",X" , "%s,X"   },   // fe
   {1, 3, Zpag, "BBS7", "" , ""   , "%s"     }    // ff
};

#define UNDEF 0xffff0000

int CPU_Type = 0;  // 1: 65c02
int SkipHex = 0;   // Switch on with -x
int Debug = 0;     // Switch on with -D 
int LiNo  = 0;
int ERRMAX = 10;   // Stop assemby after ERRMAX errors
int ErrNum;
int LoadAddress = UNDEF;
int WriteLoadAddress = 0;
int Petscii = 0;
int InsideMacro;
int CurrentMacro;
char *MacroPointer;

int oc;      // op code
int lo;      // 1. op byte
int hi;      // 2. op byte
int am;      // address mode
int il;      // instruction length
int pc;      // program counter
int bss;     // bss counter
int Phase;
int IfLevel;
int Skipping;
int SkipLine[10];

// Filenames

char Src[256];
char Lst[256];

int GenStart = 0x10000 ; // Lowest assemble address
int GenEnd   =       0 ; //Highest assemble address

// These arrays hold the parameter for storage files

#define SFMAX 20
int SFA[SFMAX];
int SFL[SFMAX];
char SFF[SFMAX][80];

int StoreCount = 0;

char Computer[80] = "C=64";

// The size is one page more than 64K because program, counter
// overflows are detected after using the new value.
// So references to pc + n do no harm if pc is near the boundary

unsigned char ROM[0x10100]; // binary


FILE *sf;
FILE *lf;
FILE *df;

#define ML 256

int ArgPtr[10];
char Line[ML];
char Label[ML];
char MacArgs[ML];
unsigned char Operand[ML];
char Comment[ML];
char Prefix[ML];
char Postfix[ML];

#define LDEF 1
#define LBSS 2
#define LPOS 3

#define MAXLAB 4096

struct LabelStruct
{
   char *Name;     // Label name - case sensitive
   int   Address;  // Range 0 - 65536
   int   Bytes;    // Length of object (string for example)
   int   Paired;   // Used in indirect addressing mode
   int   Locked;   // Defined from command line argument
   int   NumRef;   // # of references
   int  *Ref;      // list of references
   int  *Att;      // list of attributes
} lab[MAXLAB];

int Labels;

#define MAXMAC 64

struct MacroStruct
{
   char *Name;  // MACRO Name(arg,arg,...) (up to 10 arguments)
   char *Body;  // "Line1\nLine2\n ... LastLine\n"
   int  Narg;  // # of macro arguments (0-10)
   int  Cola;  // column of macro definition (for pretty printing)
} Mac[MAXMAC];

int Macros;


char *SkipSpace(char *p)
{
   if (*p) while (isspace(*p)) ++p;
   return p;
}


int isym(char c)
{
   return (c == '_' || isalnum(c));
}


char *GetSymbol(char *p, char *s)
{
   while (isym(*p)) *s++ = *p++;
   *s = 0;
   return p;
}


char *NextSymbol(char *p, char *s)
{
   p = SkipSpace(p);
   p = GetSymbol(p,s);
   return p;
}
   

char *SkipHexCode(char *p)
{
   int l;

   l = strlen(p);
   if (l > 20 && isdigit(p[4]) && isspace(p[5]) &&
       isxdigit(p[6]) && isxdigit(p[7]) && 
       isxdigit(p[8]) && isxdigit(p[9]) &&
       p[0] != ';')
   {
      if (SkipHex) memmove(Line,Line+20,l-20);
      else return (p+20);
   }
   return p;
}


void ErrorLine(char *p)
{
   int i,ep;
   printf("%s\n",Line);
   ep = p - Line;
   for (i=0 ; i < ep ; ++i) printf(" ");
   printf("^\n");
}


int IsInstruction(char *p)
{
   int i;

   // Check for 6502 & 65c02 3-letter mnemonics

   if (isalpha(p[0]) && isalpha(p[1]) && isalpha(p[2]) && !isym(p[3]))
   for (i=0 ; i < 256 ; ++i)
   {
      if (!strncasecmp(p,set[i].mne,3))
      {
         if (set[i].cpu > CPU_Type && set[i].amo == Impl)
         {
            ErrorLine(p);
            printf("Found 65c02 instruction in 6502 mode\n");
            printf("Include:  CPU = 65c02 to enable 65c02 mode\n");
            exit(1);
         }
         return i; // Is instruction!
      }
   }
   
   // Check for 65c02 4-letter mnemonics

   if (CPU_Type > 0 &&
       isalpha(p[0]) && isalpha(p[1]) && isalpha(p[2]) && 
       (p[3] >= '0' && p[3] <= '7') && !isym(p[4]))
   for (i=0 ; i < 256 ; ++i)
   {
      if (!strncasecmp(p,set[i].mne,4) && set[i].cpu <= CPU_Type)
         return i; // Is instruction!
   }
   
   return -1; // No mnemonic
}


char *NeedChar(char *p, char c)
{
   return strchr(p,c);
}


char *EvalAddress(char *p, int *a)
{
   p=SkipSpace(p);
   if (*p == '$')
   {
      sscanf(++p,"%x",a);
      while (isxdigit(*p)) ++p; 
      p = SkipSpace(p);
   }
   if (*a < 0 || *a > 0xffff)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Address %x out of range\n",*a);
   }
   return p;
}

char *EvalOperand(char *, int *, int);

char *SetPC(char *p)
{
   if (*p == '*')
   {
      p = NeedChar(p,'=');
      if (!p)
      {
         ++ErrNum;
         printf("\n*** Error line %d: ",LiNo);
         printf("Missing '=' in set pc * instruction\n");
         exit(1);
      }
   }
   else p += 3; // .ORG syntax
   p = EvalOperand(p+1,&pc,0);
   if (LoadAddress < 0) LoadAddress = pc;
   if (Phase == 2)
   {
      fprintf(lf,"%5d %4.4x          %s\n",LiNo,pc,Line);
   }
   if (GenStart > pc) GenStart = pc; // remember lowest pc value
   return p;
}

char *SetBSS(char *p)
{
   p = NeedChar(p,'=');
   if (!p)
   {
      ++ErrNum;
      printf("\n*** Error line %d: ",LiNo);
      printf("Missing '=' in set BSS & instruction\n");
      exit(1);
   }
   p = EvalOperand(p+1,&bss,0);
   if (df) fprintf(df,"BSS = %4.4x\n",bss);
   if (Phase == 2)
   {
      fprintf(lf,"%5d %4.4x          %s\n",LiNo,bss,Line);
   }
   return p;
}


int LabelIndex(char *p)
{
   int i;

   for (i = 0 ; i < Labels ; ++i)
   {
      if (!strcmp(p,lab[i].Name)) return i;
   }
   return -1;
}


int AddressIndex(int a)
{
   int i;

   for (i = 0 ; i < Labels ; ++i)
   {
      if (lab[i].Address == a) return i;
   }
   return -1;
}


int MacroIndex(char *p)
{
   int i,l;

   for (i = 0 ; i < Macros ; ++i)
   {
      l = strlen(Mac[i].Name);
      if (!strncmp(p,Mac[i].Name,l) && !isym(p[l])) return i;
   }
   return -1;
}



char *DefineLabel(char *p, int *val, int Locked)
{
   int j,l,v;

   if (Labels > MAXLAB -2)
   {
      ++ErrNum;
      printf("\n*** Error line %d: ",LiNo);
      printf("Too many labels (> %d)\n",MAXLAB);
      exit(1);
   }
   p = GetSymbol(p,Label);
   l = strlen(Label);
   p = SkipSpace(p);
   if (*p == '=') 
   {
      j = LabelIndex(Label);
      if (j < 0)
      {
         j = Labels;
         lab[j].Name = malloc(l+1);
         strcpy(lab[j].Name,Label);
         lab[j].Address = UNDEF;
         lab[j].Ref = malloc(sizeof(int));
         lab[j].Att = malloc(sizeof(int));
         Labels++;
      }
      lab[j].Ref[0] = LiNo;
      lab[j].Att[0] = LDEF;
      p = EvalOperand(p+1,&v,0);
      if (lab[j].Address == UNDEF) lab[j].Address = v;
      else if (lab[j].Address != v && !lab[j].Locked)
      {
         ++ErrNum;
         printf("\n*** Error line %d:\n",LiNo);
         ErrorLine(p);
         printf("*Multiple assignments for label [%s]\n",Label);
         printf("1st. value = $%4.4x   2nd. value = $%4.4x\n",lab[j].Address,v);
         exit(1);
      }
      *val = v;
      if (Locked) lab[j].Locked = Locked;
   }
   else if (!strncasecmp(p,".BSS",4))
   {
      p = EvalOperand(p+4,&v,0);
      j = LabelIndex(Label);
      if (j < 0)
      {
         j = Labels;
         lab[j].Name = malloc(l+1);
         strcpy(lab[j].Name,Label);
         lab[j].Address = UNDEF;
         lab[j].Ref = malloc(sizeof(int));
         lab[j].Att = malloc(sizeof(int));
         Labels++;
      }
      lab[j].Ref[0] = LiNo;
      lab[j].Att[0] = LBSS;
      if (lab[j].Address == UNDEF) lab[j].Address = bss;
      else if (lab[j].Address != bss)
      {
         ++ErrNum;
         printf("\n*** Error line %d:\n",LiNo);
         ErrorLine(p);
         printf("Multiple assignments for label [%s]\n",Label);
         printf("1st. value = $%4.4x   2nd. value = $%4.4x\n",lab[j].Address,bss);
         exit(1);
      }
      *val = bss;
      bss += v;
   }
   else
   {
      j = LabelIndex(Label);
      if (j < 0)
      {
         j = Labels;
         lab[j].Name = malloc(l+1);
         strcpy(lab[j].Name,Label);
         lab[j].Address = pc;
         lab[j].Ref = malloc(sizeof(int));
         lab[j].Att = malloc(sizeof(int));
         Labels++;
      }
      else if (lab[j].Address < 0) lab[j].Address = pc;
      else if (lab[j].Address != pc && !lab[j].Locked)
      {
         ++ErrNum;
         if (Phase == 1)
         {
            printf("\n*** Error line %d: ",LiNo);
            printf("Multiple label definition [%s]",Label);
            printf(" value 1: %4.4x   value 2: %4.4x\n",lab[j].Address,pc);
         }
         else
         {
            printf("\n*** Error line %d: ",LiNo);
            printf("Phase error label [%s]",Label);
            printf(" phase 1: %4.4x   phase 2: %4.4x\n",lab[j].Address,pc);
         }
         exit(1);
      }
      if (!lab[j].Locked) *val = pc;
      lab[j].Ref[0] = LiNo;
      lab[j].Att[0] = LPOS;
   }
   return p;
}


void AddLabel(char *p)
{
   int l;

   if (Labels > MAXLAB -2)
   {
      ++ErrNum;
      printf("\n*** Error line %d: ",LiNo);
      printf("Too many labels (> %d)\n",MAXLAB);
      exit(1);
   }
   l = strlen(p);
   lab[Labels].Address = UNDEF;
   lab[Labels].Name = malloc(l+1);
   strcpy(lab[Labels].Name,p);
   lab[Labels].Ref = malloc(sizeof(int));
   lab[Labels].Att = malloc(sizeof(int));
   lab[Labels].Ref[0] = LiNo;
   lab[Labels].Att[0] = 0;
   Labels++;
}


void SymRefs(int i)
{
   int n;

   if (Phase != 2) return;
   n = ++lab[i].NumRef;
   lab[i].Ref = realloc(lab[i].Ref,(n+1)*sizeof(int));
   lab[i].Ref[n] = LiNo;
   lab[i].Att = realloc(lab[i].Att,(n+1)*sizeof(int));
   lab[i].Att[n] = am;
}


char *EvalSymValue(char *p, int *v)
{
   int i;
   char Sym[ML];

   p = GetSymbol(p,Sym);
   for (i=0 ; i < Labels ; ++i)
   {
      if (!strcmp(Sym,lab[i].Name))
      {
         *v = lab[i].Address;
         SymRefs(i);
         return p;
      }
   }
   AddLabel(Sym);
   *v = UNDEF;
   return p;
}


char *EvalSymBytes(char *p, int *v)
{
   int i;
   char Sym[ML];

   p = GetSymbol(p,Sym);
   for (i=0 ; i < Labels ; ++i)
   {
      if (!strcmp(Sym,lab[i].Name))
      {
         *v = lab[i].Bytes;
         SymRefs(i);
         return p;
      }
   }
   AddLabel(Sym);
   *v = UNDEF;
   return p;
}


char *ParseLongData(char *p, int l)
{
   int i,v;
   union ulb
   {
     int l;
     unsigned char b[4];
   } lb;

   p = SkipSpace(p);
   if (*p == '$')
   {
      ++p;
      for (i=0 ; i < l ; ++i, p+=2)
      {
          sscanf(p,"%2x",&v);
          Operand[i] = v;
      }
   }
   else
   {
      lb.l = atoi(p);
      for (i=0 ; i < 4 ; ++i) Operand[i] = lb.b[3-i];
   }
   if (Phase == 2)
   {
      for (i=0 ; i < l ; ++i) ROM[pc+i] = Operand[i];
      fprintf(lf,"%5d %4.4x %2.2x %2.2x %2.2x %s\n",
         LiNo,pc,Operand[0],Operand[1],Operand[2],Line);
   }
   pc += l;
   return p;
}


char *ParseRealData(char *p)
{
   int i,v;
   int Sign,Exponent;
   union udb
   {
     double d;
     unsigned char b[8];
   } db;

   p = SkipSpace(p);

   if (*p == '$')
   {
      ++p;
      for (i=0 ; i < 5 ; ++i, p+=2)
      {
          sscanf(p,"%2x",&v);
          Operand[i] = v;
      }
   }
   else
   {
      db.d = atof(p);
      Sign = db.b[7] & 0x80;
      Exponent = (((db.b[7] & 0x7f) << 4) | (db.b[6] >> 4)) - 0x3ff + 0x81;
   
      Operand[0] = Exponent;
      Operand[1] = ((db.b[6] & 0x0f) << 3) | (db.b[5] >> 5) | Sign;
      Operand[2] = ((db.b[5] & 0x1f) << 3) | (db.b[4] >> 5);
      Operand[3] = ((db.b[4] & 0x1f) << 3) | (db.b[3] >> 5);
      Operand[4] = ((db.b[3] & 0x1f) << 3) | (db.b[2] >> 5);
   
      if (db.d == 0.0) for (i=0 ; i < 8 ; ++i) Operand[i] = 0;
   }

   if (Phase == 2)
   {
      for (i=0 ; i < 5 ; ++i) ROM[pc+i] = Operand[i];
      fprintf(lf,"%5d %4.4x %2.2x %2.2x %2.2x %s\n",
         LiNo,pc,Operand[0],Operand[1],Operand[2],Line);
   }
   pc += 5;
   return p;
}


char *EvalDecValue(char *p, int *v)
{
   *v = atoi(p);
   while (isdigit(*p)) ++p;
   return p;
}


char *EvalCharValue(char *p, int *v)
{
   *v = *p++;
   if (*p != '\'')
   {
      ++ErrNum;
      printf("\n*** Error line %d: ",LiNo);
      printf("Missing ' delimiter after character operand\n");
      exit(1);
   }
   return p+1;
}


char *EvalHexValue(char *p, int *v)
{
   sscanf(p,"%x",v);
   while (isxdigit(*p)) ++p;
   return p;
}


char *EvalBinValue(char *p, int *v)
{
   int r;
   r = 0;
   while(*p == ' ' || *p == '1' || *p == '0' || *p == '*' || *p == '.')
   {
      if (*p == '1' || *p == '*') r = (r << 1) + 1;
      if (*p == '0' || *p == '.') r <<= 1;
      ++p;
   }
   *v = r;
   return p;
}


char *SkipToComma(char *p)
{
   while (*p && *p != ',' && *p != ';') ++p;
   return p;
}


int OperandExists(char *p)
{
   while (*p == ' ') ++p;
   return (*p != ';' && *p != 0);
}


char *EvalOperand(char *p, int *v, int prio)
{
   int w,lo,hi;

   lo=hi=0;
   *v = UNDEF;

   p = SkipSpace(p);
   if (df) fprintf(df,"EvalOperand <%s>\n",p);

   if (*p == '<')
   {
      lo = 1;
      ++p;
   }
   else if (*p == '>')
   {
      hi = 1;
      ++p;
   }
   if (*p == '*')
   {
      *v = pc;
      ++p;
   }
   else if (*p == '[')
   {
      if (df) fprintf(df,"enter %s\n",p);
      p = EvalOperand(p+1,v,0);
      if (df) fprintf(df,"Eval: $%4.4x %d\n",*v,*v);
      p = NeedChar(p,']');
      if (df) fprintf(df,"] exit\n");
      if (!p)
      {
         printf("\n*** Error line %d:\n",LiNo);
         ErrorLine(p);
         printf("Missing right bracket ]\n");
         exit(1);
      }
      p++;
   }
   else if (*p == '-') // negative sign
   {
      p = EvalOperand(p+1,&w,15);
      if (w != UNDEF) *v = -w;
   }
   else if (*p == '!')
   {
      p = EvalOperand(p+1,&w,15);
      if (w != UNDEF) *v = !w;
   }
   else if (*p == '~')
   {
      p = EvalOperand(p+1,&w,15);
      if (w != UNDEF) *v = ~w;
   }
   else if (*p == '\'')  p = EvalCharValue(p+1,v);
   else if (*p == '$' )  p = EvalHexValue(p+1,v);
   else if (*p == '%' )  p = EvalBinValue(p+1,v);
   else if (*p == '?' )  p = EvalSymBytes(p+1,v);
   else if (*p == ',' )  return p;
   else if (isdigit(*p)) p = EvalDecValue(p,v);
   else if (isym(*p))    p = EvalSymValue(p,v);
   else
   {
      printf("\n*** Error line %d:\n",LiNo);
      ErrorLine(p);
      printf("Illegal operand\n");
      exit(1);
   }
   if (*v != UNDEF && lo) *v &= 0xff;
   if (*v != UNDEF && hi) *v >>= 8;
   
   p = SkipSpace(p);
   if (*p == '*')
   {
      if (prio > 10) return p;
      p = EvalOperand(p+1,&w,10);
      if (*v != UNDEF) *v *= w;
   }
   if (*p == '/')
   {
      if (prio > 10) return p;
      p = EvalOperand(p+1,&w,10);
      if (*v != UNDEF) *v /= w;
   }
   if (*p == '+')
   {
      if (prio > 5) return p;
      p = EvalOperand(p+1,&w,5);
      if (*v != UNDEF) *v += w;
   }
   if (*p == '-')
   {
      if (prio > 5) return p;
      p = EvalOperand(p,&w,5);
      if (*v != UNDEF) *v += w;
   }
   if (*p == '&')
   {
      p = EvalOperand(p+1,&w,0);
      if (*v != UNDEF) *v &= w;
   }
   if (*p == '|')
   {
      p = EvalOperand(p+1,&w,0);
      if (*v != UNDEF) *v |= w;
   }
   return p;
}


char *ParseWordData(char *p)
{
   int v,lo,hi;

   p = EvalOperand(p,&v,0);
   if (Phase == 2)
   {
      lo = v & 0xff;
      hi = v >> 8;
      ROM[pc  ] = lo;
      ROM[pc+1] = hi;
      fprintf(lf,"%5d %4.4x %2.2x %2.2x    %s\n",LiNo,pc,lo,hi,Line);
   }
   pc += 2;
   return p;
}


char *ParseFillData(char *p)
{
   int i,m,v;

   p = EvalOperand(p,&m,0);
   if (m < 1 || m > 32767)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Illegal FILL multiplier %d\n",m);
      exit(1);
   }
   p = NeedChar(p,'(');
   if (!p)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Missing '(' before FILL value\n");
      exit(1);
   }
   p = EvalOperand(p+1,&v,0);
   v &= 0xff;
   if (Phase == 2)
   {
      for (i=0 ; i < m ; ++i) ROM[pc+i] = v;
      fprintf(lf,"%5d %4.4x %2.2x ", LiNo,pc,v);
      if (m > 1) fprintf(lf,"%2.2x ",v);
      else       fprintf(lf,"   ");
      if (m > 2) fprintf(lf,"%2.2x ",v);
      else       fprintf(lf,"   ");
      fprintf(lf,"%s ; %d bytes\n",Line,m);
   }
   pc += m;
   return p;
}


char *ParseStoreData(char *p)
{
   int Start,Length,i;
   char Filename[80];

   if (Phase < 2) return p;
   p = EvalOperand(p,&Start,0);
   if (Start < 0 || Start > 0xffff)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Illegal start address for STORE %d\n",Start);
      exit(1);
   }
   p = NeedChar(p,',');
   if (!p)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Missing ',' after start address\n");
      exit(1);
   }
   p = EvalOperand(p+1,&Length,0);
   if (Length < 0 || Length > 0x10000)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Illegal length for STORE %d\n",Start);
      exit(1);
   }
   p = NeedChar(p,',');
   if (!p)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Missing ',' after length\n");
      exit(1);
   }
   p = NeedChar(p+1,'"');
   if (!p)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Missing quote for filename\n");
      exit(1);
   }
   ++p;
   i = 0;
   while (*p && *p != '"' && i < 80) Filename[i++] = *p++;
   Filename[i] = 0;
   SFA[StoreCount] = Start;
   SFL[StoreCount] = Length;
   strcpy(SFF[StoreCount],Filename);
   if (df) fprintf(df,"Storing %4.4x - %4.4x <%s>\n",Start,Start+Length-1,Filename);
   if (StoreCount < SFMAX) ++StoreCount;
   else
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("number of storage files exceeds %d\n",SFMAX);
      exit(1);
   }
   if (Phase == 2)
   {
      fprintf(lf,"%5d %4.4x        ", LiNo,pc);
      fprintf(lf,"%s\n",Line);
   }
   return p;
}


char *ParseBSSData(char *p)
{
   int m;

   p = EvalOperand(p,&m,0);
   if (m < 1 || m > 32767)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Illegal BSS size %d\n",m);
      exit(1);
   }
   if (Phase == 2)
   {
      fprintf(lf,"%5d %4.4x             ", LiNo,bss);
      fprintf(lf,"%s\n",Line);
   }
   bss += m;
   return p;
}


char *ParseByteData(char *p)
{
   int i,j,l,v;
   unsigned char ByteBuffer[ML];

   l = 0;
   while (*p && *p != ';') // Parse data line
   {
      p = SkipSpace(p);
      if (*p == '"') // ASCII code
      {
         ++p;
         while (*p != '"' && l < sizeof(ByteBuffer)-1)
         {
            if (*p == '\\')
            {
               ++p;
                    if (*p == 'r') ByteBuffer[l] = 13;
               else if (*p == 'n') ByteBuffer[l] = 10;
               else if (*p == '0') ByteBuffer[l] =  0;
               else ByteBuffer[l] = *p;
               ++l;
               ++p;
            }
            else
            {
               if (Petscii && *p >= 'a' && *p <= 'z') ByteBuffer[l++] = *p++ + 0x60;
               else ByteBuffer[l++] = *p++;
            }
         }
         ++p; // Skip quote
         if (*p == '^') ByteBuffer[l-1] |= 0x80; // Set bit 7 of last char
      }
      if (*p == '\'') // PETSCII code
      {
         ++p;
         while (*p != '\'' && l < sizeof(ByteBuffer)-1)
         {
            if (*p == '\\')
            {
               ++p;
                    if (*p == 'r') ByteBuffer[l] = 13;
               else if (*p == 'n') ByteBuffer[l] = 10;
               else if (*p == '0') ByteBuffer[l] =  0;
               else ByteBuffer[l] = *p;
               ++l;
               ++p;
            }
            else
            {
                    if (*p >= 'a' && *p <= 'z') ByteBuffer[l++] = *p++ - 0x20;
               else if (*p >= 'A' && *p <= 'Z') ByteBuffer[l++] = *p++ + 0x80;
               else ByteBuffer[l++] = *p++;
            }
         }
         ++p; // Skip quote
         if (*p == '^') ByteBuffer[l-1] |= 0x80; // Set bit 7 of last char
      }
      else if (isdigit(*p))
      {
         p = EvalDecValue(p,&v);
         ByteBuffer[l++] = v & 0xff;
         if (v > 255 || v < -127) ByteBuffer[l++] = v >> 8;
      }
      else if (*p == '$')
      {
         p = EvalHexValue(p+1,&v);
         ByteBuffer[l++] = v & 0xff;
         if (v > 255 || v < -127) ByteBuffer[l++] = v >> 8;
      }
      else if (*p == '<')
      {
         p = EvalOperand(p,&v,0);
         ByteBuffer[l++] = v;
      }
      else if (*p == '>')
      {
         p = EvalOperand(p,&v,0);
         ByteBuffer[l++] = v;
      }
      else if (isalpha(*p))
      {
         p = EvalOperand(p,&v,0);
         ByteBuffer[l++] = v & 0xff;
         if (v > 255 || v < 0) ByteBuffer[l++] = v >> 8;
      }
      p = SkipToComma(p);
      if (*p == ',') ++p;
   }
   if (l < 1)
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Missing byte data\n");
      ErrorLine(p);
      exit(1);
   }
   j = AddressIndex(pc);
   if (j >= 0) lab[j].Bytes = l;
   if (j >= 0 && df) fprintf(df,"Byte label [%s] $%4.4x $%4.4x %d bytes\n",
                   lab[j].Name,lab[j].Address,pc,l);
   if (Phase == 2)
   {
      fprintf(lf,"%5d %4.4x ",LiNo,pc);
      for (i=0 ; i < l ; ++i)
      {
         ROM[pc+i] = ByteBuffer[i];
         if (i < 3) fprintf(lf,"%2.2x ",ByteBuffer[i]);
      }
      for (i=l ; i < 3 ; ++i) fprintf(lf,"   ");
      fprintf(lf,"%s\n",Line);
   }
   pc += l;
   return p;
}


char *IsData(char *p)
{
   p = SkipSpace(p+1);
   if (!strncasecmp(p,"WORD",4))
   {
      p = ParseWordData(p+4);
   }
   else if (!strncasecmp(p,"BYTE",4))
   {
      p = ParseByteData(p+4);
   }
   else if (!strncasecmp(p,"QUAD",4))
   {
      p = ParseLongData(p+4,4);
   }
   else if (!strncasecmp(p,"REAL",4))
   {
      p = ParseRealData(p+4);
   }
   else if (!strncasecmp(p,"FILL",4))
   {
      p = ParseFillData(p+4);
   }
   else if (!strncasecmp(p,"BSS",3))
   {
      p = ParseBSSData(p+4);
   }
   else if (!strncasecmp(p,"ORG",3))
   {
      p = SetPC(p);    // Set program counter
   }
   else if (!strncasecmp(p,"LOAD",4))
   {
      WriteLoadAddress = 1;
   }
   else if (!strncasecmp(p,"STORE",5))
   {
      p = ParseStoreData(p+5);
   }
   else
   {
      printf("\n*** Error line %d: ",LiNo);
      printf("Unknown pseudo op\n");
      ErrorLine(p);
      exit(1);
   }
   return p;
}

 
char * SplitOperand(char *p)
{
   int l,lpar,rpar;

   l    = 0;
   il   = 0;
   lpar = 0;
   rpar = 0;
   am   = -1;
   Prefix[0]  = 0;
   Postfix[0] = 0;
   Operand[0] = 0;
   p = SkipSpace(p);  

   // Prefix

   if (*p == '#')
   {
      strcpy(Prefix,"#");
      am = Imme;
      il = 2;
      ++p;
   }
   else if ((*p == 'A' || *p == 'a') && !isym(p[1]))
   {
      strcpy(Prefix,"A");
      am = Accu;
      il = 1;
      ++p;
   }
   else if (*p == '(')
   {
      lpar = 1;
      strcpy(Prefix,"(");
      ++p;
   }
   
   // Operand

   p = SkipSpace(p);
   while (*p && *p != ';') Operand[l++] = *p++;
   Operand[l] = 0;
   while (l && isspace(Operand[l-1])) Operand[--l] = 0;
   if (am < 0 && Operand[0])
   {
      am = Abso;
      il = 3;
   }

   // Postfix

   if (l > 4 && !strcasecmp((const char *)(Operand+l-3),",X)"))
   {
      rpar = 1;
      strcpy(Postfix,",X)");
      am = Indx;
      il = 2;
      Operand[l-=3] = 0;
   }
   else if (l > 4 && !strcasecmp((const char *)Operand+l-3,"),Y"))
   {
      rpar = 1;
      strcpy(Postfix,"),Y");
      am = Indy;
      il = 2;
      Operand[l-=3] = 0;
   }
   else if (l > 2 && Operand[l-1] == ')')
   {
      rpar = 1;
      strcpy(Postfix,")");
      am = Indi;
      il = 3;
      Operand[--l] = 0;
   }
   else if (l > 2 && !strcasecmp((const char *)Operand+l-2,",X"))
   {
      strcpy(Postfix,",X");
      am = Absx;
      Operand[l-=2] = 0;
   }
   else if (l > 2 && !strcasecmp((const char *)Operand+l-2,",Y"))
   {
      strcpy(Postfix,",Y");
      am = Absy;
      Operand[l-=2] = 0;
   }
   if (lpar != rpar)
   {
      printf("\n*** Error line %d:\n",LiNo);
      ErrorLine(p);
      printf("Address mode syntax error\n");
      exit(1);
   }
   if (df) fprintf(df,"Split {%s} {%s} {%s}\n",Prefix,Operand,Postfix);
   return p;
}


void AdjustOpcode(char *p)
{
   int i;

   if (oc == 0x20) am = Abso; // JSR
   if (oc == 0x4c && am == Zpag) am = Abso; // JMP
   if (oc != 0x86 && oc != 0xa2 && am == Zpgy) am = Absy; // Only LDX/STX
   for (i=0 ; i < 256 ; ++i)
   {
      if (!strcmp(set[oc].mne,set[i].mne) && am == set[i].amo)
      {
         oc = i;
         il = set[i].len;
         return;
      }
   }
   printf("\n*** Error line %d: ",LiNo);
   printf("Illegal address mode %d for %s\n",am,set[oc].mne);
   ErrorLine(p);
   exit(1);
}


void CheckSkip(void)
{
   int i;

   Skipping = 0;
   for (i=1 ; i <= IfLevel ; ++i) Skipping |= SkipLine[i];
}

int CheckCondition(char *p)
{
   int r,v;
   r = 0;
   if (*p == '#') ++p; // Allow CPP syntax
   if (!strncasecmp(p,"if ",3))
   {
      r = 1;
      IfLevel++;
      if (IfLevel > 9)
      {
         ++ErrNum;
         printf("\n*** Error line %d: ",LiNo);
         printf("Too many IF's nested\n");
         exit(1);
      }
      p = EvalOperand(p+3,&v,0);
      SkipLine[IfLevel] = !v;
      CheckSkip();
      if (Phase == 2)
      {
         if (SkipLine[IfLevel])
            fprintf(lf,"%5d %4.4x FALSE    %s\n",LiNo,SkipLine[IfLevel],Line);
         else
            fprintf(lf,"%5d 0000 TRUE     %s\n",LiNo,Line);
      }
      if (df) fprintf(df,"%5d %4.4x          %s\n",LiNo,SkipLine[IfLevel],Line);
   }
   else if (!strncasecmp(p,"else",4) && (p[4] == 0 || isspace(p[4])))
   {
      r = 1;
      SkipLine[IfLevel] = !SkipLine[IfLevel];
      CheckSkip();
      if (Phase == 2) fprintf(lf,"%5d               %s\n",LiNo,Line);
   }
   if (!strncasecmp(p,"endif",5) && (p[5] == 0 || isspace(p[5])))
   {
      r = 1;
      IfLevel--;
      if (Phase == 2) fprintf(lf,"%5d               %s\n",LiNo,Line);
      if (IfLevel < 0)
      {
         ++ErrNum;
         printf("\n*** Error line %d: ",LiNo);
         printf("endif without if\n");
         exit(1);
      }
      CheckSkip();
      if (df) fprintf(df,"ENDIF SkipLevel[%d]=%d\n",IfLevel,SkipLine[IfLevel]);
   }
   return r;
}


char *GenerateCode(char *p)
{
   int v,lo,hi;
   char *o;

   o = (char *)Operand;
   p += strlen(set[oc].mne) ; // Skip mnemonic
   p = SkipSpace(p);
   if (set[oc].amo == Impl)
   {
      am = Impl;
      il = 1;
      if (OperandExists(p))
      {
         printf("\n*** Error line %d:\n",LiNo);
         ErrorLine(p);
         printf("Implied address mode must not have operands\n");
         exit(1);
      }
   }
   else
   {
      p = SplitOperand(p);
   }
   if (Operand[0])
   {
      o = EvalOperand((char *)Operand,&v,0);
      if (am == Imme)
      {
         if (v > -128 && v < 256) 
         {
            il = 2;
            am = Imme;
         }
         else if (Phase == 2)
         {
            printf("\n*** Error line %d:\n",LiNo);
            ErrorLine(p);
            printf("Immediate value out of range (%d)\n",v);
            exit(1);
         }
      }
      else if (set[oc].amo == Rela)
      {
         il = 2;
         am = Rela;
         if (v != UNDEF) v  -= (pc + 2);
         if (Phase == 2 && v == UNDEF)
         {
            printf("\n*** Error line %d:\n",LiNo);
            ErrorLine(p);
            printf("Branch to undefined label\n");
            exit(1);
         }
         if (Phase == 2 && (v < -128 || v > 127))
         {
            printf("\n*** Error line %d:\n",LiNo);
            ErrorLine(p);
            printf("Branch too long (%d)\n",v);
            exit(1);
         }
      }
      else if (v >= 0 && v < 256 && am >= Abso && am <= Absy)
      {
         il = 2;
         am -= 3; // Abso -> Zpag, Absx -> Zpgx, Absy -> Zpgy
      }
   }  
   else if (am != Impl && am != Accu)
   {
      printf("\n*** Error line %d:\n",LiNo);
      ErrorLine(p);
      printf("Operand missing\n");
      exit(1);
   }
   if (*o && *o != ';') 
   {
      printf("\n*** Error line %d:\n",LiNo);
      ErrorLine(p);
      printf("Operand syntax error\n");
      printf("<%s>\n",o);
      exit(1);
   }
   AdjustOpcode(p);
   if (Phase == 2)
   {
      if (v == UNDEF)
      {
         printf("\n*** Error line %d:\n",LiNo);
         ErrorLine(p);
         printf("Use of an undefined label\n");
         exit(1);
      }

      lo = v & 0xff;
      hi = v >> 8;

                  ROM[pc  ] = oc;
      if (il > 1) ROM[pc+1] = lo;
      if (il > 2) ROM[pc+2] = hi;

      fprintf(lf,"%5d %4.4x %2.2x ",LiNo,pc,oc);
      if (il > 1) fprintf(lf,"%2.2x ",lo);
      else        fprintf(lf,"   ");
      if (il > 2) fprintf(lf,"%2.2x ",hi);
      else        fprintf(lf,"   ");
      fprintf(lf,"%s\n",Line);
   }
   if (il < 1 || il > 3)
   {
      ++ErrNum;
      printf("\n*** Error line %d: ",LiNo);
      printf("Wrong instruction length = %d\n",il);
      il = 1;
   }
   if (pc+il > 0xffff)
   {
      if (Phase > 1)
      {
         ++ErrNum;
         printf("\n*** Error line %d: ",LiNo);
         printf("Program counter exceeds 64 KB\n");
      }
   }
   else pc += il;
   return p;
}


// Called after '(' returns # of args

int ScanArguments(char *p, char *args, int ptr[])
{
   int l,n;
   char sym[ML];

   n = 0;
   ptr[0] = 0;
   while (*p && n < 10)
   {
      p = SkipSpace(p);
      if (*p == ')') break; // end of list
      p = NextSymbol(p,sym);
      l = strlen(sym);
      if (l) strcpy(args+ptr[n],sym);
      else   args[ptr[n]] = 0;
      ++n;
      ptr[n] = ptr[n-1] + l + 1;
      p = SkipSpace(p);
      if (*p == ')') break; // end of list
      if (*p != ',')
      {
         ++ErrNum;
         printf("\n*** Error line %d: ",LiNo);
         printf("Syntax error in macro definition '%c'\n",*p);
         exit(1);
      }
      ++p; // skip comma
   }
   return n;
}


void RecordMacro(char *p)
{
   char Macro[ML];
   int i,j,l,al,an,bl;
   int ap[10];
   char args[ML];
   char Buf[ML];
   char *b;
   char *at;

   if (Macros > MAXMAC -2)
   {
      ++ErrNum;
      printf("\n*** Error line %d: ",LiNo);
      printf("Too many macros (> %d)\n",MAXMAC);
      exit(1);
   }
   bl = 1;
   p = NextSymbol(p,Macro);
   l = strlen(Macro);
   p = SkipSpace(p);
   if (*p == '(') 
   {
      an = ScanArguments(p+1,args,ap);
      if (df)
      {
         fprintf(df,"RecordMacro: %s(",Macro);
         for (i=0 ; i < an ; ++i)
         {
            at = args + ap[i];
            al = strlen(at);
            fprintf(df,"%s[%d]",at,al);
            if (i < an-1) fprintf(df,",");
         }
         fprintf(df,")\n");
      }
      j = MacroIndex(Macro);
      if (j < 0)
      {
         j = Macros;
         Mac[j].Name = malloc(l+1);
         strcpy(Mac[j].Name,Macro);
         Mac[j].Narg = an;
         fgets(Line,sizeof(Line),sf);
         while (!feof(sf) && !Strcasestr(Line,"ENDMAC"))
         {
            ++LiNo;
            l = strlen(Line);
            if (l && Line[l-1] == 10) Line[--l] = 0; // Remove linefeed
            if (l && Line[l-1] == 13) Line[--l] = 0; // Remove return
            p = Line;
            b = Buf;
            while (*p)
            {
               for (i=0 ; i < an ; ++i)
               {
                  at = args + ap[i];
                  al = strlen(at);
                  if (al && !strncmp(p,at,al))
                  {
                     *b++ = '&';
                     *b++ = '0' + i;
                     p += al;
                     break;
                  }
               }
               if (i == an) *b++ = *p++;
            }
            *b++ = '\n';
            *b = 0;
            l = strlen(Buf);
            if (bl == 1)
            {
               bl = l+1;
               Mac[j].Body = malloc(bl);
               strcpy(Mac[j].Body,Buf);
            }
            else
            {
               bl += l;
               Mac[j].Body = realloc(Mac[j].Body,bl);
               strcat(Mac[j].Body,Buf);
            }
            fgets(Line,sizeof(Line),sf);
         }
         Macros++;
      }
      else if (Phase == 2) // List macro
      {
         fprintf(lf,"%5d               %s\n",LiNo++,Line);
         do
         {
            fgets(Line,sizeof(Line),sf);
            fprintf(lf,"%5d               %s",LiNo++,Line);
         } while (!feof(sf) && !Strcasestr(Line,"ENDMAC"));
         LiNo-=2;
      }
      else if (Phase == 1)
      {
         ++ErrNum;
         printf("\n*** Error line %d: ",LiNo);
         printf("Duplicate macel [%s]\n",Macro);
         exit(1);
      }
      if (df) fprintf(df,"Macro [%s] = %s\n",Mac[j].Name,Mac[j].Body);
   }
   ++LiNo;
}


int ExpandMacro(char *m)
{
   int j,an;
   char *p;
   char Macro[ML];

   j = MacroIndex(m);
   if (j < 0) return j;
   if (df) fprintf(df,"Expanding [%s] phase %d\n",Mac[j].Name,Phase);

   p = NextSymbol(m,Macro);
   p = SkipSpace(p);
   if (*p == '(') an = ScanArguments(p+1,MacArgs,ArgPtr);
   else           an = 0;
   if (an != Mac[j].Narg)
   {
      ++ErrNum;
      printf("\n*** Error line %d: ",LiNo);
      printf("Wrong # of arguments in [%s] called (%d) defined (%d)\n",
            Macro,an,Mac[j].Narg);
      exit(1);
   }
   CurrentMacro = j;
   ++InsideMacro;
   MacroPointer = Mac[j].Body;

   if (Phase == 2)
   {
      Mac[j].Cola = m - Line;
      fprintf(lf,"%5d               %s\n",LiNo,Line);
   }
   return j;
}


void NextMacLine(char *w)
{
   int i;
   char *r;

   --LiNo; // Macro expansion should not increment line count
   if (*MacroPointer)
   {
      while (*MacroPointer && *MacroPointer != '\n')
      {
         if (*MacroPointer == '&')
         {
            i = *(++MacroPointer) - '0';
            r = MacArgs + ArgPtr[i];
            while (*r) *w++ = *r++;
            ++MacroPointer;
         }
         else *w++ = *MacroPointer++;
      }
      if (*MacroPointer == '\n') ++MacroPointer;
   }
   else
   {
      CurrentMacro = 0;
      --InsideMacro;
      MacroPointer = NULL;
   }
   *w = 0;
}


void ParseLine(char *cp)
{
   int v,m;

   am = -1;
   oc = -1;
   Label[0] = 0;
   Operand[0] = 0;
   Comment[0] = 0;
   cp = SkipHexCode(cp);        // Skip disassembly
   cp = SkipSpace(cp);          // Skip leading blanks
   if (CheckCondition(cp)) return;
   if (Skipping)
   {
      if (Phase == 2) fprintf(lf,"%5d SKIP          %s\n",LiNo,Line);
      if (df)         fprintf(df,"%5d SKIP          %s\n",LiNo,Line);
      return;
   }
   if (*cp == 0 || *cp == ';')  // Empty or comment only
   {
      if (Phase == 2) fprintf(lf,"%5d               %s\n",LiNo,Line);
      return;
   }
   if (isalpha(*cp))            // Macro, Label or mnemonic
   {
      if (!strncasecmp(cp,"CPU",3))
      {
         CPU_Type = 0; // Default: 6502
         if (Strcasestr(cp+3,"65c02")) CPU_Type = 1;
         if (Phase == 2)
         {
            fprintf(lf,"%5d               %s\n",LiNo,Line);
         }
         return;
      }
      if (!strncasecmp(cp,"MACRO ",6))
      {
         RecordMacro(cp+6);
         return;
      }
      if (IsInstruction(cp) < 0)
      {
         m = ExpandMacro(cp);
         if (m < 0)
         {
            cp = DefineLabel(cp,&v,0);
            cp = SkipSpace(cp);         // Skip leading blanks
            if (*cp) ExpandMacro(cp);   // Macro after label
         }
         if (*cp == 0 || *cp == ';') // no code or data
         {
            if (Phase == 2)
               fprintf(lf,"%5d %4.4x          %s\n",LiNo,v,Line);
            return;
         }
      }
   }
   if (*cp ==  0 ) return;            // No code
   if (*cp == '*') cp = SetPC(cp);    // Set program counter
   if (*cp == '&') cp = SetBSS(cp);   // Set BSS counter
   if (*cp == '.') cp = IsData(cp);   // Data
   oc = IsInstruction(cp);            // Check for mnemonic
   if (oc >= 0) GenerateCode(cp);
   if (*cp == ';') return;      // Comment line
}

void Phase1Listing(void)
{
   fprintf(df,"%5d %4.4x",LiNo,pc);
   if (Label[0]) fprintf(df,"  Label:[%s]",Label);
   if (oc >= 0) fprintf(df,"  Op:%2.2x %s",oc,set[oc].mne);
   if (Operand[0]) fprintf(df,"  %s",Operand);
   if (Comment[0]) fprintf(df,"  %s",Comment);
   fprintf(df,"\n");
}


void Phase1(void)
{
    int l;

   Phase = 1;
   fgets(Line,sizeof(Line),sf);
   while (!feof(sf))
   {
      ++LiNo;
      l = strlen(Line);
      if (l && Line[l-1] == 10) Line[--l] = 0; // Remove linefeed
      if (l && Line[l-1] == 13) Line[--l] = 0; // Remove return
      ParseLine(Line);
      if (df) Phase1Listing();
      if (InsideMacro) NextMacLine(Line);
      else fgets(Line,sizeof(Line),sf);
   }
}


void Phase2(void)
{
    int l;

   Phase = 2;
   if (IfLevel)
   {
      printf("\n*** Error in conditional assembly ***\n");
      if (IfLevel == 1)
         printf("*** an #endif statement is missing\n");
      else
         printf("*** %d #endif statements are missing\n",IfLevel);
      exit(1);
   }
   rewind(sf);
   LiNo = 0;
   fgets(Line,sizeof(Line),sf);
   while (!feof(sf))
   {
      ++LiNo;
      l = strlen(Line);
      if (l && Line[l-1] == 10) Line[--l] = 0; // Remove linefeed
      if (l && Line[l-1] == 13) Line[--l] = 0; // Remove return
      ParseLine(Line);
      if (InsideMacro) NextMacLine(Line);
      else fgets(Line,sizeof(Line),sf);
      if (GenEnd < pc) GenEnd = pc; // Remember highest assenble address
      if (ErrNum >= ERRMAX)
      {
         printf("\n*** Error count reached maximum of %d ***\n",ErrNum);
         printf("Assembly stopped\n");
         return;
      }
   }
}


void ListSymbols(int n, int lb, int ub)
{
   int i,j,l;
   char A;

   for (i=0 ; i < n && i < Labels; ++i)
   if (!lab[i].Paired && lab[i].Address >= lb && lab[i].Address <= ub)
   {
      fprintf(lf,"%-30.30s $%4.4x",lab[i].Name,lab[i].Address);
      for (j=0 ; j <= lab[i].NumRef ; ++j)
      {
         if (j > 0 && (j % 5) == 0)
         fprintf(lf,"\n                                    ");
         fprintf(lf,"%6d",lab[i].Ref[j]);
         l = lab[i].Att[j];
         if (l == LDEF || l == LBSS || l == LPOS) A = 'D';
         else if (l == Indx) A = 'x';
         else if (l == Indy) A = 'y';
         else  A = ' ';
         fprintf(lf,"%c",A);
      }
      fprintf(lf,"\n");
   }
}


void PairSymbols(void)
{
   int i,j,k,n,ni,nk,indy;

   for (i=0 ; i < Labels-1; ++i)
   if (lab[i].Address < 0xff)
   {
      indy = 0;
      for (j=0 ; j <= lab[i].NumRef ; ++j)
      if (lab[i].Att[j] == Indy && lab[i+1].Address == lab[i].Address+1)
      {
         indy = 1;
         break;
      }
      if (indy)
      {
         k  = i + 1;
         ni = lab[i].NumRef + 1;
         nk = lab[k].NumRef + 1;
         n  = ni + nk;
         lab[i].Name = realloc(lab[i].Name,strlen(lab[i].Name)+strlen(lab[k].Name)+2);
         strcat(lab[i].Name,"/");
         strcat(lab[i].Name,lab[k].Name);
         lab[i].Ref = realloc(lab[i].Ref,n*sizeof(int));
         lab[i].Att = realloc(lab[i].Att,n*sizeof(int));
         memcpy(lab[i].Ref+ni,lab[k].Ref,nk*sizeof(int));
         memcpy(lab[i].Att+ni,lab[k].Att,nk*sizeof(int));
         lab[i].NumRef = n-1;
         lab[k].Paired =   1;
      }
   }
}


void ListUndefinedSymbols(void)
{
   int i;

   for (i=0 ; i < Labels ; ++i)
   {
      if (lab[i].Address == UNDEF) printf("Undefined: %s\n",lab[i].Name);
   }
}

int CmpAddress( const void *arg1, const void *arg2 )
{
   struct LabelStruct *Label1;
   struct LabelStruct *Label2;

   Label1 = (struct LabelStruct *) arg1;
   Label2 = (struct LabelStruct *) arg2;

   if (Label1->Address == Label2->Address) return 0;
   if (Label1->Address  > Label2->Address) return 1;
   return -1;
}


int CmpRefs( const void *arg1, const void *arg2 )
{
   struct LabelStruct *Label1;
   struct LabelStruct *Label2;

   Label1 = (struct LabelStruct *) arg1;
   Label2 = (struct LabelStruct *) arg2;

   if (Label1->NumRef == Label2->NumRef)
   {
      if (Label1->Address < Label2->Address) return  1;
      if (Label1->Address > Label2->Address) return -1;
      return 0;
   }
   if (Label1->NumRef  < Label2->NumRef) return 1;
   return -1;
}

void WriteBinaries(void)
{
   int i;
   unsigned char lo,hi;
   FILE *bf;

   for (i=0 ; i < StoreCount ; ++i)
   {
      if (df) fprintf(df,"Storing $%4.4x - $%4.4x <%s>\n",
                      SFA[i],SFA[i]+SFL[i],SFF[i]);
      bf = fopen(SFF[i],"wb");
      if (WriteLoadAddress)
      {
         lo = SFA[i] & 0xff;
         hi = SFA[i]  >>  8;
         fwrite(&lo,1,1,bf);
         fwrite(&hi,1,1,bf);
      }
      fwrite(ROM+SFA[i],1,SFL[i],bf);
      fclose(bf);
   }
}

int main(int argc, char *argv[])
{
   int ic,v;

   for (ic=1 ; ic < argc ; ++ic)
   {
      if (!strcmp(argv[ic],"-x")) SkipHex = 1;
      else if (!strcmp(argv[ic],"-D")) Debug = 1;
      else if (!strncmp(argv[ic],"-d",2)) DefineLabel(argv[ic]+2,&v,1);
      else if (argv[ic][0] >= 'A')
      {
              if (!Src[0]) strcpy(Src,argv[ic]);
         else if (!Lst[0]) strcpy(Lst,argv[ic]);
      }
      else
      {
         printf("\nUsage: bsa [-d -D -x] <source> <bin> <list>\n");
         exit(1);
      }
   }
   if (!Src[0])
   {
      printf("*** missing filename fpr assembler source file ***\n");
      printf("\nUsage: bsa [-d -D -x] <source> [<bin> <list>]\n");
      exit(1);
   }

   // default file names if only source file specified:
   // prog.asm   prog   prog.lst

   if (!Lst[0]) strcpy(Lst,Src);
   if (!(strlen(Src) > 4 && !strcasecmp(Src+strlen(Src)-4,".asm")))
       strcat(Src,".asm");
   if ( (strlen(Lst) > 4 && !strcasecmp(Lst+strlen(Lst)-4,".asm")))
       Lst[strlen(Lst)-4] = 0;
   if (!(strlen(Lst) > 4 && !strcasecmp(Lst+strlen(Lst)-4,".lst")))
       strcat(Lst,".lst");

   printf("\n");
   printf("*******************************************\n");
   printf("* Black Smurf Assembler 1.2 * 16-Apr-2014 *\n");
   printf("* --------------------------------------- *\n");
   printf("* Source: %-31.31s *\n",Src);
   printf("* List  : %-31.31s *\n",Lst);
   printf("*******************************************\n");

   sf = fopen(Src,"r");
   if (!sf)
   {
      printf("Could not open <%s>\n",Src);
      exit(1);
   }
   lf = fopen(Lst,"w");  // Listing
   if (Debug) df = fopen("Debug.lst","w");

   Phase1();
   Phase2();
   WriteBinaries();
   ListUndefinedSymbols();
   PairSymbols();
   qsort(lab,Labels,sizeof(struct LabelStruct),CmpAddress);
   fprintf(lf,"\n\n%5d Symbols\n",Labels);
   fprintf(lf,"-------------\n");
   ListSymbols(Labels,0,0xffff);
   qsort(lab,Labels,sizeof(struct LabelStruct),CmpRefs);
   ListSymbols(Labels,0,0xff);
   ListSymbols(Labels,0,0x4000);
   fclose(sf);
   fclose(lf);
   if (df) fclose(df);
   printf("* Source Lines: %6d                    *\n",LiNo  );
   printf("* Symbols     : %6d                    *\n",Labels);
   printf("* Macros      : %6d                    *\n",Macros);
   printf("*******************************************\n");
   printf("\n");
   return 0;
}
