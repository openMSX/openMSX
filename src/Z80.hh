// $Id$

//TODO update heading 
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
#include "CPU.hh"
#include "CPUInterface.hh"

#ifdef DEBUG
//#define Z80DEBUG
#endif

#ifndef WORDS_BIGENDIAN
	#define LSB_FIRST
#endif

typedef union {
#ifdef LSB_FIRST
   struct { byte l,h; } B;
#else
   struct { byte h,l; } B;
#endif
   word w;
} z80regpair;

typedef signed char offset;


class Z80;
typedef void (Z80::*opcode_fn)();

class Z80 : public CPU {
	public:
		Z80(CPUInterface *interf, int clockFreq, int waitCycles);
		virtual ~Z80();
		
		void reset();

		/**
		 * Execute CPU till a previously set target-time, the target
		 * may change during emulation
		 */
		void execute();


	private:
		int executeHelper();
		void M1Cycle();
		void init();
		
		#include "Z80Core.hh"

		// flag positions
		static const byte S_FLAG = 0x80;
		static const byte Z_FLAG = 0x40;
		static const byte Y_FLAG = 0x20;
		static const byte H_FLAG = 0x10;
		static const byte X_FLAG = 0x08;
		static const byte V_FLAG = 0x04;
		static const byte P_FLAG = V_FLAG;
		static const byte N_FLAG = 0x02;
		static const byte C_FLAG = 0x01;

		// flag-register tables
		static byte ZSTable[256];
		static byte XYTable[256];
		static byte ZSXYTable[256];
		static byte ZSPXYTable[256];
		static const word DAATable[2048];
		static const byte irep_tmp1[4][4];
		static const byte drep_tmp1[4][4];
		static const byte breg_tmp2[256];
		
		// opcode function pointers
		static const opcode_fn opcode_dd_cb[256];
		static const opcode_fn opcode_fd_cb[256];
		static const opcode_fn opcode_cb[256];
		static const opcode_fn opcode_dd[256];
		static const opcode_fn opcode_ed[256];
		static const opcode_fn opcode_fd[256];
		static const opcode_fn opcode_main[256];

		// T-States tables 
		static const int cycles_main[256];
		static const int cycles_cb[256];
		static const int cycles_xx_cb[256];
		static const int cycles_xx[256];
		static const int cycles_ed[256];

		z80regpair AF,  BC,  DE,  HL, IX, IY, PC, SP;
		z80regpair AF2, BC2, DE2, HL2;
		bool nextIFF1, IFF1, IFF2, HALT;
		byte IM, I;
		byte R, R2;	// refresh = R&127 | R2&128
		int ICount;	// T-state count
		int waitCycles;
		
		#ifdef Z80DEBUG
			byte debugmemory[65536];
			char to_print_string[300];
		#endif
};

#endif // __Z80_H__

