// $Id$

/**
 * Based on:
 *    Z80Em: Portable Z80 emulator
 *    written by Marcel de Kogel 1996,1997
 * heavily rewritten to fit openMSX structure
 */

#ifndef __Z80_HH__
#define __Z80_HH__

#include "config.h"
#include "openmsx.hh"
#include "CPU.hh"
#include "EmuTime.hh"

// forward declarations
class Z80;
class CPUInterface;
typedef void (Z80::*opcode_Z80)();
typedef void (Z80::*opcode_Z80_n)(offset n);

class Z80 : public CPU {
	public:
		Z80(CPUInterface *interf, int waitCycles, const EmuTime &time);
		virtual ~Z80();
		virtual void setCurrentTime(const EmuTime &time);
		virtual const EmuTime &getCurrentTime();

	private:
		#undef _CPU_
		#define _CPU_ Z80
		#include "CPUCore.n1"

		int waitCycles;
		EmuTimeFreq<3579545> currentTime;

		// opcode function pointers
		static const opcode_Z80_n opcode_dd_cb[256];
		static const opcode_Z80_n opcode_fd_cb[256];
		static const opcode_Z80   opcode_cb[256];
		static const opcode_Z80   opcode_dd[256];
		static const opcode_Z80   opcode_ed[256];
		static const opcode_Z80   opcode_fd[256];
		static const opcode_Z80   opcode_main[256];
};

#endif

