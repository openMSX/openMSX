// $Id: CPU.cc,v

#include "CPU.hh"

CPU::CPU(CPUInterface *interf, int clockFreq) 
	: targetTime(clockFreq), currentTime(clockFreq)
{
	interface = interf;
	//targetChanged = true; // not necessary
}

CPU::~CPU()
{
}

void CPU::executeUntilTarget(const Emutime &time)
{
	setTargetTime(time);
	execute();
}

void CPU::setCurrentTime(const Emutime &time)
{
	currentTime = time;
}
const Emutime &CPU::getCurrentTime()
{
	return currentTime;
}

void CPU::setTargetTime(const Emutime &time)
{
	targetTime = time;
	targetChanged = true;
}
const Emutime &CPU::getTargetTime()
{
	return targetTime;
}

