// $Id$

#include "R800.hh"

namespace openmsx {

#include "R800Tables.nn"

R800::R800(const EmuTime& time)
	: CPU("r800", CLOCK_FREQ),
	  lastPage(-1),
	  lastRefreshTime(time)
{
	reset(time);
}

R800::~R800()
{
}

inline void R800::M1_DELAY()       { }
inline void R800::ADD_16_8_DELAY() { clock += 1; }
inline void R800::OP_16_16_DELAY() { }
inline void R800::INC_16_DELAY()   { }
inline void R800::BLOCK_DELAY()    { clock += 1; }
inline void R800::RLD_DELAY()      { clock += 1; }
inline void R800::EX_SP_HL_DELAY() { }
inline void R800::LDI_DELAY()      { }
inline void R800::DD_CB_DELAY()    { }
inline void R800::PARALLEL_DELAY() { }
inline void R800::NMI_DELAY()      { } // TODO check this
inline void R800::IM0_DELAY()      { } // TODO check this
inline void R800::IM1_DELAY()      { } // TODO check this
inline void R800::IM2_DELAY()      { clock += 3; } // TODO check this
inline void R800::PUSH_DELAY()     { clock += 1; }
inline void R800::INC_DELAY()      { clock += 1; }
inline void R800::SMALL_DELAY()    { }
inline int R800::haltStates() { return 1; }	// HALT + M1 // TODO check this

inline void R800::RDMEM_OPCODE(word address, byte &result)
{
	int newPage = address >> 8;
	if (newPage != lastPage) {
		lastPage = newPage;
		clock += 1;
		if (!clock.before(targetTime)) {
			extendTarget(clock.getTime());
		}
	}
	RDMEM_common(address, result);
}

inline void R800::RDMEM(word address, byte &result)
{
	clock += 1;
	if (!clock.before(targetTime)) {
		extendTarget(clock.getTime());
	}
	lastPage = -1;
	RDMEM_common(address, result);
}

inline void R800::WRMEM(word address, byte value)
{
	clock += 1;
	if (!clock.before(targetTime)) {
		extendTarget(clock.getTime());
	}
	lastPage = -1;
	WRMEM_common(address, value);
}

inline void R800::R800Refresh()
{
	// documentation says refresh every 222 clocks 
	//  duration:  256/1024KB  13.5 clocks
	//             512KB       21.5 clocks
	if (lastRefreshTime.getTicksTill(clock.getTime()) >= 222) {
		lastRefreshTime.advance(clock.getTime());
		clock += 22;
		if (!clock.before(targetTime)) {
			extendTarget(clock.getTime());
		}
	}
}

} // namespace openmsx

#define _CPU_ R800
#include "CPUCore.n2"

