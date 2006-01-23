// $Id$

#ifndef Z80_HH
#define Z80_HH

#include "DynamicClock.hh"

namespace openmsx {

class Z80TYPE
{
protected:
	static const int CLOCK_FREQ = 3579545;
	static const int WAIT_CYCLES = 1;

	Z80TYPE(const EmuTime& time)
		: clock(time)
	{
	}

	inline void M1_DELAY()       { clock += 1 + WAIT_CYCLES; }
	inline void ADD_16_8_DELAY() { clock += 5; }
	inline void OP_16_16_DELAY() { clock += 7; }
	inline void INC_16_DELAY()   { clock += 2; }
	inline void BLOCK_DELAY()    { clock += 5; }
	inline void RLD_DELAY()      { clock += 4; }
	inline void EX_SP_HL_DELAY() { clock += 2; }
	inline void LDI_DELAY()      { clock += 2; }
	inline void DD_CB_DELAY()    { clock += 2; }
	inline void PARALLEL_DELAY() { clock += 2; }
	inline void NMI_DELAY()      { clock += 11; }
	inline void IM0_DELAY()      { clock += 2; }
	inline void IM1_DELAY()      { clock += 2; }
	inline void IM2_DELAY()      { clock += 19; }
	inline void PUSH_DELAY()     { clock += 1; }
	inline void INC_DELAY()      { clock += 1; }
	inline void SMALL_DELAY()    { clock += 1; }  // TODO more detailed?
	inline void SET_IM_DELAY()   { }
	inline void DI_DELAY()       { }
	inline void RETN_DELAY()     { }
	inline int haltStates() { return 4 + WAIT_CYCLES; } // HALT + M1

	inline void PRE_RDMEM_OPCODE(word /*address*/) { clock += 1; }
	inline void PRE_RDMEM       (word /*address*/) { clock += 1; }
	inline void PRE_WRMEM       (word /*address*/) { clock += 1; }
	inline void POST_MEM        (word /*address*/) { clock += 2; }

	inline void PRE_IO (word /*port*/) { clock += 1; }
	inline void POST_IO(word /*port*/) { clock += 3; }

	inline void R800Refresh()
	{
		// nothing
	}

	DynamicClock clock;
};

} // namespace openmsx

#endif
