// $Id$

#ifndef __Z80_HH__
#define __Z80_HH__

#include "config.h"
#include "CPU.hh"

namespace openmsx {

class Z80;
typedef void (Z80::*Z80_ResumeFunc)();


/** Emulation of the Z80 CPU.
  * Based on Z80Em: Portable Z80 emulator
  * written by Marcel de Kogel 1996, 1997.
  * Heavily rewritten to fit openMSX structure.
  */
class Z80 : public CPU
{
public:
	static const int CLOCK_FREQ = 3579545;
	Z80(const EmuTime &time);
	virtual ~Z80();

private:
	#include "CPUCore.n1"

	static const int IO_DELAY1 = 1;
	static const int IO_DELAY2 = 3;
	static const int MEM_DELAY1 = 1;
	static const int MEM_DELAY2 = 2;
	static const int WAIT_CYCLES = 1;

	// opcode function pointers
	static const Z80_ResumeFunc opcode_dd_cb[256];
	static const Z80_ResumeFunc opcode_fd_cb[256];
	static const Z80_ResumeFunc opcode_cb[256];
	static const Z80_ResumeFunc opcode_dd[256];
	static const Z80_ResumeFunc opcode_ed[256];
	static const Z80_ResumeFunc opcode_fd[256];
	static const Z80_ResumeFunc opcode_main[256];
};

} // namespace openmsx

#endif

