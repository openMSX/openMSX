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

#include "Z80.hh"

namespace openmsx {

#include "Z80Tables.nn"

Z80::Z80(const EmuTime &time)
	: CPU("z80", CLOCK_FREQ)
{
	reset(time);
}

Z80::~Z80()
{
}

inline void Z80::M1_DELAY()       { clock += 1 + WAIT_CYCLES; }
inline void Z80::ADD_16_8_DELAY() { clock += 5; }
inline void Z80::OP_16_16_DELAY() { clock += 7; }
inline void Z80::INC_16_DELAY()   { clock += 2; }
inline void Z80::BLOCK_DELAY()    { clock += 5; }
inline void Z80::RLD_DELAY()      { clock += 4; }
inline void Z80::EX_SP_HL_DELAY() { clock += 2; }
inline void Z80::LDI_DELAY()      { clock += 2; }
inline void Z80::DD_CB_DELAY()    { clock += 2; }
inline void Z80::PARALLEL_DELAY() { clock += 2; }
inline void Z80::NMI_DELAY()      { clock += 11; }
inline void Z80::IM0_DELAY()      { clock += 2; }
inline void Z80::IM1_DELAY()      { clock += 2; }
inline void Z80::IM2_DELAY()      { clock += 19; }
inline void Z80::PUSH_DELAY()     { clock += 1; }
inline void Z80::INC_DELAY()      { clock += 1; }
inline void Z80::SMALL_DELAY()    { clock += 1; }  // TODO more detailed?
inline int Z80::haltStates() { return 4 + WAIT_CYCLES; } // HALT + M1

inline void Z80::RDMEM_OPCODE(word address, byte &result)
{
	RDMEM_common(address, result);
}

inline void Z80::RDMEM(word address, byte &result)
{
	RDMEM_common(address, result);
}

inline void Z80::WRMEM(word address, byte value)
{
	WRMEM_common(address, value);
}

inline void Z80::R800Refresh()
{
	// nothing
}

} // namespace openmsx

#define _CPU_ Z80
#include "CPUCore.n2"

