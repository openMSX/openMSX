// $Id: CPU.cc,v

#include "CPU.hh"

CPU::CPU(CPUInterface *interf) 
{
	interface = interf;
	//targetChanged = true; // not necessary
}

CPU::~CPU()
{
}

void CPU::executeUntilTarget(const EmuTime &time)
{
	setTargetTime(time);
	execute();
}

void CPU::setTargetTime(const EmuTime &time)
{
	targetTime = time;
	targetChanged = true;
}
const EmuTime &CPU::getTargetTime()
{
	return targetTime;
}

