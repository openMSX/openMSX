// $Id$

#include "CPUInterface.hh"
#include "CPU.hh"
#include "CPU.ii"


CPU::CPU(CPUInterface *interf) 
{
	interface = interf;
	
	invalidateCache(0x0000, 0x10000/CPU::CACHE_LINE_SIZE);
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


void CPU::invalidateCache(word start, int num)
{
	//PRT_DEBUG("cache: invalidate "<<start<<" "<<num*CACHE_LINE_SIZE);
	memset(&readCacheLine  [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //	NULL
	memset(&writeCacheLine [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //
	memset(&readCacheTried [start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //	FALSE
	memset(&writeCacheTried[start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //
}
