// $Id$

#include <iostream>
#include <assert.h>

#include "MSXZ80.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "Z80IO.h"


CPU_Regs R;


MSXZ80::MSXZ80() : MSXCPUDevice(3579545)
{
	PRT_DEBUG("instantiating an MSXZ80 object");
}

MSXZ80::~MSXZ80()
{
	PRT_DEBUG("destructing an MSXZ80 object");
}


void MSXZ80::init()
{
	Z80_Running=1;
	InitTables();
}

void MSXZ80::reset()
{
	//TODO
}

void MSXZ80::IRQ(bool irq)
{
	Interrupt(Z80_NORM_INT);
}

void MSXZ80::executeUntilTarget()
{
	while (currentTime < MSXCPU::instance()->targetTime) {
		Z80_SingleInstruction();
		currentTime += R.ICount;
	}
}

