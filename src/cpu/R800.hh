// $Id$

#ifndef __R800_HH__
#define __R800_HH__

#include "config.h"
#include "openmsx.hh"
#include "CPU.hh"
#include "EmuTime.hh"

// forward declarations
class R800;
class CPUInterface;
typedef void (R800::*opcode_R800)();
typedef void (R800::*opcode_R800_n)(offset n);

class R800 : public CPU {
	public:
		R800(CPUInterface *interf, const EmuTime &time);
		virtual ~R800();
		virtual void setCurrentTime(const EmuTime &time);
		virtual const EmuTime &getCurrentTime();

	private:
		#undef _CPU_
		#define _CPU_ R800
		#include "CPUCore.n1"

		EmuTimeFreq<7159090> currentTime;

		// opcode function pointers
		static const opcode_R800_n opcode_dd_cb[256];
		static const opcode_R800_n opcode_fd_cb[256];
		static const opcode_R800   opcode_cb[256];
		static const opcode_R800   opcode_dd[256];
		static const opcode_R800   opcode_ed[256];
		static const opcode_R800   opcode_fd[256];
		static const opcode_R800   opcode_main[256];
};

#endif

