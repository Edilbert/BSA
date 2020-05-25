/*

*******************
Bit Shift Assembler
*******************

Version: 25-May-2020

The assembler was developed and tested on an iMAC with OSX Mavericks.
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

It will read "hello.asm" as input file and write the
listing file with cross reference "hello.lst".
Binary output is controlled within the source file by means
of the pseudo op ".STORE" (see below for syntax):

Case sensitivity
================
6502 mnemonics, and pseudo opcodes are insensitive to case:

LDA lda Lda are all equivalent (Load Accumulator)

.BYTE .byte .Byte are all equivalent (define byte data)

Label and named constants are case sensitive by default!
The option "-i" switches off the case sensitivity for symbols.
Also the pseudo op ".CASE +/-" may be used to switch sensitivity.

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
.REAL  3.1415926               stores a 40 bit real in CBM format
.REAL4 3.1415926               stores a 32 bit real in CBM format
.BYTE <"BRK"                   low  byte of packed ASCII word (3 x 5 bit)
.BYTE >"BRK"                   high byte of packed ASCII word (3 x 5 bit)
.FILL  N ($EA)                 fill memory with N bytes containing $EA
.FILL  $A000 - * (0)           fill memory from pc(*) upto $9FFF
.INCLUDE "filename"            includes specified file
.END                           stops assembly
.CASE -                        symbols are not case sensitive
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
(     arithmetic bracket
)     arithmetic bracket
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
^     bitwise xor

Relational operators
====================
==    equal
!=    not equal
>     greater than
<     less than
>=    greater than or equal to
<=    less than or equal to
<<    left shift
>>    right shift
&&    and
||    or

Relational operators return the integer 0 (false) or 1 (true).

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
Example: Assemble first part if C64 has a non zero value

#if C64
   STA $D000
#else
   STA $9000
#endif

Example: Assemble first part if C64 is defined ($0000 - $ffff)
(undefined symbols are set to UNDEF ($ffff0000)

#ifdef C64
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

Example: check and force error

#if (MAXLEN & $ff00)
   #error This code is 8 bit only, MAXLEN too large!
#endif

The maximum nesting depth is 10

For more examples see the complete operating system for C64 and VC20
(VIC64.asm.gz) on the forum "www.forum64.de".

*/

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

// The following string routines are not called from a library
// but are coded explicitely, because there is a strong
// disagreement among Windows, MAC OS and Linux how to name them.

// **********
// Strcasecmp
// **********

int Strcasecmp(const char *s1, const char *s2)
{
   for (size_t i=0 ; i <= strlen(s1) && i <= strlen(s2) ; ++i)
   {
      if (tolower(s1[i]) > tolower(s2[i])) return  1;
      if (tolower(s1[i]) < tolower(s2[i])) return -1;
   }
   return 0;
}

// ***********
// Strncasecmp
// ***********

int Strncasecmp(const char *s1, const char *s2, size_t n)
{
   for (size_t i=0 ; i <= strlen(s1) && i <= strlen(s2) && i < n; ++i)
   {
      if (tolower(s1[i]) > tolower(s2[i])) return  1;
      if (tolower(s1[i]) < tolower(s2[i])) return -1;
   }
   return 0;
}

// **********
// Strcasestr
// **********

char *Strcasestr(const char *haystack, const char *needle)
{
   size_t hlen = strlen(haystack);
   size_t nlen = strlen(needle);

   if (!nlen) return (char *)haystack;
   if (hlen < nlen) return NULL;
   for (size_t i=0 ; i <= hlen-nlen ; ++i)
   {
      if (!Strncasecmp(haystack+i,needle,nlen)) return (char *)(haystack+i);
   }
   return NULL;
}

// ***********
// MallocOrDie
// ***********

void *MallocOrDie(size_t size)
{
   void *ptr = malloc(size);
   if (ptr) return ptr;
   fprintf(stderr,"Allocation of memory failed with size = %lu\n",size);
   exit(1);
}

// ************
// ReallocOrDie
// ************

void *ReallocOrDie(void *ptr, size_t size)
{
   ptr = realloc(ptr,size);
   if (ptr) return ptr;
   fprintf(stderr,"Reallocation of memory failed with size = %lu\n",size);
   exit(1);
}

// ****************************
// CPU types of the 6502 family
// ****************************

enum CPU_Enum
{
   CPU_6502   , // Commodore, Atari, Apple
   CPU_65SC02 , // Apple IIc
   CPU_65C02  , // Apple IIc, Apple IIe
   CPU_65802  ,
   CPU_65816  , // Apple IIgs
   CPU_End      // End marker
};

int CPU_Type = CPU_6502; // default

const char *CPU_Name[] =
{
   "6502"   , // Commodore, Atari, Apple
   "65SC02" , // Apple IIc
   "65C02"  , // Apple IIc, Apple IIe
   "65802"  ,
   "65816"    // Apple IIgs, C256 Foenix
};

enum Addressing_Mode
{
   AM_None        , // Invalid
   AM_Inherent    , // Implied for 65xx family
   AM_Register    , // Accumulator for 65xx
   AM_Immediate   ,
   AM_Branch      ,
   AM_Direct      , // Zero page for 65xx
   AM_Extended    , // Absolute for 65xx
   AM_Indexed     ,
   AM_Indirect

};

struct AM_Inherent_Struct
{
   const char *Mnemonic;
   int         Opcode;
};

struct AM_Inherent_Struct AM_Inherent_6502[] =
{
   // 65816 & 65802

   {"PHD" , 0x0b},
   {"TCS" , 0x1b},
   {"PLD" , 0x2b},
   {"TSC" , 0x3b},
   {"PHK" , 0x4b},
   {"TCD" , 0x5b},
   {"RTL" , 0x6b},
   {"TDC" , 0x7b},
   {"PHB" , 0x8b},
   {"TXY" , 0x9b},
   {"PLB" , 0xab},
   {"TYX" , 0xbb},
   {"WAI" , 0xcb},
   {"STP" , 0xdb},
   {"XBA" , 0xeb},
   {"XCE" , 0xfb},

   // 65C02 & 65SC02: index 16

   {"INA" , 0x1a}, // also INC A
   {"DEA" , 0x3a}, // also DEC A
   {"PHY" , 0x5a},
   {"PLY" , 0x7a},
   {"PHX" , 0xda},
   {"PLX" , 0xfa},

   // 6502: index 22

   {"BRK" , 0x00},
   {"PHP" , 0x08},
   {"CLC" , 0x18},
   {"PLP" , 0x28},
   {"SEC" , 0x38},
   {"RTI" , 0x40},
   {"PHA" , 0x48},
   {"CLI" , 0x58},
   {"RTS" , 0x60},
   {"PLA" , 0x68},
   {"SEI" , 0x78},
   {"DEY" , 0x88},
   {"TXA" , 0x8a},
   {"TYA" , 0x98},
   {"TXS" , 0x9a},
   {"TAY" , 0xa8},
   {"TAX" , 0xaa},
   {"CLV" , 0xb8},
   {"TSX" , 0xba},
   {"INY" , 0xc8},
   {"DEX" , 0xca},
   {"CLD" , 0xd8},
   {"INX" , 0xe8},
   {"NOP" , 0xea},
   {"SED" , 0xf8},
   {"---" ,   -1}
};

struct AM_Register_Struct
{
   const char *Mnemonic;
   int         Opcode;
};


// Operand is only the acculumator "A"

struct AM_Register_Struct AM_Register_6502[] =
{
   // 65816 & 65802 & 65C02 & 65SC02

   {"INC" , 0x1a},
   {"DEC" , 0x3a},

   // 6502

   {"ASL" , 0x0a}, // index 2
   {"ROL" , 0x2a},
   {"LSR" , 0x4a},
   {"ROR" , 0x6a},
   {"---" ,   -1}
};


struct CPU_Property_Struct
{
   struct AM_Inherent_Struct *AM_Inherent_Tab; // mnemonic table
   struct AM_Register_Struct *AM_Register_Tab; // mnemonic table
   int WordOC;                           // two byte opcodes
};

struct CPU_Property_Struct CPU_Property_Tab[] =
{
   {AM_Inherent_6502 + 22, AM_Register_6502 + 2, 0}, // 6502
   {AM_Inherent_6502 + 16, AM_Register_6502    , 0}, // 65SC02
   {AM_Inherent_6502 + 16, AM_Register_6502    , 0}, // 65C02
   {AM_Inherent_6502     , AM_Register_6502    , 0}, // 65802
   {AM_Inherent_6502     , AM_Register_6502    , 0}  // 65816
};

struct AM_Inherent_Struct *AM_Inherent_Tab = AM_Inherent_6502 + 22; // 6502
struct AM_Register_Struct *AM_Register_Tab = AM_Register_6502 +  2; // 6502

const char *Register_Operands_6502[] =
{
   "A"
};

enum AddressType {
None,           // invalid
Impl,Accu,      // 1 byte opcodes
Rela,Imme,      // 2 byte opcodes
Zpag,Zpgx,Zpgy, // 2 byte zero page modes
Abso,Absx,Absy, // 3 byte absolute modes
Indi,Indx,Indy, // 2 indirect modes
Stac,Stay,      //   stack relative
Indl,Lonx,Lony, //   indexed long
Long            //   long
};

char Prefix[] = {
' ',            // invalid
' ','A',        // 1 byte opcodes
' ','#',        // 2 byte opcodes
' ',' ',' ',    // 2 byte zero page modes
' ',' ',' ',    // 3 byte absolute modes
'(','(','(',    //   indirect modes
' ','(',        //   stack relative
'[',' ','[',    //   indexed long
' '             //   long
};

char *Suffix[] = {
"",             // invalid
"","",          // 1 byte opcodes
"","",          // 2 byte opcodes
"",",X",",Y",   // 2 byte zero page modes
"",",X",",Y",   // 3 byte absolute modes
")",",X)","),Y",//   indirect modes
",S",",S),Y",   //   stack relative
"]",",X","],Y", //   indexed long
""              //   long
};

int Lenfix[] = {
0,              // invalid
1,1,            // 1 byte opcodes
2,2,            // 2 byte opcodes
2,2,2,          // 2 byte zero page modes
3,3,3,          // 3 byte absolute modes
2,2,2,          //   indirect modes
2,2,            //   stack relative
2,4,2,          //   indexed long
4               //   long
};


struct cpu_struct
{
   int  cpu;     // 0 = 6502, 1 = 65c02, 2 = 65816
   int  amo;
   char mne[5];
};

struct cpu_struct set[256] =
{
   {0, Impl, "BRK" },   // 00
   {0, Indx, "ORA" },   // 01
   {2, Imme, "COP" },   // 02
   {2, Stac, "ORA" },   // 03
   {1, Zpag, "TSB" },   // 04
   {0, Zpag, "ORA" },   // 05
   {0, Zpag, "ASL" },   // 06
   {3, Indl, "ORA" },   // 07  RMB0
   {0, Impl, "PHP" },   // 08
   {0, Imme, "ORA" },   // 09
   {0, Accu, "ASL" },   // 0a
   {2, Impl, "PHD" },   // 0b
   {1, Abso, "TSB" },   // 0c
   {0, Abso, "ORA" },   // 0d
   {0, Abso, "ASL" },   // 0e
   {3, Long, "ORA" },   // 0f  BBR0

   {0, Rela, "BPL" },   // 10
   {0, Indy, "ORA" },   // 11
   {1, Indi, "ORA" },   // 12
   {2, Stay, "ORA" },   // 13
   {1, Zpag, "TRB" },   // 14
   {0, Zpgx, "ORA" },   // 15
   {0, Zpgx, "ASL" },   // 16
   {3, Lony, "ORA" },   // 17  RMB1
   {0, Impl, "CLC" },   // 18
   {0, Absy, "ORA" },   // 19
   {1, Accu, "INC" },   // 1a
   {2, Impl, "TCS" },   // 1b
   {1, Abso, "TRB" },   // 1c
   {0, Absx, "ORA" },   // 1d
   {0, Absx, "ASL" },   // 1e
   {3, Lonx, "ORA" },   // 1f  BBR1

   {0, Abso, "JSR" },   // 20
   {0, Indx, "AND" },   // 21
   {3, Long, "JSL" },   // 22
   {2, Stac, "AND" },   // 23
   {0, Zpag, "BIT" },   // 24
   {0, Zpag, "AND" },   // 25
   {0, Zpag, "ROL" },   // 26
   {3, Indl, "AND" },   // 27  RMB2
   {0, Impl, "PLP" },   // 28
   {0, Imme, "AND" },   // 29
   {0, Accu, "ROL" },   // 2a
   {2, Impl, "PLD" },   // 2b
   {0, Abso, "BIT" },   // 2c
   {0, Abso, "AND" },   // 2d
   {0, Abso, "ROL" },   // 2e
   {3, Long, "AND" },   // 2f  BBR2

   {0, Rela, "BMI" },   // 30
   {0, Indy, "AND" },   // 31
   {1, Indi, "AND" },   // 32
   {2, Stay, "AND" },   // 33
   {1, Zpgx, "BIT" },   // 34
   {0, Zpgx, "AND" },   // 35
   {0, Zpgx, "ROL" },   // 36
   {3, Lony, "AND" },   // 37  RMB3
   {0, Impl, "SEC" },   // 38
   {0, Absy, "AND" },   // 39
   {1, Accu, "DEC" },   // 3a
   {2, Impl, "TSC" },   // 3b
   {1, Absx, "BIT" },   // 3c
   {0, Absx, "AND" },   // 3d
   {0, Absx, "ROL" },   // 3e
   {3, Lonx, "AND" },   // 3f  BBR3

   {0, Impl, "RTI" },   // 40
   {0, Indx, "EOR" },   // 41
   {3, Impl, "WDM" },   // 42
   {2, Stac, "EOR" },   // 43
   {3, Abso, "MVP" },   // 44
   {0, Zpag, "EOR" },   // 45
   {0, Zpag, "LSR" },   // 46
   {3, Indl, "EOR" },   // 47  RMB4
   {0, Impl, "PHA" },   // 48
   {0, Imme, "EOR" },   // 49
   {0, Accu, "LSR" },   // 4a
   {2, Impl, "PHK" },   // 4b
   {0, Abso, "JMP" },   // 4c
   {0, Abso, "EOR" },   // 4d
   {0, Abso, "LSR" },   // 4e
   {3, Long, "EOR" },   // 4f  BBR4

   {0, Rela, "BVC" },   // 50
   {0, Indy, "EOR" },   // 51
   {1, Indi, "EOR" },   // 52
   {2, Stay, "EOR" },   // 53
   {2, Abso, "MVN" },   // 54
   {0, Zpgx, "EOR" },   // 55
   {0, Zpgx, "LSR" },   // 56
   {3, Lony, "EOR" },   // 57  RMB5
   {0, Impl, "CLI" },   // 58
   {0, Absy, "EOR" },   // 59
   {1, Impl, "PHY" },   // 5a
   {2, Impl, "TCD" },   // 5b
   {3, Long, "JMP" },   // 5c
   {0, Absx, "EOR" },   // 5d
   {0, Absx, "LSR" },   // 5e
   {3, Lonx, "EOR" },   // 5f  BBR5

   {0, Impl, "RTS" },   // 60
   {0, Indx, "ADC" },   // 61
   {2, Abso, "PER" },   // 62
   {2, Stac, "ADC" },   // 63
   {1, Zpag, "STZ" },   // 64
   {0, Zpag, "ADC" },   // 65
   {0, Zpag, "ROR" },   // 66
   {3, Indl, "ADC" },   // 67  RMB6
   {0, Impl, "PLA" },   // 68
   {0, Imme, "ADC" },   // 69
   {0, Accu, "ROR" },   // 6a
   {3, Impl, "RTL" },   // 6b
   {0, Indi, "JMP" },   // 6c
   {0, Abso, "ADC" },   // 6d
   {0, Abso, "ROR" },   // 6e
   {3, Long, "ADC" },   // 6f  BBR6

   {0, Rela, "BVS" },   // 70
   {0, Indy, "ADC" },   // 71
   {1, Indi, "ADC" },   // 72
   {2, Stay, "ADC" },   // 73
   {1, Zpgx, "STZ" },   // 74
   {0, Zpgx, "ADC" },   // 75
   {0, Zpgx, "ROR" },   // 76
   {3, Lony, "ADC" },   // 77  RMB7
   {0, Impl, "SEI" },   // 78
   {0, Absy, "ADC" },   // 79
   {1, Impl, "PLY" },   // 7a
   {2, Impl, "TDC" },   // 7b
   {1, Indx, "JMP" },   // 7c
   {0, Absx, "ADC" },   // 7d
   {0, Absx, "ROR" },   // 7e
   {3, Lonx, "ADC" },   // 7f  BBR7

   {1, Rela, "BRA" },   // 80
   {0, Indx, "STA" },   // 81
   {2, Abso, "BRL" },   // 82
   {2, Stac, "STA" },   // 83
   {0, Zpag, "STY" },   // 84
   {0, Zpag, "STA" },   // 85
   {0, Zpag, "STX" },   // 86
   {3, Indl, "STA" },   // 87  SMB0
   {0, Impl, "DEY" },   // 88
   {1, Imme, "BIT" },   // 89
   {0, Impl, "TXA" },   // 8a
   {2, Impl, "PHB" },   // 8b
   {0, Abso, "STY" },   // 8c
   {0, Abso, "STA" },   // 8d
   {0, Abso, "STX" },   // 8e
   {3, Long, "STA" },   // 8f  BBS0

   {0, Rela, "BCC" },   // 90
   {0, Indy, "STA" },   // 91
   {1, Indi, "STA" },   // 92
   {2, Stay, "STA" },   // 93
   {0, Zpgx, "STY" },   // 94
   {0, Zpgx, "STA" },   // 95
   {0, Zpgy, "STX" },   // 96
   {3, Lony, "STA" },   // 97  SMB1
   {0, Impl, "TYA" },   // 98
   {0, Absy, "STA" },   // 99
   {0, Impl, "TXS" },   // 9a
   {2, Impl, "TXY" },   // 9b
   {1, Abso, "STZ" },   // 9c
   {0, Absx, "STA" },   // 9d
   {1, Absx, "STZ" },   // 9e
   {3, Lonx, "STA" },   // 9f  BBS1

   {0, Imme, "LDY" },   // a0
   {0, Indx, "LDA" },   // a1
   {0, Imme, "LDX" },   // a2
   {2, Stac, "LDA" },   // a3
   {0, Zpag, "LDY" },   // a4
   {0, Zpag, "LDA" },   // a5
   {0, Zpag, "LDX" },   // a6
   {3, Indl, "LDA" },   // a7  SMB2
   {0, Impl, "TAY" },   // a8
   {0, Imme, "LDA" },   // a9
   {0, Impl, "TAX" },   // aa
   {2, Impl, "PLB" },   // ab
   {0, Abso, "LDY" },   // ac
   {0, Abso, "LDA" },   // ad
   {0, Abso, "LDX" },   // ae
   {3, Long, "LDA" },   // af  BBS2

   {0, Rela, "BCS" },   // b0
   {0, Indy, "LDA" },   // b1
   {1, Indi, "LDA" },   // b2
   {2, Stay, "LDA" },   // b3
   {0, Zpgx, "LDY" },   // b4
   {0, Zpgx, "LDA" },   // b5
   {0, Zpgy, "LDX" },   // b6
   {3, Lony, "LDA" },   // b7  SMB3
   {0, Impl, "CLV" },   // b8
   {0, Absy, "LDA" },   // b9
   {0, Impl, "TSX" },   // ba
   {2, Impl, "TYX" },   // bb
   {0, Absx, "LDY" },   // bc
   {0, Absx, "LDA" },   // bd
   {0, Absy, "LDX" },   // be
   {3, Lonx, "LDA" },   // bf  BBS3

   {0, Imme, "CPY" },   // c0
   {0, Indx, "CMP" },   // c1
   {2, Imme, "REP" },   // c2
   {2, Stac, "CMP" },   // c3
   {0, Zpag, "CPY" },   // c4
   {0, Zpag, "CMP" },   // c5
   {0, Zpag, "DEC" },   // c6
   {3, Indl, "CMP" },   // c7  SMB4
   {0, Impl, "INY" },   // c8
   {0, Imme, "CMP" },   // c9
   {0, Impl, "DEX" },   // ca
   {2, Impl, "WAI" },   // cb
   {0, Abso, "CPY" },   // cc
   {0, Abso, "CMP" },   // cd
   {0, Abso, "DEC" },   // ce
   {3, Long, "CMP" },   // cf  BBS4

   {0, Rela, "BNE" },   // d0
   {0, Indy, "CMP" },   // d1
   {1, Indi, "CMP" },   // d2
   {2, Stay, "CMP" },   // d3
   {2, Indi, "PEI" },   // d4
   {0, Zpgx, "CMP" },   // d5
   {0, Zpgx, "DEC" },   // d6
   {3, Lony, "CMP" },   // d7  SMB5
   {0, Impl, "CLD" },   // d8
   {0, Absy, "CMP" },   // d9
   {1, Impl, "PHX" },   // da
   {2, Impl, "STP" },   // db
   {3, Indl, "JMP" },   // dc
   {0, Absx, "CMP" },   // dd
   {0, Absx, "DEC" },   // de
   {3, Lonx, "CMP" },   // df  BS5

   {0, Imme, "CPX" },   // e0
   {0, Indx, "SBC" },   // e1
   {2, Imme, "SEP" },   // e2
   {2, Stac, "SBC" },   // e3
   {0, Zpag, "CPX" },   // e4
   {0, Zpag, "SBC" },   // e5
   {0, Zpag, "INC" },   // e6
   {3, Indl, "SBC" },   // e7  SMB6
   {0, Impl, "INX" },   // e8
   {0, Imme, "SBC" },   // e9
   {0, Impl, "NOP" },   // ea
   {2, Impl, "XBA" },   // eb
   {0, Abso, "CPX" },   // ec
   {0, Abso, "SBC" },   // ed
   {0, Abso, "INC" },   // ee
   {3, Long, "SBC" },   // ef  BBS6

   {0, Rela, "BEQ" },   // f0
   {0, Indy, "SBC" },   // f1
   {1, Indi, "SBC" },   // f2
   {2, Stay, "SBC" },   // f3
   {2, Abso, "PEA" },   // f4
   {0, Zpgx, "SBC" },   // f5
   {0, Zpgx, "INC" },   // f6
   {3, Lony, "SBC" },   // f7  SMB7
   {0, Impl, "SED" },   // f8
   {0, Absy, "SBC" },   // f9
   {1, Impl, "PLX" },   // fa
   {2, Impl, "XCE" },   // fb
   {2, Absx, "JSR" },   // fc
   {0, Absx, "SBC" },   // fd
   {0, Absx, "INC" },   // fe
   {3, Lonx, "SBC" }    // ff  BBS7
};

#define UNDEF 0xff0000

int SkipHex;        // Switch on with -x
int Debug;          // Switch on with -d
int LiNo;           // Line number of current file
int WithLiNo;       // Print line numbers in listing if set
int TotalLiNo;      // Total line number (with include files)
int Preprocess;     // Print preprocessed source file <file.pp>
int ErrNum;         // Error counter
int WriteLA;        // Write Load Address (CBM)
int Petscii;        // Use PETSCII encoding
int MacroStopped;   // Macro end flag
int InsideMacro;    // Macro nesting level
int CurrentMacro;   // Macro index
int ModuleStart;    // Address of a module
int ModuleTrigger;  // Start of module
int WordOC;         // List 2 byte opcodes as word

int ERRMAX = 10;    // Stop assemby after ERRMAX errors
int LoadAddress = UNDEF;
char *MacroPointer;

const char *Mne;       // Current mnemonic

int oc;      // op code
int am;      // address mode
int il;      // instruction length
int pc = -1; // program counter
int bss;     // bss counter
int Phase;
int IfLevel;
int Skipping;
int SkipLine[10];
int ForcedEnd;    // Triggered by .END command
int IgnoreCase;   // 1: Ignore case for symbols

#define ASCII      0
#define PETSCII    1
#define SCREENCODE 2

// Filenames

char *Src;
char  Lst[256];
char  Pre[256];

int GenStart = 0x10000 ; // Lowest assemble address
int GenEnd   =       0 ; //Highest assemble address

// These arrays hold the parameter for storage files

#define SFMAX 20
int SFA[SFMAX];
int SFL[SFMAX];
char SFF[SFMAX][80];

int StoreCount = 0;


// The size is one page more than 64K because program, counter
// overflows are detected after using the new value.
// So references to pc + n do no harm if pc is near the boundary

unsigned char ROM[0x10100]; // binary 64K plus 1 page


FILE *sf;
FILE *lf;
FILE *df;
FILE *pf;

struct IncludeStackStruct
{
   FILE *fp;
   int   LiNo;
   char *Src;
} IncludeStack[100];

int IncludeLevel;

#define ML 256

int ArgPtr[10];
char Line[ML];               // source line
char Label[ML];
char MacArgs[ML];
unsigned char Operand[ML];
char Comment[ML];
char LengthInfo[ML];
char ModuleName[ML];

#define LDEF 1
#define LBSS 2
#define LPOS 3

#define MAXLAB 8000

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

// *********
// SkipSpace
// *********

char *SkipSpace(char *p)
{
   if (p) while (isspace(*p)) ++p;
   return p;
}

// ****
// isym
// ****

int isym(char c)
{
   return (c == '_' || isalnum(c));
}

// *********
// GetSymbol
// *********

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

void ListSymbols(FILE *lf, int n, int lb, int ub);


#define SIZE_ERRMSG 1024

void ErrorMsg(const char *format, ...) {
   va_list args;
   char *buf;

   buf = MallocOrDie(SIZE_ERRMSG);
   snprintf(buf, SIZE_ERRMSG, "\n*** Error in file %s line %d:\n",
         IncludeStack[IncludeLevel].Src, LiNo);
   va_start(args,format);
   vsnprintf(buf+strlen(buf), SIZE_ERRMSG-strlen(buf), format, args);
   va_end(args);
   fputs(buf, stdout);
   fputs(buf, lf);
   if (df)
   {
      fputs(buf, df);
      ListSymbols(df,Labels,0,0xffff);
   }
   free(buf);
}

void PrintLiNo(int Blank)
{
   if (Phase < 2) return;
   if (WithLiNo)
   {
      fprintf(lf,"%5d",LiNo);
      if (Blank ==  1) fprintf(lf," ");
   }
   if (Blank == -1) fprintf(lf,"\n");
}

void PrintPC(void)
{
   if (Phase < 2) return;
   if (WithLiNo) PrintLiNo(1);
   fprintf(lf,"%4.4x",pc);
}

void PrintOC(void)
{
   if (WordOC)
   {
      if (oc > 255) fprintf(lf," %4.4x",oc);
      else          fprintf(lf,"   %2.2x",oc);
   }
   else
   {
      if (oc > 255) fprintf(lf," %2.2x %2.2x",oc>>8,oc&255);
      else          fprintf(lf," %2.2x",oc);
   }
}

void PrintLine(void)
{
   if (Phase < 2) return;
   PrintLiNo(1);
   fprintf(lf,"              %s\n",Line);
}

void PrintPCLine(void)
{
   if (Phase < 2) return;
   PrintPC();
   fprintf(lf,"          %s\n",Line);
}

int IsInstruction(char *p)
{
   int i,l;
   struct AM_Inherent_Struct *is;
   struct AM_Register_Struct *rs;

   // Initialize

   am  = AM_None; // address mode
   Mne = NULL;    // menmonic

   // Check table of inherent instructions

   for (is = AM_Inherent_Tab; is->Opcode >= 0; ++is)
   {
      l = strlen(is->Mnemonic);
      if (!Strncasecmp(p,is->Mnemonic,l) && !isym(p[l]))
      {
         // printf("\n*** %2.2x %s ***\n",is->Opcode,is->Mnemonic);
         am  = AM_Inherent;
         Mne = is->Mnemonic;
         return is->Opcode;
      }
   }

   // Check table of instructions with register operands only

   for (rs = AM_Register_Tab; rs->Opcode >= 0; ++rs)
   {
      l = strlen(rs->Mnemonic);
      if (!Strncasecmp(p,rs->Mnemonic,l) && !isym(p[l]))
      {
         am  = AM_Register;
         Mne = rs->Mnemonic;
         return rs->Opcode;
      }
   }

   // Check for 6502 & 65c02 3-letter mnemonics

   if (isalpha(p[0]) && isalpha(p[1]) && isalpha(p[2]) && !isym(p[3]))
   for (i=0 ; i < 256 ; ++i)
   {
      if (!Strncasecmp(p,set[i].mne,3))
      {
         if (set[i].cpu > CPU_Type && set[i].amo == Impl)
         {
            ErrorLine(p);
            ErrorMsg("Found 65c02 instruction in 6502 mode\n"
                     "Set: CPU = 65c02 to enable 65c02 mode\n");
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
      if (!Strncasecmp(p,set[i].mne,4) && set[i].cpu <= CPU_Type)
         return i; // Is instruction!
   }

   return -1; // No mnemonic
}

char *NeedChar(char *p, char c)
{
   return strchr(p,c);
}

char *EvalOperand(char *, int *, int);

char *ParseCaseData(char *p)
{
   p = SkipSpace(p);
        if (*p == '+') IgnoreCase = 0;
   else if (*p == '-') IgnoreCase = 1;
   else
   {
      ++ErrNum;
      ErrorMsg("Missing '+' or '-' after .CASE\n");
      exit(1);
   }
   PrintLine();
   return p+1;
}

char *SetPC(char *p)
{
   int v;
   if (*p == '*')
   {
      p = NeedChar(p,'=');
      if (!p)
      {
         ++ErrNum;
         ErrorMsg("Missing '=' in set pc * instruction\n");
         exit(1);
      }
   }
   else p += 3; // .ORG syntax
   p = EvalOperand(p+1,&v,0);
   if (df) fprintf(df,"PC = %4.4x\n",v);
   pc = v;
   if (LoadAddress == UNDEF) LoadAddress = pc;
   PrintPCLine();
   if (GenStart > pc) GenStart = pc; // remember lowest pc value
   return p;
}

char *SetBSS(char *p)
{
   p = NeedChar(p,'=');
   if (!p)
   {
      ++ErrNum;
      ErrorMsg("Missing '=' in set BSS & instruction\n");
      exit(1);
   }
   p = EvalOperand(p+1,&bss,0);
   if (df) fprintf(df,"BSS = %4.4x\n",bss);
   if (Phase == 2)
   {
      PrintLiNo(1);
      fprintf(lf,"%4.4x          %s\n",bss,Line);
   }
   return p;
}

int StrCmp(const char *s1, const char *s2)
{
   if (IgnoreCase) return Strcasecmp(s1,s2);
   else            return strcmp(s1,s2);
}

int StrnCmp(const char *s1, const char *s2, size_t n)
{
   if (IgnoreCase) return Strncasecmp(s1,s2,n);
   else            return strncmp(s1,s2,n);
}

int LabelIndex(char *p)
{
   int i;

   for (i = 0 ; i < Labels ; ++i)
   {
      if (!StrCmp(p,lab[i].Name)) return i;
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
      if (!StrnCmp(p,Mac[i].Name,l) && !isym(p[l])) return i;
   }
   return -1;
}



char *DefineLabel(char *p, int *val, int Locked)
{
   int j,l,v;

   if (Labels > MAXLAB -2)
   {
      ++ErrNum;
      ErrorMsg("Too many labels (> %d)\n",MAXLAB);
      exit(1);
   }
   p = GetSymbol(p,Label);
   if (*p == ':') ++p; // Ignore colon after label
   l = strlen(Label);
   p = SkipSpace(p);
   if (*p == '=')
   {
      j = LabelIndex(Label);
      if (j < 0)
      {
         j = Labels;
         lab[j].Name = MallocOrDie(l+1);
         strcpy(lab[j].Name,Label);
         lab[j].Address = UNDEF;
         lab[j].Ref = MallocOrDie(sizeof(int));
         lab[j].Att = MallocOrDie(sizeof(int));
         Labels++;
      }
      lab[j].Ref[0] = LiNo;
      lab[j].Att[0] = LDEF;
      p = EvalOperand(p+1,&v,0);
      if (lab[j].Address == UNDEF) lab[j].Address = v;
      else if (lab[j].Address != v && !lab[j].Locked)
      {
         ++ErrNum;
         ErrorLine(p);
         ErrorMsg("*Multiple assignments for label [%s]\n"
                  "1st. value = $%4.4x   2nd. value = $%4.4x\n",
                  Label, lab[j].Address, v);
         exit(1);
      }
      *val = v;
      if (Locked) lab[j].Locked = Locked;
   }
   else if (!Strncasecmp(p,".BSS",4))
   {
      p = EvalOperand(p+4,&v,0);
      j = LabelIndex(Label);
      if (j < 0)
      {
         j = Labels;
         lab[j].Name = MallocOrDie(l+1);
         strcpy(lab[j].Name,Label);
         lab[j].Address = UNDEF;
         lab[j].Ref = MallocOrDie(sizeof(int));
         lab[j].Att = MallocOrDie(sizeof(int));
         Labels++;
      }
      lab[j].Ref[0] = LiNo;
      lab[j].Att[0] = LBSS;
      if (lab[j].Address == UNDEF) lab[j].Address = bss;
      else if (lab[j].Address != bss)
      {
         ++ErrNum;
         ErrorLine(p);
         ErrorMsg("Multiple assignments for label [%s]\n"
                  "1st. value = $%4.4x   2nd. value = $%4.4x\n",
                  Label,lab[j].Address,bss);
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
         lab[j].Name = MallocOrDie(l+1);
         strcpy(lab[j].Name,Label);
         lab[j].Address = pc;
         lab[j].Ref = MallocOrDie(sizeof(int));
         lab[j].Att = MallocOrDie(sizeof(int));
         Labels++;
      }
      else if (lab[j].Address == UNDEF) lab[j].Address = pc;
      else if (lab[j].Address != pc && !lab[j].Locked)
      {
         ++ErrNum;
         if (Phase == 1)
         {
            ErrorMsg("Multiple label definition [%s]"
                     " value 1: %4.4x   value 2: %4.4x\n",
                     Label, lab[j].Address,pc);
         }
         else
         {
            ErrorMsg("Phase error label [%s]"
                     " phase 1: %4.4x   phase 2: %4.4x\n",
                     Label,lab[j].Address,pc);
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
      ErrorMsg("Too many labels (> %d)\n",MAXLAB);
      exit(1);
   }
   l = strlen(p);
   lab[Labels].Address = UNDEF;
   lab[Labels].Name = MallocOrDie(l+1);
   strcpy(lab[Labels].Name,p);
   lab[Labels].Ref = MallocOrDie(sizeof(int));
   lab[Labels].Att = MallocOrDie(sizeof(int));
   lab[Labels].Ref[0] = LiNo;
   lab[Labels].Att[0] = 0;
   Labels++;
}


void SymRefs(int i)
{
   int n;

   if (Phase != 2) return;
   n = ++lab[i].NumRef;
   lab[i].Ref = ReallocOrDie(lab[i].Ref,(n+1)*sizeof(int));
   lab[i].Ref[n] = LiNo;
   lab[i].Att = ReallocOrDie(lab[i].Att,(n+1)*sizeof(int));
   lab[i].Att[n] = am;
}


char *EvalSymValue(char *p, int *v)
{
   int i;
   char Sym[ML];

   p = GetSymbol(p,Sym);
   for (i=0 ; i < Labels ; ++i)
   {
      if (!StrCmp(Sym,lab[i].Name))
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
      if (!StrCmp(Sym,lab[i].Name))
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
   unsigned int v;
   int i;
   int w;

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
      w = atoi(p);
      for (i=3 ; i >= 0 ; --i)
      {
         Operand[i] = w & 255;
         w >>= 8;
      }
   }
   if (Phase == 2)
   {
      for (i=0 ; i < l ; ++i) ROM[pc+i] = Operand[i];
      PrintPC();
      fprintf(lf," %2.2x %2.2x %2.2x %s\n",
         Operand[0],Operand[1],Operand[2],Line);
   }
   pc += l;
   return p;
}


char *ParseRealData(char *p)
{
   unsigned int v;
   int i,mansize;
   int Sign,Exponent;
   double d;

   mansize = 4;   // default mantissa size is 4
   if (*p == '4') // REAL4 sets mantissa size to 3
   {
      mansize = 3;
      ++p;
   }
   p = SkipSpace(p);
   memset(Operand,0,sizeof(Operand));

   if (*p == '$') // hex notation
   {
      ++p;
      for (i=0 ; i <= mansize ; ++i, p+=2)
      {
          if (!isxdigit(*p) || !isxdigit(*(p+1))) break;
          sscanf(p,"%2x",&v);
          Operand[i] = v;
      }
   }
   else
   {
      d = strtod(p,NULL);
      if (d != 0)
      {
         Sign = 0;
         if (d < 0)
         {
            Sign = 0x80;
            d = -d;
         }
         d = frexp(d,&Exponent);
         Exponent += 0x80;
         if (Exponent < 1 || Exponent > 255)
         {
            ErrorMsg("Exponent %d out of range\n",Exponent);
            ++ErrNum;
            return p+strlen(p);;
         }
         Operand[0] = Exponent;
         d *= 256;
         v = d;
         Operand[1] = (v & 127) | Sign;
         d -= v;

         for (i=2 ; i < 6 ; ++i)
         {
            d *= 256;
            Operand[i] = v = d;
            d -= v;
         }
      }
   }

   // Round

   if (Operand[mansize+1] & 0x80) // rounding bit set
   {
      for (i=mansize ; i > 1 ; --i)
      {
         if (++Operand[i]) break; // no carry
      }
      if (i == 1) // carry was propagated
      {
         if (Operand[1] == 0x7f) // highest positive value
         {
            ++Operand[0];        // Increment exponent
              Operand[1] = 0;
         }
         else if (Operand[1] == 0xff)
         {
            ++Operand[0];        // Increment exponent
              Operand[1] = 0x80;
         }
         else ++Operand[1];
      }
   }

   if (Phase == 2)
   {
      for (i=0 ; i < mansize+1 ; ++i) ROM[pc+i] = Operand[i];
      PrintPC();
      fprintf(lf," %2.2x %2.2x %2.2x",Operand[0],Operand[1],Operand[2]);
      if (mansize == 3 && strncmp(Line,"   ",3)==0)
      {
         fprintf(lf," %2.2x %s\n",Operand[3],Line+3);
      }
      else if (mansize == 4 && strncmp(Line,"      ",6)==0)
      {
         fprintf(lf," %2.2x %2.2x %s\n",Operand[3],Operand[4],Line+6);
      }
      else fprintf(lf," %s\n",Line);
   }
   pc += mansize+1;
   return p;
}


char *EvalDecValue(char *p, int *v)
{
   *v = atoi(p);
   while (isdigit(*p)) ++p;
   if (!isalpha(*p)) return p;
   if ((*p >= 'A' && *p <= 'F') || (*p >= 'a' && *p <= 'f'))
      ErrorMsg("Wrong decimal constant or leading $ for hex missing\n");
   else
      ErrorMsg("Illegal character in decimal constant\n");
   ++ErrNum;
   ErrorLine(p);
   exit(1);
}


char *EvalCharValue(char *p, int *v)
{
   // special code for Commodore syntax lda #'

   if (*p == 0 || *p == ' ')
   {
      *v = ' ';
      return p + strlen(p);
   }
   *v = *p++;
   if (*p != '\'' && *p != 0)
   {
      ++ErrNum;
      ErrorMsg("Missing ' delimiter after character operand\n");
      exit(1);
   }
   return p+1;
}


char *EvalHexValue(char *p, int *v)
{
   unsigned int w;
   sscanf(p,"%x",&w);
   while (isxdigit(*p)) ++p;
   *v = w;
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

char *op_par(char *p, int *v)
{
   char c = (*p == '[') ? ']' : ')'; // closing char
   p = EvalOperand(p+1,v,0);
   p = NeedChar(p,c);
   if (!p)
   {
      ErrorLine(p);
      ErrorMsg("Missing closing %c\n",c);
      exit(1);
   }
   return p+1;
}

// functions parsing unary operators or constants

char *op_plu(char *p, int *v) { p = EvalOperand(p+1,v,12)               ; return p; }
char *op_min(char *p, int *v) { p = EvalOperand(p+1,v,12);*v = -(*v)    ; return p; }
char *op_lno(char *p, int *v) { p = EvalOperand(p+1,v,12);*v = !(*v)    ; return p; }
char *op_bno(char *p, int *v) { p = EvalOperand(p+1,v,12);*v = ~(*v)    ; return p; }

char *op_low(char *p, int *v)
{
   p = EvalOperand(p+1,v,12);
   if (*v != UNDEF) *v = *v & 0xff;
   return p;
}

char *op_hig(char *p, int *v)
{
   p = EvalOperand(p+1,v,12);
   if (*v != UNDEF) *v = *v >> 8;
   return p;
}

char *op_prc(char *p, int *v) { *v = pc; return p+1;}
char *op_hex(char *p, int *v) { return EvalHexValue(p+1,v) ;}
char *op_cha(char *p, int *v) { return EvalCharValue(p+1,v);}
char *op_bin(char *p, int *v) { return EvalBinValue(p+1,v) ;}
char *op_len(char *p, int *v) { return EvalSymBytes(p+1,v) ;}

struct unaop_struct
{
   char op;
   char *(*foo)(char*,int*);
};

#define UNAOPS 13

// table of unary operators in C style

struct unaop_struct unaop[UNAOPS] =
{
   {'[',&op_par}, // bracket
   {'(',&op_par}, // parenthesis
   {'+',&op_plu}, // positive sign
   {'-',&op_min}, // negative sign
   {'!',&op_lno}, // logical NOT
   {'~',&op_bno}, // bitwise NOT
   {'<',&op_low}, // low byte
   {'>',&op_hig}, // high byte
   {'*',&op_prc}, // program counter
   {'$',&op_hex}, // hex constant
   { 39,&op_cha}, // char constant
   {'%',&op_bin}, // binary constant
   {'?',&op_len}  // length of .BYTE data line
};

int op_mul(int l, int r) { return l *  r; }
int op_div(int l, int r) { if (r) return l / r; else return UNDEF; }
int op_add(int l, int r) { return l +  r; }
int op_sub(int l, int r) { return l -  r; }
int op_asl(int l, int r) { return l << r; }
int op_lsr(int l, int r) { return l >> r; }
int op_cle(int l, int r) { return l <= r; }
int op_clt(int l, int r) { return l <  r; }
int op_cge(int l, int r) { return l >= r; }
int op_cgt(int l, int r) { return l >  r; }
int op_ceq(int l, int r) { return l == r; }
int op_cne(int l, int r) { return l != r; }
int op_xor(int l, int r) { return l ^  r; }
int op_and(int l, int r) { return l && r; }
int op_bnd(int l, int r) { return l &  r; }
int op_lor(int l, int r) { return l || r; }
int op_bor(int l, int r) { return l |  r; }


struct binop_struct
{
   const char *op;
   int prio;
   int (*foo)(int,int);
};

#define BINOPS 17

// table of binary operators in C style and priority

struct binop_struct binop[BINOPS] =
{
   {"*" ,11,&op_mul}, //  Multiplication
   {"/" ,11,&op_div}, //  Division
   {"+" ,10,&op_add}, //  Addition
   {"-" ,10,&op_sub}, //  Subtraction
   {"<<", 9,&op_asl}, //  bitwise left  shift
   {">>", 9,&op_lsr}, //  bitwise right shift
   {"<=", 8,&op_cle}, //  less than or equal to
   {"<" , 8,&op_clt}, //  less than
   {">=", 8,&op_cge}, //  greater than or equal to
   {">" , 8,&op_cgt}, //  greater than
   {"==", 7,&op_ceq}, //  equal to
   {"!=", 7,&op_cne}, //  not equal to
   {"^" , 5,&op_xor}, //  bitwise XOR
   {"&&", 3,&op_and}, //  logical AND
   {"&" , 6,&op_bnd}, //  bitwise AND
   {"||", 2,&op_lor}, //  logical OR
   {"|" , 4,&op_bor}  //  bitwise OR
};

// ***********
// EvalOperand
// ***********

char *EvalOperand(char *p, int *v, int prio)
{
   int  i;    // loop index
   int  l;    // length of string
   int  o;    // priority of operator
   int  w;    // value of right operand
   char c;    // current character

   *v = UNDEF; // preset result to UNDEF

   p = SkipSpace(p);
   c = *p;
   if (df) fprintf(df,"EvalOperand <%s>\n",p);

   if (c == ',' )  return p; // comma separator

   // Start parsing unary operators
   // < > * have special 6502 meanings

   if (*p && strchr("[(+-!~<>*$'%?",*p))
   {
      for (i=0 ; i < UNAOPS ; ++i)
      if (*p == unaop[i].op)
      {
          if (df) fprintf(df,"op [%c] * = %4.4x\n", unaop[i].op,(unsigned int)pc);
          p = unaop[i].foo(p,v);
          if (df) fprintf(df,"op [%c] v = %4.4x\n", unaop[i].op,(unsigned int)*v);
          break;
      }
   }
   else if (isdigit(c)) p = EvalDecValue(p,v);    // decimal constant
   else if (isym(c))    p = EvalSymValue(p,v);    // symbol or label
   else
   {
      ErrorLine(p);
      ErrorMsg("Illegal operand\n");
      exit(1);
   }

   // Left operand has been parsed successfully
   // now look for binary operators
   // *v is left  operand
   //  w is right operand

   // the left or right operand may be UNDEFINED in phase 1
   // in that case do not perform the operation, just parse

   p = SkipSpace(p);

   while (*p && strchr("*/+-<>=!&^|",*p))
   {
      // loop through all binary operators

      for (i=0 ; i < BINOPS ; ++i)
      {
         l = strlen((binop[i].op));
         if (!strncmp(p,binop[i].op,l))
         {
            if ((o = binop[i].prio) <= prio) return p;
            p = EvalOperand(p+l,&w,o);
            if (*v == UNDEF || w == UNDEF) *v = UNDEF;
            else *v = binop[i].foo(*v,w);
            break;
         }
      }
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
      PrintPC();
      fprintf(lf," %2.2x %2.2x    %s\n",lo,hi,Line);
   }
   pc += 2;
   return p;
}


char *ParseFillData(char *p)
{
   int i,m,v;

   p = EvalOperand(p,&m,0);
   if (m < 0 || m > 32767)
   {
      ErrorMsg("Illegal FILL multiplier %d\n",m);
      exit(1);
   }
   p = NeedChar(p,'(');
   if (!p)
   {
      ErrorMsg("Missing '(' before FILL value\n");
      exit(1);
   }
   p = EvalOperand(p+1,&v,0);
   v &= 0xff;
   if (Phase == 2)
   {
      for (i=0 ; i < m ; ++i) ROM[pc+i] = v;
      PrintPC();
      if (m > 0) fprintf(lf," %2.2x",v);
      else       fprintf(lf,"   ");
      if (m > 1) fprintf(lf," %2.2x",v);
      else       fprintf(lf,"   ");
      if (m > 2) fprintf(lf," %2.2x",v);
      else       fprintf(lf,"   ");
      fprintf(lf," %s ; %d bytes\n",Line,m);
   }
   pc += m;
   return p;
}

void ListSizeInfo()
{
   int i;

   if (Phase == 2)
   {
      PrintPC();
      fprintf(lf,"          %s",Line);
      i = AddressIndex(ModuleStart);
      if (i >= 0)
      {
          fprintf(lf," ;%5d [%s]",pc-ModuleStart,lab[i].Name);
      }
      fprintf(lf,"\n");
   }
}


char *IncludeFile(char *p)
{
   char FileName[256];
   char *fp;
   p = NeedChar(p,'"');
   if (!p)
   {
      ErrorMsg("Missing quoted filename after .INCLUDE\n");
      exit(1);
   }
   fp = FileName;
   ++p;
   while (*p != 0 && *p != '"') *fp++ = *p++;
   *fp = 0;
   // printf("fopen %s\n",FileName);
   if (IncludeLevel >= 99)
   {
      ErrorMsg("Too many includes nested ( >= 99)\n");
      exit(1);
   }
   sf = fopen(FileName,"r");
   if (!sf)
   {
      printf("Could not open include file <%s>\n",FileName);
      exit(1);
   }
   IncludeStack[IncludeLevel].LiNo = LiNo;
   IncludeStack[++IncludeLevel].fp = sf;
   IncludeStack[IncludeLevel].Src = MallocOrDie(strlen(FileName + 1));
   strcpy(IncludeStack[IncludeLevel].Src, FileName);
   PrintLine();
   LiNo = 0;
   return p;
}

char *ParseCPUData(char *p)
{
   p = SkipSpace(p);
   for (CPU_Type = CPU_6502 ; CPU_Type < CPU_End ; ++CPU_Type)
   {
      if (!Strncasecmp(p,CPU_Name[CPU_Type],strlen(CPU_Name[CPU_Type]))) break;
   }
   if (CPU_Type == CPU_End)
   {
      ErrorMsg("Unsupported CPU type <%s>\n",p);
      exit(1);
   }
   AM_Inherent_Tab = CPU_Property_Tab[CPU_Type].AM_Inherent_Tab;
   AM_Register_Tab = CPU_Property_Tab[CPU_Type].AM_Register_Tab;
   WordOC          = CPU_Property_Tab[CPU_Type].WordOC;
   PrintLine();
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
      ErrorMsg("Illegal start address for STORE %d\n",Start);
      exit(1);
   }
   p = NeedChar(p,',');
   if (!p)
   {
      ErrorMsg("Missing ',' after start address\n");
      exit(1);
   }
   p = EvalOperand(p+1,&Length,0);
   if (Length < 0 || Length > 0x10000)
   {
      ErrorMsg("Illegal length for STORE %d\n",Length);
      exit(1);
   }
   p = NeedChar(p,',');
   if (!p)
   {
      ErrorMsg("Missing ',' after length\n");
      exit(1);
   }
   p = NeedChar(p+1,'"');
   if (!p)
   {
      ErrorMsg("Missing quote for filename\n");
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
      ErrorMsg("number of storage files exceeds %d\n",SFMAX);
      exit(1);
   }
   PrintLine();
   return p;
}


char *ParseBSSData(char *p)
{
   int m;

   p = EvalOperand(p,&m,0);
   if (m < 1 || m > 32767)
   {
      ErrorMsg("Illegal BSS size %d\n",m);
      exit(1);
   }
   if (Phase == 2)
   {
      PrintLiNo(1);
      fprintf(lf,"%4.4x             ",bss);
      fprintf(lf,"%s\n",Line);
   }
   bss += m;
   return p;
}


char *ParseBitData(char *p)
{
   int i,v;

   v = 0;
   for (i=0 ; i < 8 ; ++i)
   {
      v <<= 1;
      p = SkipSpace(p+1);
      if (*p == '*') v |= 1;
      else if (*p != '.')
      {
         ErrorMsg("use only '*' for 1 and '.' for 0 in BITS statement\n");
         exit(1);
      }
   }
   if (Phase == 2)
   {
      PrintPC();
      ROM[pc] = v;
      fprintf(lf," %2.2x       ",v);
      fprintf(lf,"%s\n",Line);
   }
   ++pc;
   return p;
}

char *ParseLitData(char *p)
{
   int i,v;

   v = 0;
   for (i=0 ; i < 8 ; ++i)
   {
      v >>= 1;
      p = SkipSpace(p+1);
      if (*p == '*') v |= 128;
      else if (*p != '.')
      {
         ErrorMsg("use only '*' for 1 and '.' for 0 in LITS statement\n");
         exit(1);
      }
   }
   if (Phase == 2)
   {
      PrintPC();
      ROM[pc] = v;
      fprintf(lf," %2.2x       ",v);
      fprintf(lf,"%s\n",Line);
   }
   ++pc;
   return p;
}

char *ParseASCII(char *p, unsigned char b[], int *l)
{
   char Delimiter;

   Delimiter = *p++;
   while (*p && *p != Delimiter && *l < ML-1)
   {
      if (*p == '\\') // special character CR, LF, NULL
      {
         ++p;
              if (*p == 'r') b[*l] = 13;
         else if (*p == 'n') b[*l] = 10;
         else if (*p == '0') b[*l] =  0;
         else b[*l] = *p;
         ++(*l);
         ++p;
      }
      else
      {
         b[(*l)++] = *p++;
      }
   }
   if (*p == Delimiter) ++p;
   if (*p == '^')
   {
      b[(*l)-1] |= 0x80; // Set bit 7 of last char
      ++p;
   }
   return p;
}

char *ParseByteData(char *p, int Charset)
{
   int i,j,l,v;
   unsigned char ByteBuffer[ML];
   char Delimiter;

   l = 0;
   while (*p && *p != ';') // Parse data line
   {
      p = SkipSpace(p);
      Delimiter = *p;
      if (Delimiter == '<' && p[1] == '"' && p[5] == '"') // Packed ASCII
      {
         ByteBuffer[0] = ((p[3] - 63) << 6) | ((p[4] - 63) << 1);
         l  = 1;
         p += 6;
      }
      else if (Delimiter == '>' && p[1] == '"' && p[5] == '"') // Packed ASCII
      {
         ByteBuffer[0] = ((p[2] - 63) << 3) | ((p[3] - 63) >> 2);
         l  = 1;
         p += 6;
      }
      else if (Delimiter == '"' || Delimiter == '\'')
      {
         i = l; // remember start of string
         p = ParseASCII(p,ByteBuffer,&l);
         if (Charset == PETSCII)
         for (j=i ; j < l ; ++j)
         {
            if (ByteBuffer[j] >= 'A' && ByteBuffer[j] <= 'Z')
                ByteBuffer[j] |= 0x80;
            if (ByteBuffer[j] >= 'a' && ByteBuffer[j] <= 'z')
                ByteBuffer[j] -= 0x20;
         }
         if (Charset == SCREENCODE)
         for (j=i ; j < l ; ++j)
         {
            if (ByteBuffer[j] >= 'a' && ByteBuffer[j] <= 'z')
                ByteBuffer[j] -= 0x60;
         }
         if (df)
         {
            fprintf(df,"String $%4.4x:<",pc);
            for (j=i ; j < l ; ++j) fprintf(df,"%c",ByteBuffer[j]&0x7f);
            fprintf(df,">\n");
         }
      }
      else
      {
         p = EvalOperand(p,&v,0);
         if (v == UNDEF && Phase == 2)
         {
            ErrorMsg("Undefined symbol in BYTE data\n");
            ErrorLine(p);
            exit(1);
         }
         ByteBuffer[l++] = v & 0xff;
         if (Delimiter != '<' && Delimiter != '>' && (v > 255 || v < -127))
         ByteBuffer[l++] = v >> 8;
      }
      p = SkipToComma(p);
      if (*p == ',') ++p;
   }
   if (l < 1)
   {
      ErrorMsg("Missing byte data\n");
      ErrorLine(p);
      exit(1);
   }
   j = AddressIndex(pc);
   if (j >= 0)
   for ( ; j < Labels ; ++j) // There may be multiple lables on this address
   {
       if (lab[j].Address == pc) lab[j].Bytes = l;
   }
   if (j >= 0 && df) fprintf(df,"Byte label [%s] $%4.4x $%4.4x %d bytes\n",
                   lab[j].Name,lab[j].Address,pc,l);
   if (Phase == 2)
   {
      PrintPC();
      for (i=0 ; i < l ; ++i)
      {
         ROM[pc+i] = ByteBuffer[i];
         if (i < 3) fprintf(lf," %2.2x",ByteBuffer[i]);
      }
      for (i=l ; i < 3 ; ++i) fprintf(lf,"   ");
      fprintf(lf," %s\n",Line);
   }
   pc += l;
   return p;
}


char *IsData(char *p)
{
        if (!Strncasecmp(p,"WORD",4))    p = ParseWordData(p+4);
   else if (!Strncasecmp(p,"WOR",3))     p = ParseWordData(p+3);
   else if (!Strncasecmp(p,"BYTE",4))    p = ParseByteData(p+4,ASCII);
   else if (!Strncasecmp(p,"BYT",3))     p = ParseByteData(p+3,ASCII);
   else if (!Strncasecmp(p,"PET",3))     p = ParseByteData(p+3,PETSCII);
   else if (!Strncasecmp(p,"SCREEN",6))  p = ParseByteData(p+6,SCREENCODE);
   else if (!Strncasecmp(p,"BITS",4))    p = ParseBitData(p+4);
   else if (!Strncasecmp(p,"LITS",4))    p = ParseLitData(p+4);
   else if (!Strncasecmp(p,"QUAD",4))    p = ParseLongData(p+4,4);
   else if (!Strncasecmp(p,"REAL",4))    p = ParseRealData(p+4);
   else if (!Strncasecmp(p,"FILL",4))    p = ParseFillData(p+4);
   else if (!Strncasecmp(p,"BSS",3))     p = ParseBSSData(p+4);
   else if (!Strncasecmp(p,"STORE",5))   p = ParseStoreData(p+5);
   else if (!Strncasecmp(p,"CPU",3))     p = ParseCPUData(p+3);
   else if (!Strncasecmp(p,"CASE",4))    p = ParseCaseData(p+4);
   else if (!Strncasecmp(p,"ORG",3))     p = SetPC(p);
   else if (!Strncasecmp(p,"LOAD",4))    WriteLA = 1;
   else if (!Strncasecmp(p,"INCLUDE",7)) p = IncludeFile(p+7);
   else if (!Strncasecmp(p,"SIZE",4))    ListSizeInfo();
   else if (!Strncasecmp(p,"SKI",3))     p += strlen(p);
   else if (!Strncasecmp(p,"PAG",3))     p += strlen(p);
   else if (!Strncasecmp(p,"END",3))     p += strlen(p);
   else
   {
      ErrorMsg("Unknown pseudo op\n");
      ErrorLine(p);
      exit(1);
   }
   if (pc > 0x10000 && pc != UNDEF)
   {
      ErrorMsg("Program counter overflow\n");
      ErrorLine(p);
      exit(1);
   }
   return p;
}


char * SplitOperand(char *p)
{
   int i,l,inquo,inapo,Sule,amo;
   int SuffMatch;
   char Pref;
   char *Suff;
   char *Mnem;

   l       =    0; // length of trimmed operand
   il      =    3; // default instruction length
   am      = Abso; // default address mode
   inquo   =    0; // inside quotes
   inapo   =    0; // inside apostrophes

   // Extract operand

   p = SkipSpace(p);
   while (*p)
   {
      if (*p == '"'  && inapo == 0) inquo = !inquo;
      if (*p == '\'' && inquo == 0) inapo = !inapo;
      if (*p == ';'  && inquo == 0 && inapo == 0) break;
      Operand[l++] = *p++;
   }
   Operand[l] = 0; // end marker
   while (l && isspace(Operand[l-1])) Operand[--l] = 0;

   // Allow BIT mnemonic with missing operand
   // used to skip next 2 byte statement

   if (l == 0 && oc == 0x24)
   {
      oc = 0x2c;
      am = Impl;
      il = 1;
      return p;
   }

   // tolerate Accumulator with missing operand

   if (l == 0 && (CPU_Type <= CPU_65C02) &&
      (oc == 0x0a || oc == 0x2a || oc == 0x4a || oc == 0x6a))
   {
      Operand[0] = 0;
      am = Accu;
      il = 1;
      return p;
   }

   // Check for existing operand

   if (l < 1)
   {
      ErrorLine(p);
      ErrorMsg("Missing operand\n");
      exit(1);
   }

   // Immediate

   if (Operand[0] == '#')
   {
      Operand[0] = ' ';
      am = Imme;
      il = 2;
      return p;
   }

   // Accumulator

   if (l == 1 && (Operand[0] == 'A' || Operand[0] == 'a'))
   {
      Operand[0] = 0;
      am = Accu;
      il = 1;
      return p;
   }

   // Look for a matching combination of mnemonic, prefix and suffix

   Mnem = set[oc].mne; // Current mnemonic

   //  ( ),Y  (  ,X)   ( )   ,Y   ,X
   for (amo = Indy ; amo >= Absx ; amo--)
   {
      Pref = Prefix[amo];
      Suff = Suffix[amo];
      Sule = strlen(Suff);

      // check for Commodore Syntax (ind)y

      if (amo == Indy && l > 2 && Operand[l-2] == ')' &&
         (Operand[l-1] == 'Y' || Operand[l-1] == 'y'))
         SuffMatch = 1;
      else
      SuffMatch = (l > Sule && !Strcasecmp((const char *)(Operand+l-Sule),Suff));

      if ((Operand[0] == Pref || Pref == ' ') && SuffMatch)
      for (i=0 ; i < 256 ; ++i)
      {
          if (CPU_Type >= set[i].cpu &&       // CPU      match ?
          Pref == Prefix[set[i].amo] &&       // Prefix   match ?
          !strcmp(Suffix[set[i].amo],Suff) && // Suffix   match ?
          !strcmp(set[i].mne,Mnem))           // Mnemonic match ?
          {
            am = amo;
            il = Lenfix[am];
            // specual code for (ind)y
            if (amo == Indy && Operand[l-2] == ')') Operand[l-2] = 0;
            else Operand[l-=Sule] = 0;
            if (Pref == '(') Operand[0] = ' ';
            if (df) fprintf(df,"Split {%c} {%s} {%s}\n",Pref,Operand,Suff);
            return p;
          }
      }
   }
   return p;
}


void AdjustOpcode(char *p)
{
   int i;

   if (CPU_Type > CPU_65816) return;

   if (oc == 0x20) am = Abso; // JSR
   if (oc == 0x4c && am == Zpag) am = Abso; // JMP
   if (oc != 0x86 && oc != 0xa2 && am == Zpgy) am = Absy; // Only LDX/STX
   if (oc == 0x4c && am == Indi)
   {
      oc = 0x6c; // JMP (nnnn)
      il = 3;
      return;
   }
   if (oc == 0x4c && am == Indx)
   {
      oc = 0x7c; // JMP (nnnn,X)
      il = 3;
      return;
   }
   for (i=0 ; i < 256 ; ++i)
   {
      if (!strcmp(set[oc].mne,set[i].mne) && am == set[i].amo)
      {
         oc = i;
         il = Lenfix[set[i].amo];
         return;
      }
   }
   if (oc == 0x2c && am == Impl) return; // BIT

   ErrorMsg("Illegal address mode %d for %s\n",am,set[oc].mne);
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
   int r,v,Ifdef,Ifval;
   r = 0;
   if (*p != '#') return 0; // No preprocessing
   p = SkipSpace(p+1);
   if (!Strncasecmp(p,"error", 5) && (Phase == 1))
   {
      CheckSkip();
      if (Skipping)
         return 0;          // Include line in listing
      char *msg = MallocOrDie(strlen(p+6) + 2);
      strcpy(msg, p+6);
      strcat(msg, "\n");
      ErrorMsg(msg);
      free(msg);
      exit(1);
   }
   Ifdef = !Strncasecmp(p,"ifdef ",6);
   Ifval = !Strncasecmp(p,"if "   ,3);
   if (Ifdef || Ifval)
   {
      r = 1;
      IfLevel++;
      if (IfLevel > 9)
      {
         ++ErrNum;
         ErrorMsg("More than 10  #IF or #IFDEF conditions nested\n");
         exit(1);
      }
      if (Ifdef)
      {
         p = EvalOperand(p+6,&v,0);
         SkipLine[IfLevel] = v == UNDEF;
      }
      else // if (Ifval)
      {
         p = EvalOperand(p+3,&v,0);
         SkipLine[IfLevel] = v == UNDEF || v == 0;
      }
      CheckSkip();
      if (Phase == 2)
      {
         PrintLiNo(1);
         if (SkipLine[IfLevel])
            fprintf(lf,"%4.4x FALSE    %s\n",SkipLine[IfLevel],Line);
         else
            fprintf(lf,"0000 TRUE     %s\n",Line);
      }
      if (df) fprintf(df,"%5d %4.4x          %s\n",LiNo,SkipLine[IfLevel],Line);
   }
   else if (!Strncasecmp(p,"else",4) && (p[4] == 0 || isspace(p[4])))
   {
      r = 1;
      SkipLine[IfLevel] = !SkipLine[IfLevel];
      CheckSkip();
      PrintLiNo(1);
      if (Phase == 2) fprintf(lf,"              %s\n",Line);
   }
   if (!Strncasecmp(p,"endif",5) && (p[5] == 0 || isspace(p[5])))
   {
      r = 1;
      IfLevel--;
      PrintLiNo(1);
      if (Phase == 2) fprintf(lf,"              %s\n",Line);
      if (IfLevel < 0)
      {
         ++ErrNum;
         ErrorMsg("endif without if\n");
         exit(1);
      }
      CheckSkip();
      if (df) fprintf(df,"ENDIF SkipLevel[%d]=%d\n",IfLevel,SkipLine[IfLevel]);
   }
   return r;
}


char *GenerateCode(char *p)
{
   int ibi = 0; // instruction byte index
   int v,lo,hi;
   char *o;

   // initialize

   v = 0;
   o = (char *)Operand;

   if (pc < 0)
   {
      ErrorLine(p);
      ErrorMsg("Undefined program counter (PC)\n");
      exit(1);
   }
   if (df) fprintf(df,"GenerateCode %4.4X %s\n",pc,p);

   // inherent instruction (no operand)

   if (am == AM_Inherent)
   {
      il = 1 + (oc > 255); // instruction length
      p += strlen(Mne) ;   // skip mnemonic
      if (OperandExists(p))
      {
         ErrorLine(p);
         ErrorMsg("Inherent/Implied address mode must not have operands\n");
         exit(1);
      }
   }

   else
   {
      p = SkipSpace(p+strlen(set[oc].mne)) ; // Skip mnemonic & space
      p = SplitOperand(p);
   }

   if (Operand[0])
   {
      if (Operand[0] == 0x27) // apostrophe
      {
         ErrorLine(p);
         ErrorMsg("Operand cannot start with apostrophe\n");
         exit(1);
      }
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
            ErrorLine(p);
            ErrorMsg("Immediate value out of range (%d)\n",v);
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
            ErrorLine(p);
            ErrorMsg("Branch to undefined label\n");
            exit(1);
         }
         if (Phase == 2 && (v < -128 || v > 127))
         {
            ErrorLine(p);
            ErrorMsg("Branch too long (%d)\n",v);
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
      ErrorLine(p);
      ErrorMsg("Operand missing\n");
      exit(1);
   }
   if (*o && *o != ';')
   {
      ErrorLine(p);
      ErrorMsg("Operand syntax error\n<%s>\n",o);
      exit(1);
   }
   AdjustOpcode(p);
   if (Phase == 2)
   {
      if (v == UNDEF)
      {
         ErrorLine(p);
         ErrorMsg("Use of an undefined label\n");
         exit(1);
      }

      lo = v & 0xff;
      hi = v >> 8;

      if (il < 3 && (v < -128 || v > 255))
      {
         ++ErrNum;
         ErrorMsg("Not a byte value : %d\n",v);
      }

      // insert binary code

      if (oc > 255) // two byte opcode
      {
         ROM[pc  ] = oc >> 8;
         ROM[pc+1] = oc;
         ibi = 2;
      }
      else
      {
         ROM[pc] = oc;
         ibi = 1;
      }

      if (il > ibi  ) ROM[pc+ibi  ] = lo;
      if (il > ibi+1) ROM[pc+ibi+1] = hi;

      PrintPC();
      PrintOC();
      if (il > 1) fprintf(lf," %2.2x",lo);
      else        fprintf(lf,"   ");
      if (il > 2) fprintf(lf," %2.2x",hi);
      else        fprintf(lf,"   ");
      fprintf(lf," %s",Line);
      if (LengthInfo[0]) fprintf(lf," %s",LengthInfo);
      fprintf(lf,"\n");
      LengthInfo[0] = 0;
   }
   if (il < 1 || il > 3)
   {
      ++ErrNum;
      ErrorMsg("Wrong instruction length = %d\n",il);
      il = 1;
   }
   if (pc+il > 0xffff)
   {
      if (Phase > 1)
      {
         ++ErrNum;
         ErrorMsg("Program counter exceeds 64 KB\n");
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
         ErrorMsg("Syntax error in macro definition '%c'\n",*p);
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
      ErrorMsg("Too many macros (> %d)\n",MAXMAC);
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
         Mac[j].Name = MallocOrDie(l+1);
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
                  if (al && !StrnCmp(p,at,al))
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
               Mac[j].Body = MallocOrDie(bl);
               strcpy(Mac[j].Body,Buf);
            }
            else
            {
               bl += l;
               Mac[j].Body = ReallocOrDie(Mac[j].Body,bl);
               strcat(Mac[j].Body,Buf);
            }
            fgets(Line,sizeof(Line),sf);
         }
         Macros++;
      }
      else if (Phase == 2) // List macro
      {
         PrintLiNo(1);
         ++LiNo;
         fprintf(lf,"            %s\n",Line);
         do
         {
            fgets(Line,sizeof(Line),sf);
            PrintLiNo(1);
            ++LiNo;
            fprintf(lf,"            %s",Line);
            if (pf) fprintf(pf,"%s",Line);
         } while (!feof(sf) && !Strcasestr(Line,"ENDMAC"));
         LiNo-=2;
      }
      else if (Phase == 1)
      {
         ++ErrNum;
         ErrorMsg("Duplicate macro [%s]\n",Macro);
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
      ErrorMsg("Wrong # of arguments in [%s] called (%d) defined (%d)\n",
            Macro,an,Mac[j].Narg);
      exit(1);
   }
   CurrentMacro = j;
   ++InsideMacro;
   MacroPointer = Mac[j].Body;

   if (Phase == 2)
   {
      Mac[j].Cola = m - Line;
      PrintLine();
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
         if (*MacroPointer == '&' && isdigit(MacroPointer[1]))
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
      MacroStopped = 1;
   }
   *w = 0;
}


void ParseLine(char *cp)
{
   int i,v,m;

   am = -1;
   oc = -1;
   Label[0] = 0;
   Operand[0] = 0;
   Comment[0] = 0;
   cp = SkipHexCode(cp);        // Skip disassembly
   cp = SkipSpace(cp);          // Skip leading blanks
   if (!strncmp(cp,"; **",4)) ModuleTrigger = LiNo;
   if (CheckCondition(cp)) return;
   if (Skipping)
   {
      PrintLiNo(1);
      if (Phase == 2) fprintf(lf,"SKIP          %s\n",Line);
      if (df)         fprintf(df,"%5d SKIP          %s\n",LiNo,Line);
      return;
   }
   if (pf && Phase == 2 && !InsideMacro)
   {
       if (MacroStopped) MacroStopped = 0;
       else fprintf(pf,"%s\n",Line); // write to preprocessed file
   }
   if (*cp == 0 || *cp == ';')  // Empty or comment only
   {
      if (Phase == 2)
      {
          if (*cp == ';') PrintLine();
          else            PrintLiNo(-1);
      }
      return;
   }
   if (isalpha(*cp))            // Macro, Label or mnemonic
   {
      if (!Strncasecmp(cp,"MACRO ",6))
      {
         RecordMacro(cp+6);
         return;
      }
      if ((oc = IsInstruction(cp)) < 0)
      {
         m = ExpandMacro(cp);
         if (m < 0)
         {
            cp = DefineLabel(cp,&v,0);
            if (ModuleTrigger == LiNo-1) ModuleStart = v;
            cp = SkipSpace(cp);         // Skip leading blanks
            if (*cp) ExpandMacro(cp);   // Macro after label
         }
         if (*cp == 0 || *cp == ';') // no code or data
         {
            PrintLiNo(1);
            if (Phase == 2)
               fprintf(lf,"%4.4x          %s\n",v&0xffff,Line);
            return;
         }
      }
   }
   if (*cp ==  0 ) return;             // No code
   if (*cp == '*') cp = SetPC(cp);     // Set program counter
   if (*cp == '&') cp = SetBSS(cp);    // Set BSS counter
   if (*cp == '.') cp = IsData(cp+1);  // Data
   if (ForcedEnd) return;
   if (oc < 0) oc = IsInstruction(cp); // Check for mnemonic after label
   if (oc >= 0)
   {
      if (Phase == 2 && oc == 0x60 && ModuleStart != UNDEF)
      {
         i = AddressIndex(ModuleStart);
         if (i >= 0)
         {
            sprintf(LengthInfo,";Size%5d [%s]",
               pc-ModuleStart+1,lab[i].Name);
         }
      }
      GenerateCode(cp);
   }
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


int CloseInclude(void)
{
   PrintLiNo(1);
   if (Phase == 2)
   {
      fprintf(lf,";                       closed INCLUDE file %s\n",
            IncludeStack[IncludeLevel].Src);
   }
   fclose(sf);
   free(IncludeStack[IncludeLevel].Src);
   sf = IncludeStack[--IncludeLevel].fp;
   LiNo = IncludeStack[IncludeLevel].LiNo;
   fgets(Line,sizeof(Line),sf);
   ForcedEnd = 0;
   return feof(sf);
}

void Phase1(void)
{
    int l,Eof;

   Phase = 1;
   ForcedEnd = 0;
   fgets(Line,sizeof(Line),sf);
   Eof = feof(sf);
   while (!Eof || IncludeLevel > 0)
   {
      if (df) fprintf(df,"Phase 1:%s",Line);
      ++LiNo; ++TotalLiNo;
      l = strlen(Line);
      if (l && Line[l-1] == 10) Line[--l] = 0; // Remove linefeed
      if (l && Line[l-1] == 13) Line[--l] = 0; // Remove return
      ParseLine(Line);
      if (df) Phase1Listing();
      if (InsideMacro) NextMacLine(Line);
      else
      {
         fgets(Line,sizeof(Line),sf);
      }
      Eof = feof(sf) || ForcedEnd;;
      if (Eof && IncludeLevel > 0) Eof = CloseInclude();
   }
}


void Phase2(void)
{
    int l,Eof;

   Phase =  2;
   pc    = -1;
   ForcedEnd = 0;
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
   LiNo = 0; TotalLiNo = 0;
   fgets(Line,sizeof(Line),sf);
   Eof = feof(sf);
   while (!Eof || IncludeLevel > 0)
   {
      ++LiNo; ++TotalLiNo;
      l = strlen(Line);
      if (l && Line[l-1] == 10) Line[--l] = 0; // Remove linefeed
      if (l && Line[l-1] == 13) Line[--l] = 0; // Remove return
      ParseLine(Line);
      if (InsideMacro) NextMacLine(Line);
      else fgets(Line,sizeof(Line),sf);
      Eof = feof(sf) || ForcedEnd;
      if (Eof && IncludeLevel > 0) Eof = CloseInclude();
      if (df) fprintf(df,"Phase 2:[%s] EOF=%d\n",Line,Eof);
      if (Eof && IncludeLevel > 0) Eof = CloseInclude();
      if (GenEnd < pc) GenEnd = pc; // Remember highest assenble address
      if (ErrNum >= ERRMAX)
      {
         printf("\n*** Error count reached maximum of %d ***\n",ErrNum);
         printf("Assembly stopped\n");
         return;
      }
   }
}


void ListSymbols(FILE *lf, int n, int lb, int ub)
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
         if ((A != ' ' || (j % 5) != 4) && j != lab[i].NumRef)
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
         lab[i].Name = ReallocOrDie(lab[i].Name,strlen(lab[i].Name)+strlen(lab[k].Name)+2);
         strcat(lab[i].Name,"/");
         strcat(lab[i].Name,lab[k].Name);
         lab[i].Ref = ReallocOrDie(lab[i].Ref,n*sizeof(int));
         lab[i].Att = ReallocOrDie(lab[i].Att,n*sizeof(int));
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
      if (lab[i].Address == UNDEF)
         printf("* Undefined   : %-25.25s *\n",lab[i].Name);
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
      if (WriteLA)
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

const char *StatOn  = "On ";
const char *StatOff = "Off";

const char *Stat(int o)
{
   if (o) return StatOn ;
   else   return StatOff;
}

int main(int argc, char *argv[])
{
   int ic,v;

   for (ic=1 ; ic < argc ; ++ic)
   {
      if (!strcmp(argv[ic],"-x")) SkipHex = 1;
      else if (!strcmp(argv[ic],"-d")) Debug = 1;
      else if (!strcmp(argv[ic],"-i")) IgnoreCase = 1;
      else if (!strcmp(argv[ic],"-n")) WithLiNo = 1;
      else if (!strcmp(argv[ic],"-p")) Preprocess = 1;
      else if (!strncmp(argv[ic],"-D",2)) DefineLabel(argv[ic]+2,&v,1);
      else if (argv[ic][0] >= '0' || argv[ic][0] == '.')
      {
         if (!Src)
         {
              Src = MallocOrDie(strlen(argv[ic]) + 4 + 1);
              strcpy(Src,argv[ic]);
         }
         else if (!Lst[0]) strcpy(Lst,argv[ic]);
      }
      else
      {
         printf("\nUsage: bsa [-d -D -i -x] <source> <list>\n");
         exit(1);
      }
   }
   if (!Src)
   {
      printf("*** missing filename for assembler source file ***\n");
      printf("\nUsage: bsa [-d -D -i -n -x] <source> [<list>]\n");
      printf("   -d print details in file <Debug.lst>\n");
      printf("   -D Define symbols\n");
      printf("   -i ignore case in symbols\n");
      printf("   -n include line numbers in listing\n");
      printf("   -p print preprocessed source\n");
      printf("   -x assemble listing file - skip hex in front\n");
      exit(1);
   }

   // default file names if only source file specified:
   // prog.asm   prog   prog.lst

   strcpy(Pre,Src);
   strcat(Pre,".pp");
   if (!Lst[0]) strcpy(Lst,Src);
   if (!(strlen(Src) > 4 && !Strcasecmp(Src+strlen(Src)-4,".asm")))
       strcat(Src,".asm");
   if ( (strlen(Lst) > 4 && !Strcasecmp(Lst+strlen(Lst)-4,".asm")))
       Lst[strlen(Lst)-4] = 0;
   if (!(strlen(Lst) > 4 && !Strcasecmp(Lst+strlen(Lst)-4,".lst")))
       strcat(Lst,".lst");

   printf("\n");
   printf("*******************************************\n");
   printf("* Bit Shift Assembler 25-May-2020         *\n");
   printf("* --------------------------------------- *\n");
   printf("* Source: %-31.31s *\n",Src);
   printf("* List  : %-31.31s *\n",Lst);
   printf("* -d:%s     -i:%s     -n:%s     -x:%s *\n",
         Stat(Debug),Stat(IgnoreCase),Stat(WithLiNo),Stat(SkipHex));
   printf("*******************************************\n");

   sf = fopen(Src,"r");
   if (!sf)
   {
      printf("Could not open <%s>\n",Src);
      exit(1);
   }
   IncludeStack[0].fp = sf;
   IncludeStack[0].Src = Src;
   lf = fopen(Lst,"w");  // Listing
   if (Debug) df = fopen("Debug.lst","w");
   if (Preprocess) pf = fopen(Pre,"w");

   Phase1();
   Phase2();
   WriteBinaries();
   ListUndefinedSymbols();
   PairSymbols();
   qsort(lab,Labels,sizeof(struct LabelStruct),CmpAddress);
   fprintf(lf,"\n\n%5d Symbols\n",Labels);
   fprintf(lf,"-------------\n");
   ListSymbols(lf,Labels,0,0xffff);
   qsort(lab,Labels,sizeof(struct LabelStruct),CmpRefs);
   ListSymbols(lf,Labels,0,0xff);
   ListSymbols(lf,Labels,0,0x4000);
   fclose(sf);
   fclose(lf);
   if (df) fclose(df);
   printf("* Source Lines: %6d                    *\n",TotalLiNo);
   printf("* Symbols     : %6d                    *\n",Labels);
   printf("* Macros      : %6d                    *\n",Macros);
   printf("*******************************************\n");
   if (ErrNum)
      printf("* %3d error%s occured%s                      *\n",
             ErrNum, ErrNum == 1 ? "" : "s", ErrNum == 1 ? " " : "");
   else printf("* OK, no errors                           *\n");
   printf("*******************************************\n");
   printf("\n");
   return 0;
}
