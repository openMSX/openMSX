// $Id$

#include <assert.h>
#include "MSXCPU.hh"


MSXCPU::MSXCPU()
{
	PRT_DEBUG("Creating an MSXCPU object");
	z80 = new MSXZ80();
	//r800 = new MSXR800();
}

MSXCPU::~MSXCPU()
{
	PRT_DEBUG("Destroying an MSXCPU object");
	delete z80;
	//delete r800;
}

MSXCPU* MSXCPU::instance()
{
	if (oneInstance == NULL ) {
		oneInstance = new MSXCPU();
	}
	return oneInstance;
}
MSXCPU* MSXCPU::oneInstance = NULL;


void MSXCPU::init()
{
	MSXDevice::init();
	activeCPU = z80;	// setActiveCPU(CPU_Z80);
	z80->init();
	//r800->init();
}

void MSXCPU::reset()
{
	MSXDevice::reset();
	z80->reset();
	//r800->reset();
}


void MSXCPU::setActiveCPU(CPUType cpu)
{
	MSXCPUDevice *newCPU = NULL;	// prevent warning
	switch (cpu) {
	case Z80:
		newCPU = z80;
		break;
//	case R800:
//		newCPU = r800;
//		break;
	}
	newCPU->setCurrentTime(activeCPU->getCurrentTime());
	activeCPU = newCPU;
}

void MSXCPU::executeUntilTarget(const Emutime &time)
{
	setTargetTime(time);
	activeCPU->executeUntilTarget();
}


void MSXCPU::setTargetTime(const Emutime &time)
{
	assert(getCurrentTime() <= time);
	targetTime = time;
}
const Emutime &MSXCPU::getTargetTime()
{
	return targetTime;
}

Emutime &MSXCPU::getCurrentTime()
{
	return activeCPU->getCurrentTime();
}


void MSXCPU::executeUntilEmuTime(const Emutime &time)
{
	assert(false);
}
