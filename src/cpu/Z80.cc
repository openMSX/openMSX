// $Id$

// TODO update heading
/*** Z80Em: Portable Z80 emulator *******************************************/
/***                                                                      ***/
/***                                 Z80.c                                ***/
/***                                                                      ***/
/*** This file contains the emulation code                                ***/
/***                                                                      ***/
/*** Copyright (C) Marcel de Kogel 1996,1997                              ***/
/***     You are not allowed to distribute this software commercially     ***/
/***     Please, notify me, if you make any changes to this file          ***/
/****************************************************************************/

#include <cassert>
#include "Z80.hh"
#include "CPUInterface.hh"
#include "Z80Tables.nn"
#include "CPU.ii"
#ifdef CPU_DEBUG
#include <stdio.h>
#include "Z80Dasm.h"
#include "ConsoleSource/Console.hh"
#endif


Z80::Z80(CPUInterface *interf, int waitCycl, const EmuTime &time) : CPU(interf)
{
	makeTables();
	waitCycles = waitCycl;
	reset(time);

	#ifdef CPU_DEBUG
	Console::instance()->registerCommand(debugCmd, "cpudebug");
	#endif
}
Z80::~Z80()
{
}

void Z80::setCurrentTime(const EmuTime &time)
{
	currentTime = time;
}
const EmuTime &Z80::getCurrentTime()
{
	return currentTime;
}



inline void Z80::M1_DELAY()       { currentTime += 1+waitCycles; }
inline void Z80::IOPORT_DELAY1()  { currentTime += 1; }
inline void Z80::IOPORT_DELAY2()  { currentTime += 3; }
inline void Z80::MEM_DELAY1()     { currentTime += 1; }
inline void Z80::MEM_DELAY2()     { currentTime += 2; }
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
inline int Z80::haltStates() { return 4 + waitCycles; }	// HALT + M1

#include "CPUCore.n2"


#ifdef CPU_DEBUG
void Z80::DebugCmd::execute(const char* commandLine)
{
	Z80::cpudebug = !Z80::cpudebug;
}
void Z80::DebugCmd::help(const char* commandLine)
{
}
bool Z80::cpudebug = false;
#endif

