// $Id$

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

const CPU::CPURegs &CPU::getCPURegs()
{
	return R;
}

void CPU::setCPURegs(const CPURegs &regs)
{
	R = regs;
}

