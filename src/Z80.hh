// $Id$

/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                                 Z80.h                                ***/
/***                                                                      ***/
/*** This file contains the function prototypes and variable declarations ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

#ifndef _Z80_H
#define _Z80_H

#include "config.h"

/****************************************************************************/
/*** Machine dependent definitions                                        ***/
/****************************************************************************/
/* #define DEBUG      */              /* Compile debugging version          */
/* #define X86_ASM    */              /* Compile optimised GCC/x86 version  */
/* #define LSB_FIRST  */              /* Compile for low-endian CPU         */
/* #define __64BIT__  */              /* Compile for 64 bit machines        */
/* #define __128BIT__ */              /* Compile for 128 bit machines       */

#ifndef WORDS_BIGENDIAN
	#define LSB_FIRST
#endif
#if (SIZEOF_LONG==8)
	#define __64BIT__
	#warning 64 bit mode is not tested yet
#endif
#if (SIZEOF_LONG==16)
	#define __128BIT__
	#warning 128 bit mode is not tested yet
#endif

/****************************************************************************/
/* sizeof(byte)=1, sizeof(word)=2, sizeof(dword)>=4                         */
/****************************************************************************/
typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned       dword;
typedef signed char    offset;
typedef unsigned long int UINT64;   // 64 bit 

/****************************************************************************/
/* Define a Z80 word. Upper bytes are always zero                           */
/****************************************************************************/
typedef union
{
#ifdef __128BIT__
 #ifdef LSB_FIRST
//   struct { byte l,h,h2,h3,h4,h5,h6,h7,
//                 h8,h9,h10,h11,h12,h13,h14,h15; } B;
//   struct { word l,h,h2,h3,h4,h5,h6,h7; } W;
//   dword D;
 #else
//   struct { byte h15,h14,h13,h12,h11,h10,h9,h8,
//                 h7,h6,h5,h4,h3,h2,h,l; } B;
//   struct { word h7,h6,h5,h4,h3,h2,h,l; } W;
//   dword D;
 #endif
#elif __64BIT__
 #ifdef LSB_FIRST
//   struct { byte l,h,h2,h3,h4,h5,h6,h7; } B;
//   struct { word l,h,h2,h3; } W;
//   dword D;
 #else
//   struct { byte h7,h6,h5,h4,h3,h2,h,l; } B;
//   struct { word h3,h2,h,l; } W;
//   dword D;
 #endif
#else
 #ifdef LSB_FIRST
   struct { byte l,h,h2,h3; } B;
   struct { word l,h; } W;
   dword D;
 #else
   struct { byte h3,h2,h,l; } B;
   struct { word h,l; } W;
   dword D;
 #endif
#endif
} z80regpair;

/****************************************************************************/
/*** End of machine dependent definitions                                 ***/
/****************************************************************************/

/****************************************************************************/
/* The Z80 registers. HALT is set to 1 when the CPU is halted, the refresh  */
/* register is calculated as follows: refresh=(Regs.R&127)|(Regs.R2&128)    */
/****************************************************************************/
typedef struct
{
  z80regpair AF,BC,DE,HL,IX,IY,PC,SP;
  z80regpair AF2,BC2,DE2,HL2;
  unsigned IFF1,IFF2,HALT,IM,I,R,R2;
  int ICount;       // T-state count
} CPU_Regs;

#define Z80_IGNORE_INT  -1   /* Ignore interrupt                            */
#define Z80_NMI_INT     -2   /* Execute NMI                                 */
#define Z80_NORM_INT    -3   /* Execute NMI                                 */

void InitTables (void);
void Z80_SingleInstruction(void);  /* Execute one single CPU instruction    */
void Z80_SetWaitStates (int n);    /* Set number of memory wait states.     */
                                   /* This only affects opcode fetching, so */
                                   /* wait state adjustment is still        */
                                   /* necessary in Z80_RDMEM, Z80_RDOP_ARG, */
                                   /* Z80_RDSTACK and Z80_WRSTACK           */
int Z80_Interrupt(void);           /* This is called after IPeriod T-States */
                                   /* have been executed. It should return  */
                                   /* Z80_IGNORE_INT, Z80_NMI_INT or a byte */
                                   /* identifying the device (most often    */
                                   /* 0xFF)                                 */

void Z80_Patch (CPU_Regs *Regs);   /* Called when ED FE occurs. Can be used */
                                   /* to emulate disk access etc.           */
void Z80_Reti (void);              /* Called when RETI occurs               */
void Z80_Retn (void);              /* Called when RETN occurs               */


/****************************************************************************/
/* Definitions of functions to read/write memory and I/O ports              */
/* You can replace these with your own, inlined if necessary                */
/****************************************************************************/
#include "Z80IO.h"


extern CPU_Regs R;

#ifdef DEBUG
extern byte debugmemory[];
#endif

#endif /* _Z80_H */

