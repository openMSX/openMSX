// $Id$

#include <sstream>
#include "CPU.hh"
#include "CPUInterface.hh"
#include "Scheduler.hh"
#include "CliCommOutput.hh"
#include "Event.hh"
#include "EventDistributor.hh"

#ifdef CPU_DEBUG
#include "BooleanSetting.hh"
#endif

using std::ostringstream;


namespace openmsx {

#ifdef CPU_DEBUG
BooleanSetting CPU::traceSetting("cputrace", "CPU tracing on/off", false);
#endif

multiset<word> CPU::breakPoints;
bool CPU::breaked = false;
bool CPU::continued = false;
bool CPU::step = false;


CPU::CPU(const string& name, int defaultFreq)
	: interface(NULL),
	  freqLocked(name + "_freq_locked",
	             "real (locked) or custom (unlocked) " + name + " frequency",
	             true),
	  freqValue(name + "_freq",
	            "custom " + name + " frequency (only valid when unlocked)",
	            defaultFreq, 1, 100000000),
	  freq(defaultFreq)
{
	currentTime.setFreq(defaultFreq);

	freqLocked.addListener(this);
	freqValue.addListener(this);
}

CPU::~CPU()
{
	freqValue.removeListener(this);
	freqLocked.removeListener(this);
}

void CPU::init(Scheduler* scheduler_)
{
	scheduler = scheduler_;
}

void CPU::setInterface(CPUInterface* interf)
{
	interface = interf;
}

void CPU::executeUntilTarget(const EmuTime& time)
{
	assert(interface);
	setTargetTime(time);
	executeCore();
}

void CPU::setTargetTime(const EmuTime& time)
{
	targetTime = time;
}

const EmuTime &CPU::getTargetTime() const
{
	return targetTime;
}

void CPU::setCurrentTime(const EmuTime& time)
{
	currentTime = time;
}
const EmuTime& CPU::getCurrentTime() const
{
	return currentTime;
}


void CPU::invalidateCache(word start, int num)
{
	//PRT_DEBUG("cache: invalidate "<<start<<" "<<num*CACHE_LINE_SIZE);
	memset(&readCacheLine  [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //	NULL
	memset(&writeCacheLine [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //
	memset(&readCacheTried [start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //	FALSE
	memset(&writeCacheTried[start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //
}


void CPU::reset(const EmuTime& time)
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
	memptr.w = 0xFFFF;
	invalidateCache(0x0000, 0x10000 / CPU::CACHE_LINE_SIZE);
	setCurrentTime(time);
	
	assert(IRQStatus == 0); // other devices must reset their IRQ source
}

void CPU::raiseIRQ()
{
	assert(IRQStatus >= 0);
	if (IRQStatus == 0)
		slowInstructions++;
	IRQStatus++;
	//PRT_DEBUG("CPU: raise IRQ " << IRQStatus);
}
void CPU::lowerIRQ()
{
	IRQStatus--;
	//PRT_DEBUG("CPU: lower IRQ " << IRQStatus);
	assert(IRQStatus >= 0);
}

void CPU::wait(const EmuTime& time)
{
	assert(time >= getCurrentTime());
	setCurrentTime(time);
	if (getTargetTime() <= time) {
		extendTarget(time);
	}
}

void CPU::extendTarget(const EmuTime& time)
{
	assert(getTargetTime() <= time);
	EmuTime now = getTargetTime();
	setTargetTime(time);
	scheduler->schedule(now, time);
}


void CPU::doBreak2()
{
	assert(!breaked);
	breaked = true;

	scheduler->increasePauseCounter();
	
	ostringstream os;
	os << "0x" << hex << (int)R.PC.w;
	CliCommOutput::instance().update(CliCommOutput::BREAK, "pc", os.str());
	static SimpleEvent<BREAK_EVENT> breakEvent;
	EventDistributor::instance().distributeEvent(breakEvent);
}

void CPU::doStep()
{
	if (breaked) {
		breaked = false;
		step = true;
		scheduler->decreasePauseCounter();
	}
}

void CPU::doContinue()
{
	if (breaked) {
		breaked = false;
		continued = true;
		scheduler->decreasePauseCounter();
	}
}

void CPU::doBreak()
{
	if (!breaked) {
		step = true;
	}
}


void CPU::update(const SettingLeafNode* setting) throw()
{
	if (setting == &freqLocked) {
		if (freqLocked.getValue()) {
			// locked
			currentTime.setFreq(freq);
		} else {
			// unlocked
			currentTime.setFreq(freqValue.getValue());
		}
	} else if (setting == &freqValue) {
		if (!freqLocked.getValue()) {
			currentTime.setFreq(freqValue.getValue());
		}
	} else {
		assert(false);
	}
}

void CPU::setFreq(unsigned freq_)
{
	freq = freq_;
	if (freqLocked.getValue()) {
		// locked
		currentTime.setFreq(freq);
	}
}

} // namespace openmsx
