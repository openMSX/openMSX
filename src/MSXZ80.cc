// $Id$

#include <iostream>
#include <cassert>

#include "MSXZ80.hh"
//#include "Scheduler.hh"


MSXZ80::MSXZ80() : MSXCPUDevice(3579545)
{
	PRT_DEBUG("instantiating an MSXZ80 object");
	z80 = new Z80(this);
	z80->Z80_SetWaitStates(1);	// 1 extra clock pulse after each instruction
}

MSXZ80::~MSXZ80()
{
	PRT_DEBUG("destructing an MSXZ80 object");
	delete z80;
}


void MSXZ80::init()
{
	z80->init();

	// optimizations
	mb = MSXMotherBoard::instance();
	cpu = MSXCPU::instance();
}

void MSXZ80::reset()
{
	z80->reset();
}

void MSXZ80::executeUntilTarget()
{
	while (currentTime < cpu->getTargetTime()) {
		int tStates = z80->Z80_SingleInstruction();
		//assert(tStates>0);
		currentTime += (uint64)tStates;
	}
}


bool MSXZ80::IRQStatus()
{
	return mb->IRQstatus();
}

byte MSXZ80::readIO(word port)
{
	return mb->readIO(port&255, currentTime);
}

void MSXZ80::writeIO (word port, byte value) {
	mb->writeIO(port&255, value, currentTime);
}

byte MSXZ80::readMem(word address) {
	return  mb->readMem(address, currentTime);
}

void MSXZ80::writeMem(word address, byte value) {
	mb->writeMem(address, value, currentTime);
}

