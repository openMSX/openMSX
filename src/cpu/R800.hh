// $Id$

#ifndef __R800_HH__
#define __R800_HH__

#include "config.h"
#include "openmsx.hh"
#include "CPU.hh"
#include "EmuTime.hh"


namespace openmsx {

class CPUInterface;
class R800;
typedef void (R800::*R800_ResumeFunc)();


class R800 : public CPU {
	public:
		R800(CPUInterface *interf, const EmuTime &time);
		virtual ~R800();
		virtual void setCurrentTime(const EmuTime &time);
		virtual const EmuTime &getCurrentTime() const;

	private:
		#include "CPUCore.n1"

		static const int IO_DELAY1 = 0;
		static const int IO_DELAY2 = 1;
		static const int MEM_DELAY1 = 0;
		static const int MEM_DELAY2 = 1;

		static const int CLOCK_FREQ = 7159090;
		EmuTimeFreq<CLOCK_FREQ> currentTime;

		// opcode function pointers
		static const R800_ResumeFunc opcode_dd_cb[256];
		static const R800_ResumeFunc opcode_fd_cb[256];
		static const R800_ResumeFunc opcode_cb[256];
		static const R800_ResumeFunc opcode_dd[256];
		static const R800_ResumeFunc opcode_ed[256];
		static const R800_ResumeFunc opcode_fd[256];
		static const R800_ResumeFunc opcode_main[256];

		R800_ResumeFunc resume;
};

} // namespace openmsx

#endif

