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

#ifdef DEBUG
//#define Z80DEBUG
#endif

class Z80Interface {
	public:
		virtual byte readIO   (word port) = 0;
		virtual void writeIO  (word port,byte value) = 0;
		virtual byte readMem  (word address) = 0;
		virtual void writeMem (word address, byte value) = 0;

		virtual bool IRQStatus() = 0;
		virtual bool NMIStatus() { return false; }
		virtual byte dataBus() { return 255; }

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

		/**
		 * Execute one single CPU instruction
		 *  return the T-States of that instruction
		 */
		int Z80_SingleInstruction();
		
		/**
		 * Set number of memory wait states.
		 * This only affects opcode fetching, so
		 * wait state adjustment is still
		 * necessary in Z80_RDMEM, Z80_RDOP_ARG,
		 * Z80_RDSTACK and Z80_WRSTACK           
		 */
		void Z80_SetWaitStates (int n);


#ifndef WORDS_BIGENDIAN
	#define LSB_FIRST
#endif
typedef signed char    offset;

/****************************************************************************/
/* Define a Z80 word. Upper bytes are always zero                           */
/****************************************************************************/
typedef union {
#ifdef LSB_FIRST
   struct { byte l,h; } B;
#else
   struct { byte h,l; } B;
#endif
   word w;
} z80regpair;

	private:
		#include "Z80Core.hh"

		static const byte S_FLAG = 0x80;
		static const byte Z_FLAG = 0x40;
		static const byte Y_FLAG = 0x20;
		static const byte H_FLAG = 0x10;
		static const byte X_FLAG = 0x08;
		static const byte V_FLAG = 0x04;
		static const byte P_FLAG = V_FLAG;
		static const byte N_FLAG = 0x02;
		static const byte C_FLAG = 0x01;

		static byte ZSTable[256];
		static byte XYTable[256];
		static byte ZSXYTable[256];
		static byte ZSPXYTable[256];
		static const word DAATable[2048];
		
		static opcode_fn opcode_dd_cb[256];
		static opcode_fn opcode_fd_cb[256];
		static opcode_fn opcode_cb[256];
		static opcode_fn opcode_dd[256];
		static opcode_fn opcode_ed[256];
		static opcode_fn opcode_fd[256];
		static opcode_fn opcode_main[256];

		static byte irep_tmp1[4][4];
		static byte drep_tmp1[4][4];
		static byte breg_tmp2[256];

		#ifdef Z80DEBUG
			byte debugmemory[65536];
			char to_print_string[300];
		#endif
		
		//TODO should not be static
		static int waitStates;
		static int cycles_main[256];
		static int cycles_cb[256];
		static int cycles_xx_cb[256];
		static int cycles_xx[256];
		static int cycles_ed[256];
		
		Z80Interface *interface;

/****************************************************************************/
/* The Z80 registers. HALT is set to 1 when the CPU is halted, the refresh  */
/* register is calculated as follows: refresh=(Regs.R&127)|(Regs.R2&128)    */
/****************************************************************************/

		z80regpair AF,  BC,  DE,  HL, IX, IY, PC, SP;
		z80regpair AF2, BC2, DE2, HL2;
		bool nextIFF1, IFF1, IFF2, HALT;
		unsigned IM, I, R, R2;
		int ICount;       // T-state count
};

#endif // __Z80_H__

