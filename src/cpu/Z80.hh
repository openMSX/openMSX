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

#ifndef __Z80_HH__
#define __Z80_HH__

#include "config.h"
#include "openmsx.hh"
#include "CPU.hh"
#include "EmuTime.hh"

#ifdef CPU_DEBUG
#include "ConsoleSource/ConsoleCommand.hh"
#endif


// forward declarations
class Z80;
class CPUInterface;
typedef void (Z80::*opcode_fn)();
typedef void (Z80::*opcode_fn_n)(offset n);

class Z80 : public CPU {
	public:
		Z80(CPUInterface *interf, int waitCycles, const EmuTime &time);
		virtual ~Z80();
		virtual void setCurrentTime(const EmuTime &time);
		virtual const EmuTime &getCurrentTime();

	private:
		#define _CPU_ Z80
		#include "CPUCore.n1"

		int waitCycles;
		EmuTimeFreq<3579545> currentTime;

		// opcode function pointers
		static const opcode_fn_n opcode_dd_cb[256];
		static const opcode_fn_n opcode_fd_cb[256];
		static const opcode_fn opcode_cb[256];
		static const opcode_fn opcode_dd[256];
		static const opcode_fn opcode_ed[256];
		static const opcode_fn opcode_fd[256];
		static const opcode_fn opcode_main[256];
		
	#ifdef CPU_DEBUG
	public:
			byte debugmemory[65536];
			char to_print_string[300];
			static bool cpudebug;

			class DebugCmd : public ConsoleCommand {
				virtual void execute(const char *commandLine);
				virtual void help(const char *commandLine);
			};
			DebugCmd debugCmd;
	#endif
};

#endif // __Z80_H__

