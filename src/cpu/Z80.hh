// $Id$

#ifndef Z80_HH
#define Z80_HH

#include "CPUClock.hh"
#include <cassert>

namespace openmsx {

class Z80TYPE : public CPUClock
{
protected:
	static const int CLOCK_FREQ = 3579545;
	static const int WAIT_CYCLES = 1;

	Z80TYPE(const EmuTime& time)
		: CPUClock(time)
	{
	}

	inline void M1_DELAY()       { add(1 + WAIT_CYCLES); }
	inline void ADD_16_8_DELAY() { add(5); }
	inline void OP_16_16_DELAY() { add(7); }
	inline void INC_16_DELAY()   { add(2); }
	inline void BLOCK_DELAY()    { add(5); }
	inline void RLD_DELAY()      { add(4); }
	inline void EX_SP_HL_DELAY() { add(2); }
	inline void LDI_DELAY()      { add(2); }
	inline void DD_CB_DELAY()    { add(2); }
	inline void PARALLEL_DELAY() { add(2); }
	inline void NMI_DELAY()      { add(11); }
	inline void IM0_DELAY()      { add(2); }
	inline void IM1_DELAY()      { add(2); }
	inline void IM2_DELAY()      { add(19); }
	inline void PUSH_DELAY()     { add(1); }
	inline void INC_DELAY()      { add(1); }
	inline void SMALL_DELAY()    { add(1); }  // TODO more detailed?
	inline void SET_IM_DELAY()   { }
	inline void DI_DELAY()       { }
	inline void RETN_DELAY()     { }
	inline void MULUB_DELAY()    { assert(false); }
	inline void MULUW_DELAY()    { assert(false); }
	inline unsigned haltStates() { return 4 + WAIT_CYCLES; } // HALT + M1

	inline void PRE_RDMEM_OPCODE(word /*address*/) { add(1); }
	inline void PRE_RDMEM       (word /*address*/) { add(1); }
	inline void PRE_WRMEM       (word /*address*/) { add(1); }
	inline void POST_MEM        (word /*address*/) { add(2); }

	inline void PRE_IO (word /*port*/) { add(1); }
	inline void POST_IO(word /*port*/) { add(3); }

	inline void R800Refresh()
	{
		// nothing
	}
};

} // namespace openmsx

#endif
