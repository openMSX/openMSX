// $Id$

#include <cassert>
#include <list>
#include "MSXCPU.hh"
#include "msxconfig.hh"


MSXCPU::MSXCPU(MSXConfig::Device *config, const EmuTime &time)
	: MSXDevice(config, time)
{
	PRT_DEBUG("Creating an MSXCPU object");
	oneInstance = this;
	z80 = new Z80(MSXMotherBoard::instance(), 1, time);
	//r800 = new R800(MSXMotherBoard::instance(), time);
	activeCPU = z80;	// setActiveCPU(CPU_Z80);
	reset(time);
}

MSXCPU::~MSXCPU()
{
	PRT_DEBUG("Destroying an MSXCPU object");
	delete z80;
	//delete r800;
}

MSXCPU* MSXCPU::instance()
{
	if (oneInstance == NULL) {
		std::list<MSXConfig::Device*> deviceList;
		deviceList = MSXConfig::instance()->getDeviceByType("CPU");
		if (deviceList.size() != 1)
			PRT_ERROR("There must be exactly one CPU in config file");
		MSXConfig::Device* config = deviceList.front();
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
	//r800->reset(time);
}


void MSXCPU::setActiveCPU(CPUType cpu)
{
	CPU *newCPU;
	switch (cpu) {
	case CPU_Z80:
		newCPU = z80;
		break;
//	case CPU_R800:
//		newCPU = r800;
//		break;
	default:
		assert(false);
		newCPU = NULL;	// prevent warning
	}
	if (newCPU != activeCPU) {
		newCPU->setCurrentTime(activeCPU->getCurrentTime());
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


void MSXCPU::executeUntilEmuTime(const EmuTime &time)
{
	assert(false);
}

