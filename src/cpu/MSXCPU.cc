// $Id$

#include <cassert>
#include "MSXCPU.hh"
#include "MSXConfig.hh"
#include "MSXCPUInterface.hh"
#include "CPU.hh"
#include "Z80.hh"
#include "R800.hh"


MSXCPU::MSXCPU(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXCPU object");
	oneInstance = this;
	z80 = new Z80(MSXCPUInterface::instance(), 1, time);
	r800 = new R800(MSXCPUInterface::instance(), time);
	activeCPU = z80;	// setActiveCPU(CPU_Z80);
	reset(time);
}

MSXCPU::~MSXCPU()
{
	PRT_DEBUG("Destroying an MSXCPU object");
	delete z80;
	delete r800;
}

MSXCPU* MSXCPU::instance()
{
	if (oneInstance == NULL) {
		MSXConfig::Device* config = MSXConfig::Backend::instance()->getDeviceById("cpu");
		EmuTime zero;
		new MSXCPU(config, zero);
	}
	return oneInstance;
}
MSXCPU* MSXCPU::oneInstance = NULL;


void MSXCPU::reset(const EmuTime &time)
{
	MSXDevice::reset(time);
	z80->reset(time);
	r800->reset(time);
}


void MSXCPU::setActiveCPU(CPUType cpu)
{
	CPU *newCPU;
	switch (cpu) {
		case CPU_Z80:
			newCPU = z80;
			break;
		case CPU_R800:
			newCPU = r800;
			break;
		default:
			assert(false);
			newCPU = NULL;	// prevent warning
	}
	if (newCPU != activeCPU) {
		newCPU->setCurrentTime(activeCPU->getCurrentTime());
		newCPU->invalidateCache(0x0000, 0x10000/CPU::CACHE_LINE_SIZE);
		activeCPU = newCPU;
	}
}

void MSXCPU::executeUntilTarget(const EmuTime &time)
{
	activeCPU->executeUntilTarget(time);
}


void MSXCPU::setTargetTime(const EmuTime &time)
{
	activeCPU->setTargetTime(time);
}
const EmuTime &MSXCPU::getTargetTime()
{
	return activeCPU->getTargetTime();
}

const EmuTime &MSXCPU::getCurrentTime()
{
	return activeCPU->getCurrentTime();
}


void MSXCPU::invalidateCache(word start, int num)
{
	activeCPU->invalidateCache(start, num);
}

void MSXCPU::raiseIRQ()
{
	z80->raiseIRQ();
	r800->raiseIRQ();
}
void MSXCPU::lowerIRQ()
{
	z80->lowerIRQ();
	r800->lowerIRQ();
}
