// $Id$

#include "CPUInterface.hh"
#include "CPU.hh"
#include "CPU.ii"
#include "CPUTables.nn"

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

CPU::CPURegs &CPU::getCPURegs()
{
	return R;
}

void CPU::invalidateCache(word start, int num)
{
	//PRT_DEBUG("cache: invalidate "<<start<<" "<<num*CACHE_LINE_SIZE);
	memset(&readCacheLine  [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //	NULL
	memset(&writeCacheLine [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //
	memset(&readCacheTried [start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //	FALSE
	memset(&writeCacheTried[start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //
}


void CPU::makeTables()
{
	for (int i=0; i<256; ++i) {
		byte zFlag = (i==0) ? Z_FLAG : 0;
		byte sFlag = i & S_FLAG;
		byte xFlag = i & X_FLAG;
		byte yFlag = i & Y_FLAG;
		int p = 0;
		if (i&1)   ++p;
		if (i&2)   ++p;
		if (i&4)   ++p;
		if (i&8)   ++p;
		if (i&16)  ++p;
		if (i&32)  ++p;
		if (i&64)  ++p;
		if (i&128) ++p;
		byte pFlag = (p&1) ? 0 : V_FLAG;
		ZSTable[i]    = zFlag | sFlag;
		XYTable[i]    = xFlag | yFlag;
		ZSXYTable[i]  = zFlag | sFlag | xFlag | yFlag;
		ZSPXYTable[i] = zFlag | sFlag | xFlag | yFlag | pFlag;
	}
}

void CPU::reset(const EmuTime &time)
{
	// AF and SP are 0xffff
	// PC, R, IFF1, IFF2, HALT and IM are 0x0
	// all others are random
	R.AF.w = 0xffff;
	R.BC.w = 0xffff;
	R.DE.w = 0xffff;
	R.HL.w = 0xffff;
	R.IX.w = 0xffff;
	R.IY.w = 0xffff;
	R.PC.w = 0x0000;
	R.SP.w = 0xffff;
	R.AF2.w = 0xffff;
	R.BC2.w = 0xffff;
	R.DE2.w = 0xffff;
	R.HL2.w = 0xffff;
	R.nextIFF1 = false;
	R.IFF1 = false;
	R.IFF2 = false;
	R.HALT = false;
	R.IM = 0;
	R.I = 0xff;
	R.R = 0x00;
	R.R2 = 0;

	setCurrentTime(time);
}

