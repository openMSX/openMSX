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

void CPU::executeUntilTarget(const EmuTime &time)
{
	setTargetTime(time);
	execute();
}

void CPU::setCurrentTime(const EmuTime &time)
{
	currentTime = time;
}
const EmuTime &CPU::getCurrentTime()
{
	return currentTime;
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

