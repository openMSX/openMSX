// $Id$

#include "CPUInterface.hh"
#include "CPU.hh"
#include "CPU.ii"


CPU::CPU(CPUInterface *interf) 
{
	interface = interf;
	//targetChanged = true; // not necessary
	
	invalidateCache(0x0000, 0x10000);
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


void CPU::invalidateCache(word start, int length)
{
	PRT_DEBUG("cache: invalidate "<<start<<" "<<length );
	int num = (length+CACHE_LINE_LOW) >> CACHE_LINE_BITS;
	memset(&readCacheLine [start>>CACHE_LINE_BITS], 0xff, num*sizeof(byte*)); //	-1
	memset(&writeCacheLine[start>>CACHE_LINE_BITS], 0xff, num*sizeof(byte*)); //	-1
}
