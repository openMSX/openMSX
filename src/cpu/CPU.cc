// $Id$

#include "CPUInterface.hh"
#include "CPU.hh"
#include "Scheduler.hh"

#ifdef CPU_DEBUG
#include "Settings.hh"
#endif


namespace openmsx {

#ifdef CPU_DEBUG
BooleanSetting *CPU::traceSetting =
	new BooleanSetting("cputrace", "CPU tracing on/off", false);
#endif

CPU::CPU()
	: interface(NULL)
{
}

CPU::~CPU()
{
}

void CPU::init(Scheduler* scheduler_)
{
	scheduler = scheduler_;
}

void CPU::setInterface(CPUInterface* interf)
{
	interface = interf;
}

void CPU::executeUntilTarget(const EmuTime &time)
{
	assert(interface);
	setTargetTime(time);
	executeCore();
}

void CPU::setTargetTime(const EmuTime &time)
{
	targetTime = time;
}

const EmuTime &CPU::getTargetTime() const
{
	return targetTime;
}

void CPU::invalidateCache(word start, int num)
{
	//PRT_DEBUG("cache: invalidate "<<start<<" "<<num*CACHE_LINE_SIZE);
	memset(&readCacheLine  [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //	NULL
	memset(&writeCacheLine [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //
	memset(&readCacheTried [start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //	FALSE
	memset(&writeCacheTried[start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //
}


void CPU::reset(const EmuTime &time)
{
	// AF and SP are 0xFFFF
	// PC, R, IFF1, IFF2, HALT and IM are 0x0
	// all others are random
	R.AF.w  = 0xFFFF;
	R.BC.w  = 0xFFFF;
	R.DE.w  = 0xFFFF;
	R.HL.w  = 0xFFFF;
	R.IX.w  = 0xFFFF;
	R.IY.w  = 0xFFFF;
	R.PC.w  = 0x0000;
	R.SP.w  = 0xFFFF;
	R.AF2.w = 0xFFFF;
	R.BC2.w = 0xFFFF;
	R.DE2.w = 0xFFFF;
	R.HL2.w = 0xFFFF;
	R.nextIFF1 = false;
	R.IFF1     = false;
	R.IFF2     = false;
	R.HALT     = false;
	R.IM = 0;
	R.I  = 0xFF;
	R.R  = 0x00;
	R.R2 = 0;
	IRQStatus = 0;
	memptr.w = 0xFFFF;
	invalidateCache(0x0000, 0x10000 / CPU::CACHE_LINE_SIZE);
	setCurrentTime(time);
}

void CPU::raiseIRQ()
{
	if (IRQStatus == 0)
		slowInstructions++;
	IRQStatus++;
	//PRT_DEBUG("CPU: raise IRQ " << IRQStatus);
}
void CPU::lowerIRQ()
{
	IRQStatus--;
	//PRT_DEBUG("CPU: lower IRQ " << IRQStatus);
}

void CPU::wait(const EmuTime& time)
{
	assert(time >= getCurrentTime());
	if (getTargetTime() <= time) {
		extendTarget(time);
	}
	setCurrentTime(time);
}

void CPU::extendTarget(const EmuTime& time)
{
	assert(getTargetTime() <= time);
	scheduler->schedule(time);
	setTargetTime(time);
}

} // namespace openmsx
