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
#include "EmuTime.hh"

#ifdef DEBUG
//#define Z80DEBUG
#endif

// forward declarations
class Z80;
class CPUInterface;
typedef void (Z80::*opcode_fn)();

class Z80 : public CPU {
	public:
		Z80(CPUInterface *interf, int waitCycles, const EmuTime &time);
		virtual ~Z80();
		
		void reset(const EmuTime &time);

		/**
		 * Execute CPU till a previously set target-time, the target
		 * may change during emulation
		 */
		void execute();

		void setCurrentTime(const EmuTime &time);
		const EmuTime &getCurrentTime();

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

	private:
		inline void executeInstruction(byte opcode);
		inline void M1Cycle();
		
		#include "Z80Core.hh"

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

		int waitCycles;
		
		#ifdef Z80DEBUG
			byte debugmemory[65536];
			char to_print_string[300];
		#endif

		EmuTimeFreq<3579545> currentTime;
};

#endif // __Z80_H__

