// $Id$

#include "R800.hh"
#include "CPUInterface.hh"


namespace openmsx {

#include "R800Tables.nn"

R800::R800(const EmuTime &time)
	: CPU("r800", CLOCK_FREQ)
{
	reset(time);
}
R800::~R800()
{
}

inline void R800::M1_DELAY()       { }
inline void R800::ADD_16_8_DELAY() { currentTime += 1; }
inline void R800::OP_16_16_DELAY() { }
inline void R800::INC_16_DELAY()   { }
inline void R800::BLOCK_DELAY()    { }
inline void R800::RLD_DELAY()      { currentTime += 1; }
inline void R800::EX_SP_HL_DELAY() { }
inline void R800::LDI_DELAY()      { }
inline void R800::DD_CB_DELAY()    { }
inline void R800::PARALLEL_DELAY() { }
inline void R800::NMI_DELAY()      { } // TODO check this
inline void R800::IM0_DELAY()      { } // TODO check this
inline void R800::IM1_DELAY()      { } // TODO check this
inline void R800::IM2_DELAY()      { currentTime += 3; } // TODO check this
inline void R800::SMALL_DELAY()    { }
inline int R800::haltStates() { return 1; }	// HALT + M1 // TODO check this

} // namespace openmsx

#define _CPU_ R800
#include "CPUCore.n2"

