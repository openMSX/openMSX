// $Id$

#include <sstream>
#include "CPU.hh"
#include "MSXCPUInterface.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "CliCommOutput.hh"
#include "Event.hh"
#include "EventDistributor.hh"

using std::ostringstream;

namespace openmsx {

multiset<word> CPU::breakPoints;
bool CPU::breaked = false;
bool CPU::continued = false;
bool CPU::step = false;


CPU::CPU(const string& name, int defaultFreq,
	 const BooleanSetting& traceSetting_)
	: interface(NULL)
	, scheduler(Scheduler::instance())
	, freqLocked(name + "_freq_locked",
	             "real (locked) or custom (unlocked) " + name + " frequency",
	             true)
	, freqValue(name + "_freq",
	            "custom " + name + " frequency (only valid when unlocked)",
	            defaultFreq, 1000000, 100000000)
	, freq(defaultFreq)
	, traceSetting(traceSetting_)
{
	if (freqLocked.getValue()) {
		// locked
		clock.setFreq(defaultFreq);
	} else {
		// unlocked
		clock.setFreq(freqValue.getValue());
	}

	freqLocked.addListener(this);
	freqValue.addListener(this);
}

CPU::~CPU()
{
	freqValue.removeListener(this);
	freqLocked.removeListener(this);
}

void CPU::setMotherboard(MSXMotherBoard* motherboard_)
{
	motherboard = motherboard_;
}

void CPU::setInterface(MSXCPUInterface* interf)
{
	interface = interf;
}

void CPU::advance(const EmuTime& time)
{
	clock.advance(time);
}
const EmuTime& CPU::getCurrentTime() const
{
	return clock.getTime();
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
	R.I  = 0x00;
	R.R  = 0x00;
	R.R2 = 0;
	memptr.w = 0xFFFF;
	invalidateCache(0x0000, 0x10000 / CPU::CACHE_LINE_SIZE);
	clock.reset(time);
	
	assert(IRQStatus == 0); // other devices must reset their IRQ source
}

void CPU::exitCPULoop()
{
	exitLoop = true;
	++slowInstructions;
}

void CPU::raiseIRQ()
{
	assert(IRQStatus >= 0);
	if (IRQStatus == 0) {
		slowInstructions++;
	}
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
	scheduler.schedule(time);
	advance(time);
}

void CPU::doBreak2()
{
	assert(!breaked);
	breaked = true;

	motherboard->block();
	scheduler.setCurrentTime(clock.getTime());

	ostringstream os;
	os << "0x" << hex << (int)R.PC.w;
	CliCommOutput::instance().update(CliCommOutput::BREAK, "pc", os.str());
	Event* breakEvent = new SimpleEvent<BREAK_EVENT>();
	EventDistributor::instance().distributeEvent(breakEvent);
}

void CPU::doStep()
{
	if (breaked) {
		breaked = false;
		step = true;
		motherboard->unblock();
	}
}

void CPU::doContinue()
{
	if (breaked) {
		breaked = false;
		continued = true;
		motherboard->unblock();
	}
}

void CPU::doBreak()
{
	if (!breaked) {
		step = true;
	}
}


void CPU::update(const SettingLeafNode* setting)
{
	if (setting == &freqLocked) {
		if (freqLocked.getValue()) {
			// locked
			clock.setFreq(freq);
		} else {
			// unlocked
			clock.setFreq(freqValue.getValue());
		}
	} else if (setting == &freqValue) {
		if (!freqLocked.getValue()) {
			clock.setFreq(freqValue.getValue());
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
		clock.setFreq(freq);
	}
}

} // namespace openmsx
