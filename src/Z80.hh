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

#ifndef __Z80_H__
#define __Z80_H__

#include "config.h"
#include "openmsx.hh"


class Z80Interface {
	public:
		virtual byte readIO   (word port) = 0;
		virtual void writeIO  (word port,byte value) = 0;
		virtual byte readMem  (word address) = 0;
		virtual void writeMem (word address, byte value) = 0;

		/**
		 * Called when ED FE occurs. Can be used
		 * to emulated disk access etc.
		 */
		virtual void Z80_Patch () {}
		/**
		 * Called when RETI accurs
		 */
		virtual void Z80_Reti () {}
		/**
		 * Called when RETN occurs
		 */
		virtual void Z80_Retn () {}
};

class Z80;
typedef void (Z80::*opcode_fn)();

class Z80 {
	public:
		Z80(Z80Interface *interf);
		~Z80();
		void init();
		void reset();

		int Z80_SingleInstruction();  /* Execute one single CPU instruction    */
		void Z80_SetWaitStates (int n);    /* Set number of memory wait states.     */
                                   /* This only affects opcode fetching, so */
                                   /* wait state adjustment is still        */
                                   /* necessary in Z80_RDMEM, Z80_RDOP_ARG, */
                                   /* Z80_RDSTACK and Z80_WRSTACK           */
		void Z80_Interrupt(int j);


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

/****************************************************************************/
/* Define a Z80 word. Upper bytes are always zero                           */
/****************************************************************************/
typedef union {
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
typedef struct {
	z80regpair AF,  BC,  DE,  HL, IX, IY, PC, SP;
	z80regpair AF2, BC2, DE2, HL2;
	bool IFF1, IFF2, HALT;
	unsigned IM, I, R, R2;
	int ICount;       // T-state count
} CPU_Regs;

#define Z80_IGNORE_INT  -1   /* Ignore interrupt                            */
#define Z80_NMI_INT     -2   /* Execute NMI                                 */
#define Z80_NORM_INT    -3   /* Execute NMI                                 */

	private:
		#include "Z80Core.hh"

		static const byte S_FLAG = 0x80;
		static const byte Z_FLAG = 0x40;
		static const byte H_FLAG = 0x10;
		static const byte V_FLAG = 0x04;
		static const byte N_FLAG = 0x02;
		static const byte C_FLAG = 0x01;

		static byte PTable[512];
		static byte ZSTable[512];
		static byte ZSPTable[512];
		static const short DAATable[2048];
		
		static opcode_fn opcode_dd_cb[256];
		static opcode_fn opcode_fd_cb[256];
		static opcode_fn opcode_cb[256];
		static opcode_fn opcode_dd[256];
		static opcode_fn opcode_ed[256];
		static opcode_fn opcode_fd[256];
		static opcode_fn opcode_main[256];

		#ifdef DEBUG
			byte debugmemory[65536];
			char to_print_string[300];
		#endif
		
		//TODO should not be static
		static unsigned cycles_main[256];
		static unsigned cycles_cb[256];
		static unsigned cycles_xx_cb[256];
		static unsigned cycles_xx[256];
		static unsigned cycles_ed[256];
		
		Z80Interface *interface;

		CPU_Regs R;
};

#endif // __Z80_H__

