// $Id$

#include <iostream>
#include <assert.h>

#include "MSXZ80.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"


MSXZ80::MSXZ80() : MSXCPUDevice(3579545)
{
	PRT_DEBUG("instantiating an MSXZ80 object");
	z80 = new Z80(this);
}

MSXZ80::~MSXZ80()
{
	PRT_DEBUG("destructing an MSXZ80 object");
	delete z80;
}


void MSXZ80::init()
{
	z80->init();
}

void MSXZ80::reset()
{
	z80->reset();
}

void MSXZ80::executeUntilTarget()
{
	while (currentTime < MSXCPU::instance()->getTargetTime()) {
		currentTime += z80->Z80_SingleInstruction();
	}
}


bool MSXZ80::IRQStatus()
{
	return MSXMotherBoard::instance()->IRQstatus();
}

byte MSXZ80::readIO(word port)
{
	return MSXMotherBoard::instance()->readIO(port&255, currentTime);
}

void MSXZ80::writeIO (word port, byte value) {
	MSXMotherBoard::instance()->writeIO(port&255, value, currentTime);
}

byte MSXZ80::readMem(word address) {
	return  MSXMotherBoard::instance()->readMem(address, currentTime);
}

void MSXZ80::writeMem(word address, byte value) {
	MSXMotherBoard::instance()->writeMem(address, value, currentTime);
}

