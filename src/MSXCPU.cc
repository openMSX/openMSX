// $Id$

#include <cassert>
#include "MSXCPU.hh"


MSXCPU::MSXCPU(MSXConfig::Device *config) : MSXDevice(config)
{
	PRT_DEBUG("Creating an MSXCPU object");
	oneInstance = this;
	z80 = new Z80(this, 3579545, 1);
	//r800 = new R800(this, 3579545*2);
	activeCPU = z80;	// setActiveCPU(CPU_Z80);
}

MSXCPU::~MSXCPU()
{
	PRT_DEBUG("Destroying an MSXCPU object");
	delete z80;
	//delete r800;
}

MSXCPU* MSXCPU::instance()
{
	assert (oneInstance != NULL );
	return oneInstance;
}
MSXCPU* MSXCPU::oneInstance = NULL;


void MSXCPU::init()
{
	MSXDevice::init();
	mb = MSXMotherBoard::instance();
}

void MSXCPU::reset()
{
	MSXDevice::reset();
	z80->reset();
	//r800->reset();
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



bool MSXCPU::IRQStatus()
{
	return mb->IRQstatus();
}

byte MSXCPU::readIO(word port, EmuTime &time)
{
	return mb->readIO(port&255, time);
}

void MSXCPU::writeIO (word port, byte value, EmuTime &time) {
	mb->writeIO(port&255, value, time);
}

byte MSXCPU::readMem(word address, EmuTime &time) {
	return  mb->readMem(address, time);
}

void MSXCPU::writeMem(word address, byte value, EmuTime &time) {
	mb->writeMem(address, value, time);
}

