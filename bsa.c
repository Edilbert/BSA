/*

*******************
Bit Shift Assembler
*******************

Version: 10-Jan-2023

The assembler was developed and tested on a MAC with OS Catalina.
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
.CPU  45GS02                   set target CPU (6502,65C02,45GS02)
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

// ****************************
// CPU types of the 6502 family
// ****************************

#define CPU_6502    1
#define CPU_65SC02  2
#define CPU_65C02   4
#define CPU_45GS02  8
#define CPU_65816  16

#define C45 0xf7
#define C16 0xef

#define CPU_NAMES 5
char *CPU_Names[CPU_NAMES] =
{
   "6502"   , // Commodore, Atari, Apple, Acorn BBC
   "65SC02" , // Apple IIc
   "65C02"  , // Apple IIc, Apple IIe
   "45GS02" , // Commodore C65, MEGA65
   "65816"    // Apple IIgs, C256 Foenix
};

int   CPU_Type = CPU_6502;
char *CPU_Name = "6502";

#define AM_None -1
#define AM_Dpag  0
#define AM_Abso  1
#define AM_Dpgx  2
#define AM_Absx  3
#define AM_Indx  4
#define AM_Imme  5
#define AM_Indy  6
#define AM_Absy  7
#define AM_Indz  8
#define AM_Rela  9
#define AM_Relo 10
#define AM_Bits 11
#define AM_Impl 12
#define AM_Indi 13
#define AM_Quad 14

#define AM_MODES 15

// ***********************************
// Mnemonics with implied address mode
// ***********************************

struct ImpStruct
{
   char Mne[4];       // Mnemonic
   int  CPU;          // CPU type
   int  Opc;          // Opcodes
} Imp[] =
{
   {"BRK",0,0x00},
   {"PHP",0,0x08},
   {"ASL",0,0x0a}, // ASL A
   {"CLC",0,0x18},
   {"PLP",0,0x28},
   {"ROL",0,0x2a}, // ROL A
   {"BIT",0,0x2c}, // SKIP 2 byte
   {"SEC",0,0x38},
   {"RTI",0,0x40},
   {"ASR",0,0x43}, // ASR A
   {"PHA",0,0x48},
   {"LSR",0,0x4a}, // LSR A
   {"CLI",0,0x58},
   {"RTS",0,0x60},
   {"PLA",0,0x68},
   {"ROR",0,0x6a}, // ROR A
   {"SEI",0,0x78},
   {"DEY",0,0x88},
   {"TXA",0,0x8a},
   {"TYA",0,0x98},
   {"TXS",0,0x9a},
   {"TAY",0,0xa8},
   {"TAX",0,0xaa},
   {"CLV",0,0xb8},
   {"TSX",0,0xba},
   {"INY",0,0xc8},
   {"DEX",0,0xca},
   {"CLD",0,0xd8},
   {"INX",0,0xe8},
   {"NOP",0,0xea},
   {"SED",0,0xf8},

   // not for 6502

   {"INA",1,0x1a}, // INC A
   {"INC",1,0x1a}, // INC A
   {"DEA",1,0x3a}, // DEC A
   {"DEC",1,0x3a}, // DEC A
   {"PHY",1,0x5a},
   {"PLY",1,0x7a},
   {"PHX",1,0xda},
   {"PLX",1,0xfa},

   // 45GS02 only

   {"CLE",C45,0x02},
   {"SEE",C45,0x03},
   {"TSY",C45,0x0b},
   {"INZ",C45,0x1b},
   {"TYS",C45,0x2b},
   {"DEZ",C45,0x3b},
   {"NEG",C45,0x42}, // NEG A
   {"TAZ",C45,0x4b},
   {"TAB",C45,0x5b},
   {"AUG",C45,0x5c},
   {"MAP",C45,0x5c}, // AUG
   {"TZA",C45,0x6b},
   {"TBA",C45,0x7b},
   {"PHZ",C45,0xdb},
   {"EOM",C45,0xea}, // NOP
   {"PLZ",C45,0xfb},

   // 65802 & 65816

   {"PHD",C16,0x0b},
   {"TCS",C16,0x1b},
   {"PLD",C16,0x2b},
   {"TSA",C16,0x3b},
   {"TSC",C16,0x3b},
   {"WDM",C16,0x42},
   {"MVP",C16,0x44},
   {"PHK",C16,0x4b},
   {"MVN",C16,0x54},
   {"TCD",C16,0x5b},
   {"RTL",C16,0x6b},
   {"TDC",C16,0x7b},
   {"PHB",C16,0x8b},
   {"PLB",C16,0xab},
   {"TYX",C16,0xbb},
   {"WAI",C16,0xcb},
   {"STP",C16,0xdb},
   {"SWA",C16,0xeb},
   {"XBA",C16,0xeb},
   {"XCE",C16,0xfb},
};

#define IMPS (sizeof(Imp) / sizeof(struct ImpStruct))

struct RelStruct
{
   char Mne[4];       // Mnemonic
   int  CPU;          // CPU type
   int  Opc;          // Opcodes
} Rel[] =
{
   {"BPL",0,0x10},
   {"BMI",0,0x30},
   {"BVC",0,0x50},
   {"BVS",0,0x70},
   {"BCC",0,0x90},
   {"BCS",0,0xb0},
   {"BNE",0,0xd0},
   {"BEQ",0,0xf0},

   // not for 6502

   {"BRA",1,0x80},
   {"BRU",1,0x80},
   {"BSR",1,0x63}
};

#define RELS (sizeof(Rel) / sizeof(struct RelStruct))

struct BitStruct
{
   char Mne[4];       // Mnemonic
   int  CPU;          // CPU type
   int  Opc;          // Opcodes
} Bit[] =
{
   {"RMB",C45,0x07},
   {"SMB",C45,0x87},
   {"BBR",C45,0x0f},
   {"BBS",C45,0x8f}
};

#define BITS (sizeof(Bit) / sizeof(struct BitStruct))

int GenIndex;
int JMPIndex;
int JSRIndex;
int BITIndex;
int STYIndex;
int PHWIndex;
int LDAIndex;
int STAIndex;
int NegNeg;
int PreNop;

struct GenStruct
{
   char Mne[5];        // Mnemonic
   int  CPU;           // CPU type
   int  Opc[AM_MODES]; // Opcodes
} Gen[] =
{
   //             0    1    2    3    4    5    6    7    8
   //     CPU    DP  Abs DP,X Ab,X (,X)    # (),Y Ab,Y (),Z
   // -----------------------------------------------------
   {"ORA",  0,{0x05,0x0d,0x15,0x1d,0x01,0x09,0x11,0x19,0x12}}, //  0
   {"AND",  0,{0x25,0x2d,0x35,0x3d,0x21,0x29,0x31,0x39,0x32}}, //  1
   {"EOR",  0,{0x45,0x4d,0x55,0x5d,0x41,0x49,0x51,0x59,0x52}}, //  2
   {"ADC",  0,{0x65,0x6d,0x75,0x7d,0x61,0x69,0x71,0x79,0x72}}, //  3
   {"STA",  0,{0x85,0x8d,0x95,0x9d,0x81,  -1,0x91,0x99,0x92}}, //  4
   {"LDA",  0,{0xa5,0xad,0xb5,0xbd,0xa1,0xa9,0xb1,0xb9,0xb2}}, //  5
   {"CMP",  0,{0xc5,0xcd,0xd5,0xdd,0xc1,0xc9,0xd1,0xd9,0xd2}}, //  6
   {"SBC",  0,{0xe5,0xed,0xf5,0xfd,0xe1,0xe9,0xf1,0xf9,0xf2}}, //  7

   {"ASL",  0,{0x06,0x0e,0x16,0x1e,  -1,  -1,  -1,  -1,  -1}}, //  8
   {"ROL",  0,{0x26,0x2e,0x36,0x3e,  -1,  -1,  -1,  -1,  -1}}, //  9
   {"LSR",  0,{0x46,0x4e,0x56,0x5e,  -1,  -1,  -1,  -1,  -1}}, // 10
   {"ROR",  0,{0x66,0x6e,0x76,0x7e,  -1,  -1,  -1,  -1,  -1}}, // 11
   {"DEC",  0,{0xc6,0xce,0xd6,0xde,  -1,  -1,  -1,  -1,  -1}}, // 12
   {"INC",  0,{0xe6,0xee,0xf6,0xfe,  -1,  -1,  -1,  -1,  -1}}, // 13
   {"ASR",C45,{0x44,  -1,0x54,  -1,  -1,  -1,  -1,  -1,  -1}}, // 14
   {"BIT",  0,{0x24,0x2c,0x34,0x3c,  -1,0x89,  -1,  -1,  -1}}, // 15

   {"JMP",  0,{  -1,0x4c,  -1,  -1,0x7c,  -1,  -1,  -1,0x6c}}, // 16
   {"JSR",  0,{  -1,0x20,  -1,  -1,0x23,  -1,  -1,  -1,0x22}}, // 17

   {"CPX",  0,{0xe4,0xec,  -1,  -1,  -1,0xe0,  -1,  -1,  -1}}, // 18
   {"CPY",  0,{0xc4,0xcc,  -1,  -1,  -1,0xc0,  -1,  -1,  -1}}, // 19
   {"LDX",  0,{0xa6,0xae,  -1,  -1,  -1,0xa2,  -1,0xbe,  -1}}, // 20
   {"LDY",  0,{0xa4,0xac,0xb4,0xbc,  -1,0xa0,  -1,  -1,  -1}}, // 21
   {"STX",  0,{0x86,0x8e,  -1,  -1,  -1,  -1,  -1,0x9b,  -1}}, // 22
   {"STY",  0,{0x84,0x8c,0x94,0x8b,  -1,  -1,  -1,  -1,  -1}}, // 23

   {"STZ",  1,{0x64,0x9c,0x74,0x9e,  -1,  -1,  -1,  -1,  -1}}, // 24
   {"CPZ",C45,{0xd4,0xdc,  -1,  -1,  -1,0xc2,  -1,  -1,  -1}}, // 25
   {"LDZ",C45,{  -1,0xab,  -1,0xbb,  -1,0xa3,  -1,  -1,  -1}}, // 26

   {"ASW",C45,{  -1,0xcb,  -1,  -1,  -1,  -1,  -1,  -1,  -1}},
   {"ROW",C45,{  -1,0xeb,  -1,  -1,  -1,  -1,  -1,  -1,  -1}},
   {"DEW",C45,{0xc3,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1}},
   {"INW",C45,{0xe3,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1}},
   {"PHW",C45,{  -1,0xfc,  -1,  -1,  -1,0xf4,  -1,  -1,  -1}},
   {"TSB",C45,{0x04,0x0c,  -1,  -1,  -1,  -1,  -1,  -1,  -1}},
   {"TRB",C45,{0x14,0x1c,  -1,  -1,  -1,  -1,  -1,  -1,  -1}}
 };

#define GENS (sizeof(Gen) / sizeof(struct GenStruct))

// menmonics for 45GS02 32 bit instructions with the Q register
// the Q register is a combination of A,X,Y,Z
// address modes are:
// accumulator         LSR  Q       Q
// base page quad      LDQ  dp      8 bit address
// absolute  quad      LDQ  nnnn   16 bit address
// indirect  quad      LDQ  (dp)   16 bit address indirect
// indirect  quad      LDQ  [dp]   32 bit address indirect

// the opcodes are derived from the equivalent 6502 opcodes for A
// the prefix for using Q instead of A is $4242 (NEG NEG)
// for the 32 bit address mode is $4242ea (NEG NEG NOP)

// The order of the following mnemonics MUST coincide with
// the order of the equivalent 6502 mnemonic above

char *MneQ[16] =
{
   "ORQ" , //  0
   "ANDQ", //  1
   "EORQ", //  2
   "ADCQ", //  3
   "STQ" , //  4
   "LDQ" , //  5
   "CMPQ", //  6
   "SBCQ", //  7

   "ASLQ", //  8
   "ROLQ", //  9
   "LSRQ", // 10
   "RORQ", // 11
   "DEQ" , // 12
   "INQ" , // 13
   "ASRQ", // 14
   "BITQ"  // 15
};



struct IndStruct
{
   char Mne[4];       // Mnemonic
   int  CPU;          // CPU type
   int  Opc;          // Opcodes
} Ind[] =
{
// 6502
   {"JMP",0,0x6c},
// 65SC02
   {"ORA",1,0x12},
   {"AND",1,0x32},
   {"EOR",1,0x52},
   {"ADC",1,0x72},
   {"STA",1,0x92},
   {"LDA",1,0xb2},
   {"CMP",1,0xd2},
   {"SBC",1,0xf2}
};

#define DIMOP_6502  139
#define DIMOP_65C02 (sizeof(Mat) / sizeof(struct MatStruct))

int DimOp = DIMOP_6502;

// ********
// GetIndex
// ********

unsigned int GetIndex(char *mne)
{
   unsigned int i;

   for (i=0 ; i < GENS ; ++i)
   {
      if (!strcmp(Gen[i].Mne,mne)) return i;
   }
   fprintf(stderr,"\n*** internal error in GetIndex(%s) ***\n",mne);
   exit(1);
}

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



#define UNDEF 0xff0000

int BranchOpt;      // Branch optimisation
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
int WordOC;         // List 2 byte opcodes as word

unsigned char ROM_Fill;

int ERRMAX = 10;    // Stop assemby after ERRMAX errors
int LoadAddress = UNDEF;
char *MacroPointer;

const char *Mne;       // Current mnemonic

int bp;      // base page
int oc;      // op code
int am;      // address mode
int il;      // instruction length
int ml;      // mnemonic length
int pc = -1; // program counter
int bss;     // bss counter
int Pass;
int o16;     // force 16 bit operand
#define MAXPASS 20
int BOC[MAXPASS];       // branch opt count
int MaxPass = MAXPASS;
int BSO_Mode;
int IfLevel;
int Skipping;
int SkipLine[10];
int ForcedEnd;    // Triggered by .END command
int IgnoreCase;   // 1: Ignore case for symbols

#define ASCII      0
#define PETSCII    1
#define SCREENCODE 2

// Filenames

char  Src[256];
char  Lst[256];
char  Pre[256];
char  Ext[5];

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
char LengthInfo[ML];
char Scope[ML];

#define LDEF 20
#define LBSS 21
#define LPOS 22

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

int isym(char *c)
{
   if (*c == '_' || *c == '$' || *c == '.' || isalnum(*c)) return 1;
   if (*c == '@' && isalpha(c[1])) return 1;
   return 0;
}

// *****
// isnnd
// *****

int isnnd(char *p)
{
   char *dollar;

   // check for labels of style nn$ used in the Commodore 65 sources
   // to be compiled with the VAX BSO assembler

   dollar = strchr(p,'$');
   if (!dollar) return 0;
   while (p < dollar)
   {
      if (!isdigit(*p++)) return 0;
   }
   return 1;
}


// *********
// GetSymbol
// *********

char *GetSymbol(char *p, char *s)
{
    char *dfs = s;


   // expand BSO local symbols like 40$ to Label_40$

   if (isnnd(p))
   {
      if (Scope[0])
      {
         strcpy(s,Scope);
         s += strlen(s);
         *s++ = '_';
      }
      do *s++ = *p;
      while (*p++ != '$');
      *s = 0;
      return p;
   }

   // check for local symbols inside modules

   if (!BSO_Mode && (*p == '.' || *p == '_') && Scope[0]) // expand local symbol
   {
      strcpy(s,Scope);  // module name
      s += strlen(s);   // advance pointer after module name
      *s++ = *p++;      // copy local identifier
   }

   // copy alphanumeric characters to symbol

   while (isym(p)) *s++ = *p++;

   // terminate string

   *s = 0;

   // debug output

   if (df)
   {
      fprintf(df,"GetSymbol:");
      if (Scope[0]) fprintf(df,"Scope:[%s]",Scope);
      fprintf(df,"%s\n",dfs);
   }
   return p;
}


// **********
// NextSymbol
// **********

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
   if (ep < 0 || ep > 79) return;

   for (i=0 ; i < ep ; ++i) printf(" ");
   printf("^\n");
}

void ListSymbols(FILE *lf, int n, int lb, int ub);

// ********
// ErrorMsg
// ********

void ErrorMsg(const char *format, ...)
{
   va_list args;
   char buf[1024];
   memset(buf,0,sizeof(buf));
   snprintf(buf, sizeof(buf)-1, "\n*** Error in file %s line %d:\n",
         IncludeStack[IncludeLevel].Src, LiNo);
   va_start(args,format);
   vsnprintf(buf+strlen(buf), sizeof(buf)-strlen(buf)-1, format, args);
   va_end(args);
   fputs(buf, stdout);
   fputs(buf, lf);
   if (df) fputs(buf, df);
}

void PrintLiNo(int Blank)
{
   if (Pass < MaxPass) return;
   if (WithLiNo)
   {
      fprintf(lf,"%5d",LiNo);
      if (Blank ==  1) fprintf(lf," ");
   }
   if (Blank == -1) fprintf(lf,"\n");
}

void PrintPC(void)
{
   if (Pass < MaxPass) return;
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
   if (Pass < MaxPass) return;
   PrintLiNo(1);
   fprintf(lf,"              %s\n",Line);
}

void PrintPCLine(void)
{
   if (Pass < MaxPass) return;
   PrintPC();
   fprintf(lf,"          %s\n",Line);
}

int OperandExists(char *p)
{
   while (isspace(*p)) ++p;
   if (*p == ';' || *p ==  0 ) return 0;
   if (*p != 'A' && *p != 'a' && *p != 'Q' && *p != 'q') return 1;
   if (CPU_Type != CPU_45GS02 && (*p == 'Q' || *p == 'q')) return 1;

   // treat accumulator mode as implied

   ++p;
   while (isspace(*p)) ++p;
   return (*p != ';' && *p !=  0 );
}

int Qumulator(char *p)
{
   while (isspace(*p)) ++p;
   if (*p != 'Q' && *p != 'q') return 0;
   if (p[1] == 0) return 1;
   if (isym(p+1)) return 0;
   return 1;
}

// *************
// IsInstruction
// *************

int IsInstruction(char *p)
{
   unsigned int i,l,bn,oe;

   // Initialize

   am  = AM_None; // address mode
   o16 = 0;       // forced operand 16 bit
   Mne = NULL;    // menmonic
   GenIndex = -1;
   ml = 3;

   // length of mnemonic must be 3 or 4

   if (strlen(p) < 3) return -1;

   // first three characters must be letters

   if (!(isalpha(p[0]) && isalpha(p[1]) && isalpha(p[2]))) return -1;

   // Check 4 character mnemonics

   if (strlen(p) > 3 && p[3] >= '0' && p[3] <= '7' && p[4] <= 0x20)
   for (i=0 ; i < BITS ; ++i)
   {
      if (!Strncasecmp(p,Bit[i].Mne,3) // match mnemonic
      &&  !(Bit[i].CPU & CPU_Type))    // match CPU
      {
         if (i > 1) // BBR & BBS
         {
            am  = AM_Bits;
            bn  = p[3] & 7;
            Mne = Bit[i].Mne;
            if (df) fprintf(df,"BBR/BBS:%s %2.2x\n",Mne,Bit[i].Opc|(bn << 4));
            return Bit[i].Opc | (bn << 4);
         }
         else  // RMB & SMB
         {
            am  = AM_Dpag;
            bn  = p[3] & 7;
            Mne = Bit[i].Mne;
            if (df) fprintf(df,"RMB/SMB:%s %2.2x\n",Mne,Bit[i].Opc|(bn << 4));
            return Bit[i].Opc | (bn << 4);
         }
      }
   }

   // Check Q mnemonics

   if (CPU_Type == CPU_45GS02 && strlen(p) > 5)
   for (i=0 ; i < 16 ; ++i)
   {
      l = strlen(MneQ[i]);
      if (!Strncasecmp(p,MneQ[i],l) && isspace(p[l]))
      {
         Mne = MneQ[i];
         if (Qumulator(p+l)) break;
         GenIndex = i;
         return 512+i;
      }
   }

   // Check for long branch instructions

   if (CPU_Type == CPU_45GS02 && (*p == 'L' || *p == 'l'))
   for (i=0 ; i < 9 ; ++i)
   {
      if (!Strncasecmp(p+1,Rel[i].Mne,3)) // match mnemonic
      {
         am  = AM_Relo;
         Mne = Rel[i].Mne;
         if (df) fprintf(df,"Long Rel:L%s %2.2x\n",Mne,Rel[i].Opc+3);
         return Rel[i].Opc+3;
      }
   }

   // Check for long branch BSR

   if (CPU_Type == CPU_45GS02 && !Strncasecmp(p,"BSR",3))
   {
      am  = AM_Relo;
      Mne = Rel[10].Mne;
      return 0x63;
   }

   // chacter after mnemonic must be zero or white space

   if (CPU_Type == CPU_45GS02 && strlen(p) > 3 &&
       (p[3] == 'Q' || p[3] == 'q'))
   {
      if (p[4] != 0 && !isspace(p[4])) return -1;
      oe = OperandExists(p+4);
      ml = 4;
   }
   else
   {
      if (p[3] != 0 && !isspace(p[3])) return -1;
      oe = OperandExists(p+3);
   }

   // Check table of implied instructions

   if (!oe)
   for (i=0 ; i < IMPS ; ++i)
   {
      if (!Strncasecmp(p,Imp[i].Mne,3) // match mnemonic
      &&  !(Imp[i].CPU & CPU_Type))    // match CPU
      {
         am  = AM_Impl;
         Mne = Imp[i].Mne;
         if (df) fprintf(df,"Imp:%s %2.2x\n",Mne,Imp[i].Opc);
         if (p[3] == 'Q' || p[3] == 'q') return 512+Imp[i].Opc;
         return Imp[i].Opc;
      }
   }

   // Check for short branch instructions

   for (i=0 ; i < RELS ; ++i)
   {
      if (!Strncasecmp(p,Rel[i].Mne,3) // match mnemonic
      &&  !(Rel[i].CPU & CPU_Type))    // match CPU
      {
         am  = AM_Rela;
         Mne = Rel[i].Mne;
         if (df) fprintf(df,"Rel:%s %2.2x\n",Mne,Rel[i].Opc);
         return Rel[i].Opc;
      }
   }

   // Check for all other mnemonics

   for (i=0 ; i < GENS ; ++i)
   {
      if (!Strncasecmp(p,Gen[i].Mne,3) // match mnemonic
      &&  !(Gen[i].CPU & CPU_Type))    // match CPU
      {
         Mne = Gen[i].Mne;
         if (df) fprintf(df,"Gen:%s %2.2x\n",Mne,i);
         GenIndex = i;
         return 256+i;
      }
   }

   // No mnemonic

   return -1;
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
   PrintPCLine();
   p = EvalOperand(p+1,&v,0);
   if (df) fprintf(df,"PC = %4.4x\n",v);
   pc = v;
   if (LoadAddress == UNDEF) LoadAddress = pc;
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
   if (Pass == MaxPass)
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
      if (!StrnCmp(p,Mac[i].Name,l) && !isym(p+l)) return i;
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
   if (df) fprintf(df,"DEFINE LABEL\n");
   p = GetSymbol(p,Label);

   // in BSO mode use scope

   if (BSO_Mode && isalpha(Label[0]) && !strncmp(Label,Line,strlen(Label)))
   {
      strcpy(Scope,Label);
      ModuleStart = pc;
   }

   if (df) fprintf(df,"DefineLabel:%s\n",Label);
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
         if (Pass < MaxPass) lab[j].Address = v;
         else
         {
            ++ErrNum;
            ErrorLine(p);
            ErrorMsg("*Multiple assignments for label [%s]\n"
                     "1st. value = $%4.4x   2nd. value = $%4.4x\n",
                     Label, lab[j].Address, v);
            exit(1);
         }
      }
      *val = v;
      if (Locked) lab[j].Locked = Locked;
      if (df)
      {
         if (lab[j].Address == UNDEF)
            fprintf(df,"P%d:%s = UNDEFINED\n",Pass,lab[j].Name);
         else
            fprintf(df,"P%d:%s = $%4.4x\n",Pass,lab[j].Name,lab[j].Address);
      }
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
         if (Pass == 1)
         {
            ++ErrNum;
            ErrorMsg("Multiple label definition [%s]"
                     " value 1: %4.4x   value 2: %4.4x\n",
                     Label, lab[j].Address,pc);
            exit(1);
         }
         else if (Pass < MaxPass)
         {
            if (df) fprintf(df,"Change %d:%4.4x -> %4.4x %s\n",
               Pass,lab[j].Address,pc,Label);
            lab[j].Address = pc;
            ++BOC[Pass-1]; // branch optimisation count
         }
         else
         {
            ErrorMsg("Phase error label [%s]"
                     " pass %d: %4.4x   pass %d: %4.4x\n",
                     Label,Pass-1,lab[j].Address,Pass,pc);
            exit(1);
         }
      }
      if (!lab[j].Locked) *val = pc;
      lab[j].Ref[0] = LiNo;
      lab[j].Att[0] = LPOS;
   }
   if (df) fprintf(df,"P%d: {%s}=$%4.4x\n",Pass,lab[j].Name,lab[j].Address);
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
   if (df) fprintf(df,"AddLabel:%s\n",p);
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

   if (Pass != MaxPass) return;
   n = ++lab[i].NumRef;
   lab[i].Ref = ReallocOrDie(lab[i].Ref,(n+1)*sizeof(int));
   lab[i].Ref[n] = LiNo;
   lab[i].Att = ReallocOrDie(lab[i].Att,(n+1)*sizeof(int));
   lab[i].Att[n] = am;
}


// ************
// EvalSymValue
// ************

char *EvalSymValue(char *p, int *v)
{
   int i;
   char Sym[ML];

   if (df) fprintf(df,"EVALSYM\n");
   p = GetSymbol(p,Sym);
   for (i=0 ; i < Labels ; ++i)
   {
      if (!StrCmp(Sym,lab[i].Name))
      {
         *v = lab[i].Address;
         SymRefs(i);
         if (Pass == MaxPass && *v == UNDEF)
         {
            ErrorLine(p);
            ErrorMsg("%s = UNDEFINED\n",lab[i].Name);
            exit(1);
         }
         return p;
      }
   }
   AddLabel(Sym);
   *v = UNDEF;
   return p;
}

// ************
// EvalSymBytes
// ************

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
   if (Pass == MaxPass)
   {
      for (i=0 ; i < l ; ++i) ROM[pc+i] = Operand[i];
      PrintPC();
      fprintf(lf," %2.2x %2.2x %2.2x %s\n",
         Operand[0],Operand[1],Operand[2],Line);
   }
   pc += l;
   return p;
}

double BasicReal(unsigned char B[])
{
   double Mantissa;
   double Result;
   int  Sign;
   int  Exponent;

   Exponent =   B[0] - 128;
   Sign     =   B[1] & 0x80;
   Mantissa = ((unsigned long)(B[1] | 0x80) << 24)
            +  (B[2]         << 16)
            +  (B[3]         <<  8)
            +  (B[4]              );
   Result   = ldexp(Mantissa,Exponent-32);
   if (Sign) Result = -Result;
   return Result;
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
   else if (*p == '@') // .real @204,@346,@032,@055,@033
   {
      for (i=0 ; i <= mansize ; ++i, p+=5)
      {
          sscanf(p+1,"%3o",&v);
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

   if (Pass == MaxPass)
   {
      for (i=0 ; i < mansize+1 ; ++i) ROM[pc+i] = Operand[i];
      PrintPC();
      fprintf(lf," %2.2x %2.2x %2.2x",Operand[0],Operand[1],Operand[2]);
      if (mansize == 3 && strncmp(Line,"   ",3)==0)
      {
         fprintf(lf," %2.2x %s",Operand[3],Line+3);
      }
      else if (mansize == 4 && strncmp(Line,"      ",6)==0)
      {
         fprintf(lf," %2.2x %2.2x %s",Operand[3],Operand[4],Line+6);
      }
      else fprintf(lf," %s",Line);
      fprintf(lf," %20.10lf\n",BasicReal(Operand));
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
   if (*p == '\\')
   {
      ++p;
           if (*p == 'r') *v = 13;
      else if (*p == 'n') *v = 10;
      else if (*p == 'a') *v =  7;
      else if (*p == 'e') *v = 27;
      else if (*p == '0') *v =  0;
      else                *v = *p;
      ++p;
   }
   else *v = *p++;
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


char *EvalOctValue(char *p, int *v)
{
   unsigned int w;
   sscanf(p,"%o",&w);
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
char *op_oct(char *p, int *v) { return EvalOctValue(p+1,v) ;}
char *op_cha(char *p, int *v) { return EvalCharValue(p+1,v);}
char *op_bin(char *p, int *v) { return EvalBinValue(p+1,v) ;}
char *op_len(char *p, int *v) { return EvalSymBytes(p+1,v) ;}

struct unaop_struct
{
   char op;
   char *(*foo)(char*,int*);
};

#define UNAOPS 14

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
   {'?',&op_len}, // length of .BYTE data line
   {'@',&op_oct}  // octal constant
};

char unastring[UNAOPS+1] = "[(+-!~<>*$'%?";

int unaops = UNAOPS-1;

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

   if (*p && strchr(unastring,*p))
   {
      for (i=0 ; i < unaops ; ++i)
      if (*p == unaop[i].op)
      {
          p = unaop[i].foo(p,v);
          break;
      }
   }
   else if (isdigit(c) && !isnnd(p)) p = EvalDecValue(p,v);
   else if (isym(p)    ||  isnnd(p)) p = EvalSymValue(p,v);
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

   while (*p && strchr("=*/+-<>!&^|",*p))
   {
      // loop through all binary operators

      for (i=0 ; i < BINOPS ; ++i)
      {
         l = strlen((binop[i].op));
         if (!strncmp(p,binop[i].op,l))
         {
            if ((o = binop[i].prio) <= prio) return p;
            p = EvalOperand(p+l,&w,o);
            if (df)
            {
               fprintf(df,"BinOp %d %s %d\n",*v,binop[i].op,w);
            }
            if (*v == UNDEF || w == UNDEF) *v = UNDEF;
            else *v = binop[i].foo(*v,w);
            break;
         }
      }
   }
   return p;
}

// *************
// ParseWordData
// *************

char *ParseWordData(char *p, int bigendian)
{
   int i,j,l,v;
   unsigned char ByteBuffer[ML];

   l = 0;
   while (*p && *p != ';') // Parse data line
   {
      p = SkipSpace(p);
      p = EvalOperand(p,&v,0);
      if (v == UNDEF && Pass == MaxPass)
      {
         ErrorMsg("Undefined symbol in WORD data\n");
         ErrorLine(p);
         exit(1);
      }
      if (bigendian)
      {
         ByteBuffer[l++] = v >> 8;
         ByteBuffer[l++] = v & 0xff;
      }
      else
      {
         ByteBuffer[l++] = v & 0xff;
         ByteBuffer[l++] = v >> 8;
      }
      p = SkipToComma(p);

      if (*p == ',') ++p;
   }
   if (l < 1)
   {
      ErrorMsg("Missing WORD data\n");
      ErrorLine(p);
      exit(1);
   }

   j = AddressIndex(pc);
   if (j >= 0)
   for ( ; j < Labels ; ++j) // There may be multiple lables on this address
   {
       if (lab[j].Address == pc) lab[j].Bytes = l;
   }

   if (Pass == MaxPass)
   {
      PrintPC();
      for (i=0 ; i < l ; ++i)
      {
         ROM[pc+i] = ByteBuffer[i];
         if (i < 2) fprintf(lf," %2.2x",ByteBuffer[i]);
      }
      fprintf(lf,"    %s\n",Line);
   }
   pc += l;
   return p;
}



// *************
// ParseHex4Data
// *************

char *ParseHex4Data(char *p)
{
   int v;
   char hbuf[10];

   p = EvalOperand(p,&v,0);
   if (Pass == MaxPass)
   {
      sprintf(hbuf,"%4.4X",v & 0xffff);
      ROM[pc  ] = hbuf[0];
      ROM[pc+1] = hbuf[1];
      ROM[pc+2] = hbuf[2];
      ROM[pc+3] = hbuf[3];
      PrintPC();
      fprintf(lf," %2.2x %2.2x %2.2x  %s\n",
              hbuf[0],hbuf[1],hbuf[2],Line);
   }
   pc += 4;
   return p;
}

// *************
// ParseDec4Data
// *************

char *ParseDec4Data(char *p)
{
   int v;
   char hbuf[10];

   if (df) fprintf(df,"Dec4:%s\n",p);
   p = EvalOperand(p,&v,0);
   if (df) fprintf(df,"Dec4:%d\n",v);
   if (Pass == MaxPass)
   {
      sprintf(hbuf,"%4d",v);
      ROM[pc  ] = hbuf[0];
      ROM[pc+1] = hbuf[1];
      ROM[pc+2] = hbuf[2];
      ROM[pc+3] = hbuf[3];
      PrintPC();
      fprintf(lf," %2.2x %2.2x %2.2x  %s\n",
              hbuf[0],hbuf[1],hbuf[2],Line);
   }
   pc += 4;
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
   if (Pass == MaxPass)
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

char *ListSizeInfo(char *p)
{
   int i;

   if (Pass == MaxPass)
   {
      PrintPC();
      fprintf(lf,"          %s",Line);
      i = AddressIndex(ModuleStart);
      if (i >= 0)
          fprintf(lf," [%s] Size = %d [$%x]",lab[i].Name,
                  pc-ModuleStart,pc-ModuleStart);
      fprintf(lf,"\n");
   }
   return p + strlen(p);
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
   int i;
   p = SkipSpace(p);
   for (i=0, CPU_Type = 1 ; i < CPU_NAMES ; ++i,CPU_Type <<=1)
   {
      if (!Strncasecmp(p,CPU_Names[i],strlen(CPU_Names[i]))) break;
   }
   if (i == CPU_NAMES)
   {
      ErrorMsg("Unsupported CPU type <%s>\n",p);
      exit(1);
   }
   CPU_Name = CPU_Names[i];
   if (df) fprintf(df,"new CPU: %s [%d]\n",CPU_Name,CPU_Type);
   PrintLine();
   return p;
}

char *ParseStoreData(char *p)
{
   int Start,Length,i;
   char Filename[80];

   if (Pass < MaxPass) return p;
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
   if (Pass == MaxPass)
   {
      PrintLiNo(1);
      fprintf(lf,"%4.4x             ",bss);
      fprintf(lf,"%s\n",Line);
   }
   bss += m;
   return p;
}


char *ParseBaseData(char *p)
{
   p = EvalOperand(p,&bp,0);
   if (bp < 0 || bp > 255)
   {
      ErrorMsg("Illegal base page value %d\n",bp);
      exit(1);
   }
   if (Pass == MaxPass)
   {
      PrintLiNo(1);
      fprintf(lf,"%s\n",Line);
   }
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
   if (Pass == MaxPass)
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
   if (Pass == MaxPass)
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
   while (*p && (*p != Delimiter || *(p+1) == *p) && *l < ML-1)
   {
      if (*p == '\\') // special character CR, LF, NULL
      {
         ++p;
              if (*p == 'r') b[*l] = 13;
         else if (*p == 'n') b[*l] = 10;
         else if (*p == 'a') b[*l] =  7;
         else if (*p == 'e') b[*l] = 27;
         else if (*p == '0') b[*l] =  0;
         else b[*l] = *p;
         ++(*l);
         ++p;
      }
      else if (*p == 0x27 && *(p+1) == 0x27) // ''
      {
         b[(*l)++] = *p++;
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
   if (!strncmp(p,"+$80",4))
   {
      b[(*l)-1] |= 0x80; // Set bit 7 of last char
      p += 4;
   }
   return p;
}

char *ParseByteData(char *p, int Charset)
{
   int i,j,l,v;
   char Delimiter;
   unsigned char ByteBuffer[ML];

   l = 0;
   while (*p && *p != ';') // Parse data line
   {
      p = SkipSpace(p);
      Delimiter = *p;
      if (Delimiter == '<' && p[1] == '"' && p[5] == '"') // Packed ASCII
      {
//       if (CPU_Type == CPU_45GS02)
//       ByteBuffer[0] = ((p[3] - 64) << 5) | ((p[4] - 64)     );
//       else
         ByteBuffer[0] = ((p[3] - 63) << 6) | ((p[4] - 63) << 1);
         l  = 1;
         p += 6;
      }
      else if (Delimiter == '>' && p[1] == '"' && p[5] == '"') // Packed ASCII
      {
//       if (CPU_Type == CPU_45GS02)
//       ByteBuffer[0] = ((p[2] - 64) << 2) | ((p[3] - 64) >> 3);
//       else
         ByteBuffer[0] = ((p[2] - 63) << 3) | ((p[3] - 63) >> 2);
         l  = 1;
         p += 6;
      }
      else if (Delimiter == '#' && p[1] == '"' && p[5] == '"') // Hashed ASCII
      {
         v = p[4]-64 + 27 * (p[3]-64 + 27 * (p[2]-64));
         ByteBuffer[0] = v & 0xff;
         ByteBuffer[1] = v >> 8;
         l  = 2;
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
         if (v == UNDEF && Pass == MaxPass)
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
   if (Pass == MaxPass)
   {
      PrintPC();
      for (i=0 ; i < l ; ++i)
      {
         ROM[pc+i] = ByteBuffer[i];
         if (i < 3) fprintf(lf," %2.2x",ByteBuffer[i]);
      }
      for (i=l ; i < 3 ; ++i) fprintf(lf,"   ");
      if (l == 4 && strncmp(Line,"   ",3)==0)
      {
         fprintf(lf," %2.2x %s",ByteBuffer[3],Line+3);
      }
      else if (l == 5 && strncmp(Line,"      ",6)==0)
      {
         fprintf(lf," %2.2x %2.2x %s",ByteBuffer[3],ByteBuffer[4],Line+6);
      }
      else fprintf(lf," %s",Line);
      fprintf(lf,"\n");
   }
   pc += l;
   return p;
}

// ********
// IsPseudo
// ********

char *IsPseudo(char *p)
{
        if (!Strncasecmp(p,"WORD",4))    p = ParseWordData(p+4,0);
   else if (!Strncasecmp(p,"BIGW",4))    p = ParseWordData(p+4,1);
   else if (!Strncasecmp(p,"HEX4",4))    p = ParseHex4Data(p+4);
   else if (!Strncasecmp(p,"DEC4",4))    p = ParseDec4Data(p+4);
   else if (!Strncasecmp(p,"WOR",3))     p = ParseWordData(p+3,0);
   else if (!Strncasecmp(p,"BYTE",4))    p = ParseByteData(p+4,ASCII);
   else if (!Strncasecmp(p,"BYT",3))     p = ParseByteData(p+3,ASCII);
   else if (!Strncasecmp(p,"PET",3))     p = ParseByteData(p+3,PETSCII);
   else if (!Strncasecmp(p,"DISP",4))    p = ParseByteData(p+4,SCREENCODE);
   else if (!Strncasecmp(p,"BITS",4))    p = ParseBitData(p+4);
   else if (!Strncasecmp(p,"LITS",4))    p = ParseLitData(p+4);
   else if (!Strncasecmp(p,"QUAD",4))    p = ParseLongData(p+4,4);
   else if (!Strncasecmp(p,"REAL",4))    p = ParseRealData(p+4);
   else if (!Strncasecmp(p,"FILL",4))    p = ParseFillData(p+4);
   else if (!Strncasecmp(p,"BSS",3))     p = ParseBSSData(p+4);
   else if (!Strncasecmp(p,"STORE",5))   p = ParseStoreData(p+5);
   else if (!Strncasecmp(p,"CPU",3))     p = ParseCPUData(p+3);
   else if (!Strncasecmp(p,"BASE",4))    p = ParseBaseData(p+4);
   else if (!Strncasecmp(p,"CASE",4))    p = ParseCaseData(p+4);
   else if (!Strncasecmp(p,"ORG",3))     p = SetPC(p);
   else if (!Strncasecmp(p,"LOAD",4))    WriteLA = 1;
   else if (!Strncasecmp(p,"INCLUDE",7)) p = IncludeFile(p+7);
   else if (!Strncasecmp(p,"!SRC",4))    p = IncludeFile(p+4);
   else if (!Strncasecmp(p,"SIZE",4))    p = ListSizeInfo(p);
   else if (!Strncasecmp(p,"SKI",3))     p += strlen(p);
   else if (!Strncasecmp(p,"PAG",3))     p += strlen(p);
   else if (!Strncasecmp(p,"NAM",3))     p += strlen(p);
   else if (!Strncasecmp(p,"SUBTTL",6))  p += strlen(p);
   else if (!Strncasecmp(p,"END",3))     p += strlen(p);
   else if (!Strncasecmp(p,"!ADDR ",6))  p += 6;
   if (pc > 0x10000 && pc != UNDEF)
   {
      ErrorMsg("Program counter overflow\n");
      ErrorLine(p);
      exit(1);
   }
   return p;
}

// ***********
// AddressMode
// ***********

int AddressMode(unsigned char *p)
{
   size_t l = strlen((const char *)p);
   char s = 0;
   char i = 0;
   char m = 0;
   char o = 0;
   int oi = 0;
   int mi = 0;
   int ii = 0;

   // remove ",Z" for Q instructions

   if (l > 2 && oc > 511 && p[l-2] == ',' && (p[l-1] == 'z' || p[l-1] == 'Z'))
   {
      l -= 2;
      p[l] = 0;
   }

   // remove redundant pair of brackets
   // or identify Q operation with 32 bit address

   if (l > 1 && p[0] == '[' && p[l-1] == ']')
   {
      p[0]   = ' '; // remove [
      p[l-1] = 0;   // remove ]
      p      = (unsigned char*)SkipSpace((char *)p);
      l      = strlen((const char *)p);
      if (oc > 511)
      {
         il = 5;
         return AM_Indz; // Q instruction
      }
   }

   // prefix character

   s = p[0];

   // immediate

   if (s == '#')
   {
      p[0] = ' ';
      il = 2;
      return AM_Imme;
   }

   if (s == '`')
   {
      p[0] = ' ';
      o16 = 1;
      il = 3;
   }

   if (s != '(' && s != '[') s = 0;

   // outer character

   if (l > 0) o = toupper(p[l-1]);


   if (o != ')' && o != 'X' && o != 'Y' && o != 'Z') o = 0;
   else oi = l-1;

   if (o)
   {
      --l;
      while (l > 0 && isspace(p[l-l])) --l;

      // middle character

      if (l > 0 && p[l-1] == ' ') --l; // e.g. addr, x
      if (l > 0) m = toupper(p[l-1]);
      if (m != ',' && m != 'X') m = 0;
      else mi = l-1;

      if (m)
      {
         --l;
         while (l > 0 && isspace(p[l-l])) --l;

         // inner character

         if (l > 0) i = toupper(p[l-1]);
         if (i != ',' && i != ')' && i != ']') i = 0;
         else ii = l-1;
      }
   }

   // [DP],Z : 32 bit indirect address

   if (s == '[' && i == ']' && m == ',' && o == 'Z')
   {
      il = 3;
      p[0] = p[ii] = p[mi] = p[oi] = ' ';
      return AM_Indz;
   }

   // (DP),Z : 16 bit indirect address

   if (s == '(' && i == ')' && m == ',' && o == 'Z')
   {
      il = 2;
      p[0] = p[ii] = p[mi] = p[oi] = ' ';
      return AM_Indz;
   }

   // (DP),Y : 16 bit indirect address

   if (s == '(' && i == ')' && m == ',' && o == 'Y')
   {
      il = 2;
      p[0] = p[ii] = p[mi] = p[oi] = ' ';
      return AM_Indy;
   }

   // (DP,X) : 16 bit indirect address

   if (s == '(' && i == ',' && m == 'X' && o == ')')
   {
      il = 2;
      p[0] = p[ii] = p[mi] = p[oi] = ' ';
      return AM_Indx;
   }

   // (ADR) : same mode as (DP),Z

   if (s == '(' && o == ')')
   {
      il = 3;
      p[0] = p[oi] = ' ';
      return AM_Indz;
   }

   // ADR,Y : indexed address

   if (m == ',' && o == 'Y')
   {
      il = 3;
      p[mi] = p[oi] = ' ';
      return AM_Absy;
   }

   // ADR,X : indexed address

   if (m == ',' && o == 'X')
   {
      il = 3;
      p[mi] = p[oi] = ' ';
      return AM_Absx;
   }

   return AM_Abso;
}

// ************
// SplitOperand
// ************

char * SplitOperand(char *p)
{
   int l,inquo,inapo,to;

   l       =    0; // length of trimmed operand
   il      =    3; // default instruction length
   inquo   =    0; // inside quotes
   inapo   =    0; // inside apostrophes
   if (am != AM_Dpag) am = AM_Abso; // default address mode

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

   // valid address mode for Q instructions are:

   // AM_Impl  Q
   // AM_Dpag  base page
   // AM_Abso  absolute
   // AM_      indirect
   // AM_Indz  indirect 32 bit

   if (oc > 511) // Q mnemonics
   {
      am = AddressMode(Operand);
      if (am == AM_Abso)
      {
         oc = Gen[GenIndex].Opc[am];
         il = 5;
      }
      else if (am == AM_Impl)
      {
         oc = Gen[GenIndex].Opc[am];
         il = 3;
      }
      else if (am == AM_Indz)
      {
         oc = Gen[GenIndex].Opc[am];
         if (il == 3) il = 4;
      }
      else
      {
         ++ErrNum;
         ErrorLine(p);
         ErrorMsg("illegal address mode\n");
         exit(1);
      }
   }

   else if (oc > 255)
   {
      am = AddressMode(Operand);
      if (df) fprintf(df,"AMOC: %s %d\n",Gen[GenIndex].Mne,am);
      if (am < 9)
      {
         to = Gen[GenIndex].Opc[am];
         if (to == -1 && am == AM_Absx)
         {
            am = AM_Dpgx;
            il = 2;
            oc = Gen[GenIndex].Opc[AM_Dpgx];
         }
         else if (to == -1 && am == AM_Abso)
         {
            am = AM_Dpag;
            il = 2;
            oc = Gen[GenIndex].Opc[AM_Dpag];
         }
         else oc = to;
      }
   }

   if (oc < 0 || oc > 255)
   {
      ++ErrNum;
      ErrorLine(p);
      ErrorMsg("syntax error\n");
      exit(1);
   }

   // Allow BIT mnemonic with missing operand
   // used to skip next 2 byte statement

   if (l == 0 && oc == 0x24)
   {
      oc = 0x2c;
      am = AM_Impl;
      il = 1;
      return p;
   }
   return p;
}

// *********
// CPU_Error
// *********

void CPU_Error(void)
{
   ++ErrNum;
   ErrorLine(Line);
   ErrorMsg("Illegal instruction or operand for CPU %s\n",CPU_Name);
   exit(1);
}

// ************
// AdjustOpcode
// ************

void AdjustOpcode(int *v)
{
   NegNeg = 0;
   PreNop = 0;

   // JMP

   if (GenIndex == JMPIndex)
   {
      if (am == AM_Indx && CPU_Type < CPU_65SC02) CPU_Error();
      il = 3; // JMP (addr,X) uses 16 bit address
      return;
   }

   // JSR

   if (GenIndex == JSRIndex)
   {
      if (am == AM_Indx && CPU_Type < CPU_45GS02) CPU_Error();
      il = 3; // JSR (addr,X) uses 16 bit address
      if (CPU_Type == CPU_65816) oc = 0xfc;
      return;
   }

   // BIT

   if (GenIndex == BITIndex)
   {
      if (am > AM_Abso && CPU_Type < CPU_65SC02) CPU_Error();
      return;
   }

   // STY

   if (GenIndex == STYIndex)
   {
      if (am == AM_Absx && CPU_Type != CPU_45GS02) CPU_Error();
      return;
   }

   // PHW

   if (GenIndex == PHWIndex)
   {
      if (am == AM_Imme) il = 3;
      return;
   }

   // MEGA65 32 bit indirect address mode

   if (am == AM_Indz && il == 3)
   {
      *v = (*v << 8) | oc;
      oc = 0xea;   // NOP   (DP),Z
      return;
   }

   // MEGA65 qumulator

   if (am == AM_Impl && il == 3)
   {
      NegNeg = 1;
      return;
   }

   // MEGA65 direct page quad

   if (am == AM_Dpag && il == 4)
   {
      NegNeg = 1;
      return;
   }

   // MEGA65 absolute quad

   if (am == AM_Abso && il == 5)
   {
      NegNeg = 1;
      return;
   }

   // MEGA65 16 bit indirect quad

   if (am == AM_Indz && il == 4)
   {
      NegNeg = 1;
      return;
   }

   // MEGA65 32 bit indirect quad

   if (am == AM_Indz && il == 5)
   {
      NegNeg = 1;
      PreNop = 1;
      return;
   }
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
   if (!Strncasecmp(p,"error", 5) && (Pass == 1))
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
         if (df) fprintf(df,"#if (%d)\n",v);
      }
      CheckSkip();
      if (Pass == MaxPass)
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
      if (Pass == MaxPass) fprintf(lf,"              %s\n",Line);
   }
   if (!Strncasecmp(p,"endif",5) && (p[5] == 0 || isspace(p[5])))
   {
      r = 1;
      IfLevel--;
      PrintLiNo(1);
      if (Pass == MaxPass) fprintf(lf,"              %s\n",Line);
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




// ************
// GenerateCode
// ************

char *GenerateCode(char *p)
{
   int v,w,lo,hi,pl;
   char *o;
   char *li;

   // initialize

   pl = pc;
   li = Line;
   v = 0; // operand value
   w = 0; // operand value - base value
   o = (char *)Operand;

   if (df) fprintf(df,"GenerateCode %4.4X %d %s\n",pc,am,p);
   if (pc < 0)
   {
      ErrorLine(p);
      ErrorMsg("Undefined program counter (PC)\n");
      exit(1);
   }

   // implied instruction (no operand or implied A register)

   if (df) fprintf(df,"Implied: oc = %d\n",oc);
   if (am == AM_Impl)
   {
      if (oc > 511)
      {
         il = 3;
         p += ml;
         oc -= 512;
      }
      else
      {
         il = 1;              // instruction length
         p += 3;              // skip mnemonic
      }
   }

   // test & branch  BBRn dp,label

   else if (am == AM_Bits)
   {
      il = 3;  // opcode + dp address + branch

      // get direct page address to test

      o = EvalOperand(p+4,&lo,0);
      lo -= (bp << 8);
      if (Pass == MaxPass && (lo < 0 || lo > 255))
      {
         ErrorLine(p+4);
         ErrorMsg("Need direct page address, read (%d)\n",lo);
         exit(1);
      }

      // check if comma separator is present

      o = SkipSpace(o);
      if (*o != ',' )
      {
         ErrorLine(o);
         ErrorMsg("Need two arguments\n");
         exit(1);
      }

      // get branch target

      o = EvalOperand(o+1,&hi,0);
      if (hi != UNDEF)  hi -= (pc + 3);
      if (Pass == MaxPass && hi == UNDEF)
      {
         ErrorLine(p);
         ErrorMsg("Branch to undefined label\n");
         exit(1);
      }
      if (Pass == MaxPass && (hi < -128 || hi > 127))
      {
         ErrorLine(p);
         ErrorMsg("Branch too long (%d)\n",hi);
         exit(1);
      }
      if (lo != UNDEF && hi != UNDEF) v = lo | (hi << 8);
      else v = UNDEF;
      hi &= 0xff;
   }

   // automatic branch optimisation

   else if (am == AM_Rela && BranchOpt && CPU_Type == CPU_45GS02)
   {
      il = 2; // default = short branch
      o = EvalOperand(p+3,&v,0);
      if (v != UNDEF) v  -= (pc + 2);

      if (v == UNDEF) // assume long branch
      {
         il  = 3;
         oc |= 3;
      }
      else if (v < -128 || v > 127)
      {
         il  = 3;
         oc |= 3;
         v &= 0xffff;
      }

      // lock branch opcode on pass before final

      if (Pass == MaxPass-1)
      {
         ROM[pc] = oc;
      }

      // use locked opcode on final pass

      if (Pass == MaxPass)
      {
         if (v == UNDEF)
         {
            ErrorLine(p);
            ErrorMsg("Branch to undefined label\n");
            exit(1);
         }
         oc = ROM[pc];
         il = 2;
         if ((oc & 3) == 3)
         {
            il = 3;
            v &= 0xffff;
         }
      }
   }

   // short branches

   else if (am == AM_Rela)
   {
      il = 2;
      o = EvalOperand(p+3,&v,0);
      if (v != UNDEF) v  -= (pc + 2);
      if (Pass == MaxPass && v == UNDEF)
      {
         ErrorLine(p);
         ErrorMsg("Branch to undefined label\n");
         exit(1);
      }
      if (Pass == MaxPass && (v < -128 || v > 127))
      {
         ErrorLine(p);
         ErrorMsg("Branch too long (%d)\n",v);
         exit(1);
      }
   }

   // long branches

   else if (am == AM_Relo)
   {
      il = 3;
      o = EvalOperand(p+4,&v,0);
      if (v != UNDEF) v = (v - pc - 2) & 0xffff;
      if (Pass == MaxPass && v == UNDEF)
      {
         ErrorLine(p);
         ErrorMsg("Branch to undefined label\n");
         exit(1);
      }
   }

   // anything else

   else
   {
      p = SkipSpace(p+4) ; // Skip mnemonic & space
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
      w = v - (bp << 8); // for base page != zero
      if (GenIndex != PHWIndex && am == AM_Imme && Pass == MaxPass &&
          (v < -128 || v > 255))
      {
            ErrorLine(p);
            ErrorMsg("Immediate value out of range (%d)\n",v);
            exit(1);
      }
      else if (am == AM_Dpag)
      {
         if (df) fprintf(df,"DPAG:%s\n",Line);
         il = 2;
         v = w;
         if (v < -128 || v > 255)
         {
            ErrorLine(p);
            ErrorMsg("base page value out of range (%d)\n",v);
            exit(1);
         }
      }
      else if (w >= 0 && w < 256 && GenIndex >= 0 && o16 == 0)
      {
         if (am == AM_Abso && Gen[GenIndex].Opc[AM_Dpag] >= 0)
         {
            v = w;
            am = AM_Dpag;
            oc = Gen[GenIndex].Opc[AM_Dpag];
            --il;
         }
         else if (am == AM_Absx && Gen[GenIndex].Opc[AM_Dpgx] >= 0)
         {
            v = w;
            am = AM_Dpgx;
            oc = Gen[GenIndex].Opc[AM_Dpgx];
            il = 2;
         }
         else if (oc == 0xbe) // LDX Abs,Y
         {
            v = w;
            oc = 0xb6;        // LDX DP,Y
            il = 2;
         }
         else if (oc == 0x9b) // STX Abs,Y
         {
            v = w;
            oc = 0x96;        // STX DP,Y
            il = 2;
         }
      }
   }
   else if (am != AM_Impl && am != AM_Rela && am != AM_Relo && am != AM_Bits)
   {
      ErrorLine(p);
      ErrorMsg("Operand missing\n");
      exit(1);
   }

   // check for LDA (bp,SP),Y or STA (bp,SP),Y

   if (CPU_Type == CPU_45GS02 && am == AM_Indy && !Strncasecmp(o,",SP",3))
   {
      if (GenIndex == LDAIndex)
      {
          oc = 0xe2;
          *o = 0;
      }
      if (GenIndex == STAIndex)
      {
          oc = 0x82;
          *o = 0;
      }
   }

   if (*o && *o != ';')
   {
      ErrorLine(p);
      ErrorMsg("Operand syntax error\n<%s>\n",o);
      exit(1);
   }
   AdjustOpcode(&v);
   if (Pass == MaxPass)
   {
      if (v == UNDEF)
      {
         ErrorLine(p);
         ErrorMsg("Use of an undefined label\n");
         exit(1);
      }

      lo = v & 0xff;
      hi = v >> 8;
      if (hi == bp && il < 3)
      {
         hi = 0;
         v  = lo;
      }

      if (il < 3 && (v < -128 || v > 255))
      {
         ++ErrNum;
         ErrorMsg("Not a byte value : %d\n",v);
      }

      // insert binary code

      if (NegNeg)
      {
         ROM[pl++] = 0x42;
         ROM[pl++] = 0x42;
      }
      if (PreNop)
      {
         ROM[pl++] = 0xea;
      }

      ROM[pl++] = oc;

      PrintPC();
      if (NegNeg) fprintf(lf," 42 42");
      if (PreNop) fprintf(lf," ea");
      PrintOC();
      if (pl < pc+il)
      {
         ROM[pl++] = lo;
         fprintf(lf," %2.2x",lo&0xff);
      }
      else if (il < 3) fprintf(lf,"   ");
      if (pl < pc+il)
      {
         ROM[pl++] = hi;
         fprintf(lf," %2.2x",hi&0xff);
      }
      else if (il < 3) fprintf(lf,"   ");
      if (il > 3 && !strncmp(li,"   ",3)) li += 3;
      if (il > 4 && !strncmp(li,"   ",3)) li += 3;
      fprintf(lf," %s",li);
      if (LengthInfo[0]) fprintf(lf," %s",LengthInfo);
      fprintf(lf,"\n");
      LengthInfo[0] = 0;
   }
   if (pc+il > 0xffff)
   {
      if (Pass == MaxPass)
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
         ErrorMsg("Syntax error in macro '%c'\n",*p);
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
   p = NextSymbol(p,Macro);  // macro name
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
      else     // skip macro body
      {
         while (!feof(sf) && !Strcasestr(Line,"ENDMAC"))
         {
            ++LiNo;
            fgets(Line,sizeof(Line),sf);
         }
      }
      if (Pass == MaxPass) // List macro
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
   if (df) fprintf(df,"Expanding [%s] phase %d\n",Mac[j].Name,Pass);

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

   if (Pass == MaxPass)
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

// ***********
// CommentLine
// ***********

int CommentLine(char *p)
{
   p = SkipSpace(p);
   if (*p == ';' || *p == 0) return 1;
   if (*p == '*')
   {
      p = SkipSpace(p+1);
      if (*p == '=') return 0; // set ORG
      return 1;
   }
   return 0;
}

// ***********
// ParseModule
// ***********

char *ParseModule(char *p)
{
   p = SkipSpace(p);
   DefineLabel(p,&ModuleStart,0);
   strcpy(Scope,Label);
   if (df) fprintf(df,"SCOPE: [%s]\n",Scope);
   if (Pass == MaxPass) fprintf(lf,"              %s\n",Line);
   return p+strlen(p);
}

// ***********
// ParseEndMod
// ***********

char *ParseEndMod(char *p)
{
   if (Pass == MaxPass)
   {
      ListSizeInfo(p);
   }
   Scope[0] = 0;
   ModuleStart = 0;
   return p+strlen(p);
}



// *********
// ParseLine
// *********

void ParseLine(char *cp)
{
   int v,m;
   // char *start = cp;  // Remember start of line

   if (df) fprintf(df,"Pass %d:ParseLine:%s\n",Pass,cp);
   am = -1;
   oc = -1;
   Label[0] = 0;
   Operand[0] = 0;
   cp = SkipHexCode(cp);        // Skip disassembly
   cp = SkipSpace(cp);          // Skip leading blanks
   if (CheckCondition(cp)) return;
   if (Skipping)
   {
      PrintLiNo(1);
      if (Pass == MaxPass) fprintf(lf,"SKIP          %s\n",Line);
      if (df)         fprintf(df,"%5d SKIP          %s\n",LiNo,Line);
      return;
   }
   if (pf && Pass == MaxPass && !InsideMacro)
   {
       if (MacroStopped) MacroStopped = 0;
       else fprintf(pf,"%s\n",Line); // write to preprocessed file
   }
   if (CommentLine(cp))
   {
      if (Pass == MaxPass)
      {
          if (*cp) PrintLine();
          else     PrintLiNo(-1);
      }
      return;
   }
   if (!Strncasecmp(cp,"!ADDR ",6))    cp += 6;
   if (!Strncasecmp(cp,"MODULE",6))    cp = ParseModule(cp+6);
   if (!Strncasecmp(cp,"ENDMOD",6))    cp = ParseEndMod(cp+6);
   if (*cp =='_' || isalpha(*cp) || isnnd(cp)) // Macro, Label or mnemonic
   {
      if (!Strncasecmp(cp,"MACRO ",6))
      {
         RecordMacro(cp+6);
         if (df) fprintf(df,"Macro recorded\n");
         if (df) fprintf(df,"Line:%s\n",Line);
         return;
      }
      if ((oc = IsInstruction(cp)) < 0)
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
            PrintLiNo(1);
            if (Pass == MaxPass)
               fprintf(lf,"%4.4x          %s\n",v&0xffff,Line);
            return;
         }
      }
   }
   if (*cp ==  0 ) return;               // No code
   if (*cp == '*') cp = SetPC(cp);       // Set program counter
   if (*cp == '&') cp = SetBSS(cp);      // Set BSS counter
   if (*cp == '.') cp = IsPseudo(cp+1);  // Pseudo Op with dot
   if (*cp == '!') cp = IsPseudo(cp);    // Pseudo Op with exclamation mark
   if (*cp == ',')
   {
      ++ErrNum;
      ErrorLine(cp);
      ErrorMsg("Syntax Error");
      exit(1);
   }
   if (*cp)        cp = IsPseudo(cp);    // Pseudo Op
   if (ForcedEnd) return;
   if (oc < 0) oc = IsInstruction(cp); // Check for mnemonic after label
   if (oc >= 0) GenerateCode(cp);
}

void Pass1Listing(void)
{
   if (Line[0] == 0 || Line[0] == ';') return;
   fprintf(df,"%s\n",Line);
   fprintf(df,"%5d",LiNo);
   if (pc >= 0) fprintf(df," %4.4x",pc);
   else         fprintf(df,"     ");
   if (Label[0]) fprintf(df," [%s]",Label);
   if (Operand[0]) fprintf(df,"  <%s>",Operand);
   fprintf(df,"\n");
}


int CloseInclude(void)
{
   PrintLiNo(1);
   if (Pass == MaxPass)
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

void Pass1(void)
{
   int l,Eof;

   pc = -1;
   bp =  0;
   ForcedEnd = 0;
   LiNo = 0;
   TotalLiNo = 0;
   strcpy(Scope,"Main");
   rewind(sf);
   fgets(Line,sizeof(Line),sf);
   Eof = feof(sf);
   while (!Eof || IncludeLevel > 0)
   {
      ++LiNo; ++TotalLiNo;
      l = strlen(Line);
      if (l && Line[l-1] == 10) Line[--l] = 0; // Remove linefeed
      if (l && Line[l-1] == 13) Line[--l] = 0; // Remove return
      ParseLine(Line);
      if (df) Pass1Listing();
      if (InsideMacro) NextMacLine(Line);
      else
      {
         fgets(Line,sizeof(Line),sf);
      }
      Eof = feof(sf) || ForcedEnd;;
      if (Eof && IncludeLevel > 0) Eof = CloseInclude();
   }
}


void Pass2(void)
{
    int l,Eof;

   pc    = -1;
   bp    =  0;
   ForcedEnd = 0;
   strcpy(Scope,"Main");
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
      if (df) fprintf(df,"Pass %d:[%s] EOF=%d\n",Pass,Line,Eof);
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
         else if (l == AM_Indx) A = 'x';
         else if (l == AM_Indy) A = 'y';
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
      if (lab[i].Att[j] == AM_Indy && lab[i+1].Address == lab[i].Address+1)
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
   int ic,v,l;

   for (ic=1 ; ic < argc ; ++ic)
   {
      if (!strcmp(argv[ic],"-x")) SkipHex = 1;
      else if (!strcmp(argv[ic],"-b")) BranchOpt = 1;
      else if (!strcmp(argv[ic],"-d")) Debug = 1;
      else if (!strcmp(argv[ic],"-i")) IgnoreCase = 1;
      else if (!strcmp(argv[ic],"-n")) WithLiNo = 1;
      else if (!strcmp(argv[ic],"-p")) Preprocess = 1;
      else if (!strncmp(argv[ic],"-D",2)) DefineLabel(argv[ic]+2,&v,1);
      else if (argv[ic][0] >= '0' || argv[ic][0] == '.')
      {
         if (!Src[0]) strncpy(Src,argv[ic],sizeof(Src)-1);
      }
      else
      {
         printf("\nUsage: bsa [-d -D -i -x] <source> <list>\n");
         exit(1);
      }
   }
   if (!Src[0])
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

   // split filename

   strcpy(Ext,".asm");
   l = strlen(Src);
   if (l > 4 && Src[l-4] == '.')
   {
      strcpy(Ext,Src+l-4);
      Src[l-4] = 0;
   }

   if (!strcmp(Ext,".src")) // assume source code for MEGA65
   {
      BSO_Mode   = 1; // VAX BSO assembler compatibility mode
      CPU_Type   = CPU_45GS02;
      CPU_Name   = CPU_Names[3];
      BranchOpt  = 1;
      IgnoreCase = 1;
      ROM_Fill   = 0xff;
      unaops++;      // allow octal constants
      strcpy(unastring,"[(+-!~<>*$'%?@");
   }

   strcpy(Pre,Src);
   strcpy(Lst,Src);
   strcat(Pre,".pp");
   strcat(Src,Ext);
   strcat(Lst,".lst");

   printf("\n");
   printf("*******************************************\n");
   printf("* Bit Shifter's Assembler 10-Jan-2023     *\n");
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

   JMPIndex = GetIndex("JMP");
   JSRIndex = GetIndex("JSR");
   BITIndex = GetIndex("BIT");
   STYIndex = GetIndex("STY");
   PHWIndex = GetIndex("PHW");
   LDAIndex = GetIndex("LDA");
   STAIndex = GetIndex("STA");

   memset(ROM,ROM_Fill,sizeof(ROM));

   for (Pass = 1 ; Pass < MaxPass ; ++Pass)
   {
      Pass1();
   }
   Pass2();
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
   for (l = 0 ; l < MaxPass ; ++l)
   {
      if (BOC[l])
      printf("* Pass     %3d: %6d label changes      *\n",l+1,BOC[l]);
   }
   printf("*******************************************\n");
   if (ErrNum)
      printf("* %3d error%s occured%s                      *\n",
             ErrNum, ErrNum == 1 ? "" : "s", ErrNum == 1 ? " " : "");
   else printf("* OK, no errors                           *\n");
   printf("*******************************************\n");
   printf("\n");
   return 0;
}
