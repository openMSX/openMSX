// $Id$

#ifndef __R800_HH__
#define __R800_HH__

#include "EmuTime.hh"

namespace openmsx {

class R800TYPE
{
protected:
	static const int CLOCK_FREQ = 7159090;
	static const int IO_DELAY1 = 0;
	static const int IO_DELAY2 = 3;
	static const int MEM_DELAY1 = 0;
	static const int MEM_DELAY2 = 1;

	inline void M1_DELAY()       { }
	inline void ADD_16_8_DELAY() { clock += 1; }
	inline void OP_16_16_DELAY() { }
	inline void INC_16_DELAY()   { }
	inline void BLOCK_DELAY()    { clock += 1; }
	inline void RLD_DELAY()      { clock += 1; }
	inline void EX_SP_HL_DELAY() { }
	inline void LDI_DELAY()      { }
	inline void DD_CB_DELAY()    { }
	inline void PARALLEL_DELAY() { }
	inline void NMI_DELAY()      { } // TODO check this
	inline void IM0_DELAY()      { } // TODO check this
	inline void IM1_DELAY()      { } // TODO check this
	inline void IM2_DELAY()      { clock += 3; } // TODO check this
	inline void PUSH_DELAY()     { clock += 1; }
	inline void INC_DELAY()      { clock += 1; }
	inline void SMALL_DELAY()    { }
	inline int haltStates() { return 1; }	// HALT + M1 // TODO check this

	R800TYPE(const EmuTime& time)
		: lastRefreshTime(time)
		, lastPage(-1)
	{
	}
	
	inline void PRE_RDMEM_OPCODE(word address)
	{
		int newPage = address >> 8;
		if (newPage != lastPage) {
			lastPage = newPage;
			clock += 1;
		}
	}

	inline void PRE_RDMEM(word address)
	{
		clock += 1;
		lastPage = -1;
	}

	inline void PRE_WRMEM(word address)
	{
		clock += 1;
		lastPage = -1;
	}

	inline void R800Refresh()
	{
		// documentation says refresh every 222 clocks 
		//  duration:  256/1024KB  13.5 clocks
		//             512KB       21.5 clocks
		if (lastRefreshTime.getTicksTill(clock.getTime()) >= 222) {
			lastRefreshTime.advance(clock.getTime());
			clock += 22;
		}
	}

	DynamicClock clock;
	Clock<CLOCK_FREQ> lastRefreshTime;
	int lastPage;
};

} // namespace openmsx

#endif

