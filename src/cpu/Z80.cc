// $Id$

/****************************************************************************/
/*** During the proof of concept openMSX used                              **/
/*** Z80Em, the Portable Z80 emulator                                      **/
/*** of Marcel de Kogel ( (C)1996,1997 )                                   **/
/***                                                                       **/
/*** Now this code is completely rewritten by Wouter Vermaelen for openMSX **/
/*** with inter-instruction timings, C++ enabling,EmuTime support etc etc  **/
/*** and bears no resemblance anymore with Z80Em                           **/
/***                                                                       **/
/****************************************************************************/

#include <cassert>
#include "Z80.hh"
#include "CPUInterface.hh"


namespace openmsx {

#include "Z80Tables.nn"

Z80::Z80(const EmuTime &time)
{
	reset(time);
}
Z80::~Z80()
{
}

void Z80::setCurrentTime(const EmuTime &time)
{
	currentTime = time;
}
const EmuTime &Z80::getCurrentTime() const
{
	return currentTime;
}


inline void Z80::M1_DELAY()       { currentTime += 1 + WAIT_CYCLES; }
inline void Z80::ADD_16_8_DELAY() { currentTime += 5; }
inline void Z80::OP_16_16_DELAY() { currentTime += 5; }
inline void Z80::INC_16_DELAY()   { currentTime += 2; }
inline void Z80::BLOCK_DELAY()    { currentTime += 5; }
inline void Z80::RLD_DELAY()      { currentTime += 4; }
inline void Z80::EX_SP_HL_DELAY() { currentTime += 2; }
inline void Z80::LDI_DELAY()      { currentTime += 2; }
inline void Z80::DD_CB_DELAY()    { currentTime += 2; }
inline void Z80::PARALLEL_DELAY() { currentTime += 2; }
inline void Z80::NMI_DELAY()      { currentTime += 11; }
inline void Z80::IM0_DELAY()      { currentTime += 2; }
inline void Z80::IM1_DELAY()      { currentTime += 2; }
inline void Z80::IM2_DELAY()      { currentTime += 19; }
inline void Z80::SMALL_DELAY()    { currentTime += 1; }	// TODO more detailed?
inline int Z80::haltStates() { return 4 + WAIT_CYCLES; }	// HALT + M1

} // namespace openmsx

#define _CPU_ Z80
#include "CPUCore.n2"

