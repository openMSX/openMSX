// $Id$

#include <sstream>
#include <iomanip>
#include <cassert>
#include "MSXCPUInterface.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "CliCommOutput.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "BooleanSetting.hh"
#include "Dasm.hh"
#include "CPUCore.hh"
#include "Z80.hh"
#include "R800.hh"

using std::string;

namespace openmsx {

template <class T> CPUCore<T>::CPUCore(
	const string& name, const BooleanSetting& traceSetting_,
	const EmuTime& time)
	: T(time)
	, nmiEdge(false)
	, NMIStatus(0)
	, IRQStatus(0)
	, interface(NULL)
	, scheduler(Scheduler::instance())
	, freqLocked(name + "_freq_locked",
	             "real (locked) or custom (unlocked) " + name + " frequency",
	             true)
	, freqValue(name + "_freq",
	            "custom " + name + " frequency (only valid when unlocked)",
	            T::CLOCK_FREQ, 1000000, 100000000)
	, freq(T::CLOCK_FREQ)
	, traceSetting(traceSetting_)
{
	if (freqLocked.getValue()) {
		// locked
		T::clock.setFreq(T::CLOCK_FREQ);
	} else {
		// unlocked
		T::clock.setFreq(freqValue.getValue());
	}

	freqLocked.addListener(this);
	freqValue.addListener(this);
	reset(time);
}

template <class T> CPUCore<T>::~CPUCore()
{
	freqValue.removeListener(this);
	freqLocked.removeListener(this);
}

template <class T> void CPUCore<T>::setMotherboard(MSXMotherBoard* motherboard_)
{
	motherboard = motherboard_;
}

template <class T> void CPUCore<T>::setInterface(MSXCPUInterface* interf)
{
	interface = interf;
}

template <class T> void CPUCore<T>::advance(const EmuTime& time)
{
	T::clock.advance(time);
}

template <class T> const EmuTime& CPUCore<T>::getCurrentTime() const
{
	return T::clock.getTime();
}

template <class T> void CPUCore<T>::invalidateCache(word start, int num)
{
	//PRT_DEBUG("cache: invalidate "<<start<<" "<<num*CACHE_LINE_SIZE);
	memset(&readCacheLine  [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //	NULL
	memset(&writeCacheLine [start>>CACHE_LINE_BITS], 0, num*sizeof(byte*)); //
	memset(&readCacheTried [start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //	FALSE
	memset(&writeCacheTried[start>>CACHE_LINE_BITS], 0, num*sizeof(bool)); //
}

template <class T> CPU::CPURegs& CPUCore<T>::getRegisters()
{
	return R;
}

template <class T> void CPUCore<T>::reset(const EmuTime& time)
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
	invalidateCache(0x0000, 0x10000 / CACHE_LINE_SIZE);
	T::clock.reset(time);
	
	assert(NMIStatus == 0); // other devices must reset their NMI source
	assert(IRQStatus == 0); // other devices must reset their IRQ source
}

template <class T> void CPUCore<T>::exitCPULoop()
{
	exitLoop = true;
	++slowInstructions;
}

template <class T> void CPUCore<T>::raiseIRQ()
{
	assert(IRQStatus >= 0);
	if (IRQStatus == 0) {
		slowInstructions++;
	}
	IRQStatus++;
}

template <class T> void CPUCore<T>::lowerIRQ()
{
	IRQStatus--;
	assert(IRQStatus >= 0);
}

template <class T> void CPUCore<T>::raiseNMI()
{
	assert(NMIStatus >= 0);
	if (NMIStatus == 0) {
		nmiEdge = true;
		slowInstructions++;
	}
	NMIStatus++;
}

template <class T> void CPUCore<T>::lowerNMI()
{
	NMIStatus--;
	assert(NMIStatus >= 0);
}

template <class T> void CPUCore<T>::wait(const EmuTime& time)
{
	assert(time >= getCurrentTime());
	scheduler.schedule(time);
	advance(time);
}

template <class T> void CPUCore<T>::doBreak2()
{
	assert(!breaked);
	breaked = true;

	motherboard->block();
	scheduler.setCurrentTime(T::clock.getTime());

	std::ostringstream os;
	os << "0x" << std::hex << (int)R.PC.w;
	CliCommOutput::instance().update(CliCommOutput::BREAK, "pc", os.str());
	Event* breakEvent = new SimpleEvent<BREAK_EVENT>();
	EventDistributor::instance().distributeEvent(breakEvent);
}

template <class T> void CPUCore<T>::doStep()
{
	if (breaked) {
		breaked = false;
		step = true;
		motherboard->unblock();
	}
}

template <class T> void CPUCore<T>::doContinue()
{
	if (breaked) {
		breaked = false;
		continued = true;
		motherboard->unblock();
	}
}

template <class T> void CPUCore<T>::doBreak()
{
	if (!breaked) {
		step = true;
	}
}

template <class T> void CPUCore<T>::update(const Setting* setting)
{
	if (setting == &freqLocked) {
		if (freqLocked.getValue()) {
			// locked
			T::clock.setFreq(freq);
		} else {
			// unlocked
			T::clock.setFreq(freqValue.getValue());
		}
	} else if (setting == &freqValue) {
		if (!freqLocked.getValue()) {
			T::clock.setFreq(freqValue.getValue());
		}
	} else {
		assert(false);
	}
}

template <class T> void CPUCore<T>::setFreq(unsigned freq_)
{
	freq = freq_;
	if (freqLocked.getValue()) {
		// locked
		T::clock.setFreq(freq);
	}
}


template <class T> inline byte CPUCore<T>::READ_PORT(word port)
{
	memptr.w = port + 1;
	T::clock += T::IO_DELAY1;
	scheduler.schedule(T::clock.getTime());
	byte result = interface->readIO(port, T::clock.getTime());
	T::clock += T::IO_DELAY2;
	return result;
}

template <class T> inline void CPUCore<T>::WRITE_PORT(word port, byte value)
{
	memptr.w = port + 1;
	T::clock += T::IO_DELAY1;
	scheduler.schedule(T::clock.getTime());
	interface->writeIO(port, value, T::clock.getTime());
	T::clock += T::IO_DELAY2;
}

template <class T> inline byte CPUCore<T>::RDMEM_common(word address)
{
	int line = address >> CACHE_LINE_BITS;
	if (readCacheLine[line] != NULL) {
		// cached, fast path
		T::clock += (T::MEM_DELAY1 + T::MEM_DELAY2);
		byte result = readCacheLine[line][address&CACHE_LINE_LOW];
		debugmemory[address] = result;
		return result;
	} else {
		return RDMEMslow(address);	// not inlined
	}
}
template <class T> byte CPUCore<T>::RDMEMslow(word address)
{
	// not cached
	int line = address >> CACHE_LINE_BITS;
	if (!readCacheTried[line]) {
		// try to cache now
		readCacheTried[line] = true;
		readCacheLine[line] = interface->getReadCacheLine(address&CACHE_LINE_HIGH);
		if (readCacheLine[line] != NULL) {
			// cached ok
			T::clock += (T::MEM_DELAY1 + T::MEM_DELAY2);
			byte result = readCacheLine[line][address&CACHE_LINE_LOW];
			debugmemory[address] = result;
			return result;
		}
	}
	// uncacheable
	T::clock += T::MEM_DELAY1;
	scheduler.schedule(T::clock.getTime());
	byte result = interface->readMem(address, T::clock.getTime());
	debugmemory[address] = result;
	T::clock += T::MEM_DELAY2;
	return result;
}

template <class T> inline void CPUCore<T>::WRMEM_common(word address, byte value)
{
	int line = address >> CACHE_LINE_BITS;
	if (writeCacheLine[line] != NULL) {
		// cached, fast path
		T::clock += (T::MEM_DELAY1 + T::MEM_DELAY2);
		writeCacheLine[line][address&CACHE_LINE_LOW] = value;
		return;
	}
	WRMEMslow(address, value);	// not inlined
}
template <class T> void CPUCore<T>::WRMEMslow(word address, byte value)
{
	// not cached
	int line = address >> CACHE_LINE_BITS;
	if (!writeCacheTried[line]) {
		// try to cache now
		writeCacheTried[line] = true;
		writeCacheLine[line] =
			interface->getWriteCacheLine(address&CACHE_LINE_HIGH);
		if (writeCacheLine[line] != NULL) {
			// cached ok
			T::clock += (T::MEM_DELAY1 + T::MEM_DELAY2);
			writeCacheLine[line][address&CACHE_LINE_LOW] = value;
			return;
		}
	}
	// uncacheable
	T::clock += T::MEM_DELAY1;
	scheduler.schedule(T::clock.getTime());
	interface->writeMem(address, value, T::clock.getTime());
	T::clock += T::MEM_DELAY2;
}

template <class T> inline byte CPUCore<T>::RDMEM_OPCODE(word address)
{
	T::PRE_RDMEM_OPCODE(address);
	return RDMEM_common(address);
}

template <class T> inline byte CPUCore<T>::RDMEM(word address)
{
	T::PRE_RDMEM(address);
	return RDMEM_common(address);
}

template <class T> inline void CPUCore<T>::WRMEM(word address, byte value)
{
	T::PRE_WRMEM(address);
	WRMEM_common(address, value);
}

template <class T> inline void CPUCore<T>::M1Cycle() { R.R++; T::M1_DELAY(); }

// NMI interrupt
template <class T> inline void CPUCore<T>::nmi()
{
	T::SMALL_DELAY(); R.SP.w--;
	WRMEM(R.SP.w, R.PC.B.h);
	R.SP.w--;
	WRMEM(R.SP.w, R.PC.B.l);
	R.HALT = false;
	R.IFF1 = R.nextIFF1 = false;
	R.PC.w = 0x0066;
	M1Cycle();
	T::NMI_DELAY();
}

// IM0 interrupt
template <class T> inline void CPUCore<T>::irq0()
{
	// TODO current implementation only works for 1-byte instructions
	//      ok for MSX
	R.HALT = false;
	R.IFF1 = R.nextIFF1 = R.IFF2 = false;
	T::IM0_DELAY();
	executeInstruction1(interface->dataBus());
}

// IM1 interrupt
template <class T> inline void CPUCore<T>::irq1()
{
	R.HALT = false;
	R.IFF1 = R.nextIFF1 = R.IFF2 = false;
	T::IM1_DELAY();
	executeInstruction1(0xFF);	// RST 0x38
}

// IM2 interrupt
template <class T> inline void CPUCore<T>::irq2()
{
	R.HALT = false;
	R.IFF1 = R.nextIFF1 = R.IFF2 = false;
	T::SMALL_DELAY(); R.SP.w--;
	WRMEM(R.SP.w, R.PC.B.h);
	R.SP.w--;
	WRMEM(R.SP.w, R.PC.B.l);
	word x = interface->dataBus() | (R.I << 8);
	R.PC.B.l = RDMEM(x++);
	R.PC.B.h = RDMEM(x);
	M1Cycle();
	T::IM2_DELAY();
}

template <class T> inline void CPUCore<T>::executeInstruction1(byte opcode)
{
	M1Cycle();
	(this->*opcode_main[opcode])();
}

template <class T> inline void CPUCore<T>::executeInstruction()
{
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	executeInstruction1(opcode);
}

template <class T> inline void CPUCore<T>::cpuTracePre()
{
	start_pc = R.PC.w;
}
template <class T> inline void CPUCore<T>::cpuTracePost()
{
	if (traceSetting.getValue()) {
		string dasmOutput;
		dasm(&debugmemory[start_pc], start_pc, dasmOutput);
		std::cout << std::setfill('0') << std::hex << std::setw(4) << start_pc
		     << " : " << dasmOutput
		     << " AF=" << std::setw(4) << R.AF.w
		     << " BC=" << std::setw(4) << R.BC.w
		     << " DE=" << std::setw(4) << R.DE.w
		     << " HL=" << std::setw(4) << R.HL.w
		     << " IX=" << std::setw(4) << R.IX.w
		     << " IY=" << std::setw(4) << R.IY.w
		     << " SP=" << std::setw(4) << R.SP.w
		     << std::endl << std::dec;
	}
}

template <class T> inline void CPUCore<T>::executeFast()
{
	T::R800Refresh();
	executeInstruction();
}

template <class T> void CPUCore<T>::executeSlow()
{
	if (nmiEdge) {
		nmiEdge = false;
		nmi();	// NMI occured
	} else if (R.IFF1 && IRQStatus) {
		// normal interrupt
		switch (R.IM) {
			case 0: irq0();
				break;
			case 1: irq1();
				break;
			case 2: irq2();
				break;
			default:
				assert(false);
		}
	} else if (R.HALT) {
		// in halt mode
		uint64 ticks = T::clock.getTicksTillUp(scheduler.getNext());
		int hltStates = T::haltStates();
		int halts = (ticks + hltStates - 1) / hltStates;	// rounded up
		R.R += halts;
		T::clock += halts * hltStates;
		slowInstructions = 2;
	} else {
		R.IFF1 = R.nextIFF1;
		executeFast();
	}
}

template <class T> void CPUCore<T>::execute()
{
	assert(!breaked);

	exitLoop = false;
	slowInstructions = 2;

	if (continued || step) {
		// at least one instruction
		continued = false;
		scheduler.schedule(T::clock.getTime());
		cpuTracePre();
		executeSlow();
		cpuTracePost();
		--slowInstructions;
		if (step) {
			step = false;
			doBreak2();
			return;
		}
	}

	if (breakPoints.empty() && !traceSetting.getValue()) {
		// fast path, no breakpoints, no tracing
		while (!exitLoop) {
			if (slowInstructions) {
				slowInstructions--;
				scheduler.schedule(T::clock.getTime());
				executeSlow();
			} else {
				while (!slowInstructions) {
					scheduler.schedule(T::clock.getTime());
					executeFast();
				}
			}
		}
	} else {
		while (!exitLoop) {
			if (breakPoints.find(R.PC.w) != breakPoints.end()) {
				doBreak2();
				return;
			} else {
				cpuTracePre();
				if (slowInstructions == 0) {
					scheduler.schedule(T::clock.getTime());
					executeFast();
				} else {
					slowInstructions--;
					scheduler.schedule(T::clock.getTime());
					executeSlow();
				}
				cpuTracePost();
			}
		}
	}
}


// conditions
template <class T> inline bool CPUCore<T>::C()  { return R.AF.B.l & C_FLAG; }
template <class T> inline bool CPUCore<T>::NC() { return !C(); }
template <class T> inline bool CPUCore<T>::Z()  { return R.AF.B.l & Z_FLAG; }
template <class T> inline bool CPUCore<T>::NZ() { return !Z(); }
template <class T> inline bool CPUCore<T>::M()  { return R.AF.B.l & S_FLAG; }
template <class T> inline bool CPUCore<T>::P()  { return !M(); }
template <class T> inline bool CPUCore<T>::PE() { return R.AF.B.l & V_FLAG; }
template <class T> inline bool CPUCore<T>::PO() { return !PE(); }


// LD r,r
template <class T> void CPUCore<T>::ld_a_a()     { }
template <class T> void CPUCore<T>::ld_a_b()     { R.AF.B.h = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_a_c()     { R.AF.B.h = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_a_d()     { R.AF.B.h = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_a_e()     { R.AF.B.h = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_a_h()     { R.AF.B.h = R.HL.B.h; }
template <class T> void CPUCore<T>::ld_a_l()     { R.AF.B.h = R.HL.B.l; }
template <class T> void CPUCore<T>::ld_a_ixh()   { R.AF.B.h = R.IX.B.h; }
template <class T> void CPUCore<T>::ld_a_ixl()   { R.AF.B.h = R.IX.B.l; }
template <class T> void CPUCore<T>::ld_a_iyh()   { R.AF.B.h = R.IY.B.h; }
template <class T> void CPUCore<T>::ld_a_iyl()   { R.AF.B.h = R.IY.B.l; }
template <class T> void CPUCore<T>::ld_b_b()     { }
template <class T> void CPUCore<T>::ld_b_a()     { R.BC.B.h = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_b_c()     { R.BC.B.h = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_b_d()     { R.BC.B.h = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_b_e()     { R.BC.B.h = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_b_h()     { R.BC.B.h = R.HL.B.h; }
template <class T> void CPUCore<T>::ld_b_l()     { R.BC.B.h = R.HL.B.l; }
template <class T> void CPUCore<T>::ld_b_ixh()   { R.BC.B.h = R.IX.B.h; }
template <class T> void CPUCore<T>::ld_b_ixl()   { R.BC.B.h = R.IX.B.l; }
template <class T> void CPUCore<T>::ld_b_iyh()   { R.BC.B.h = R.IY.B.h; }
template <class T> void CPUCore<T>::ld_b_iyl()   { R.BC.B.h = R.IY.B.l; }
template <class T> void CPUCore<T>::ld_c_c()     { }
template <class T> void CPUCore<T>::ld_c_a()     { R.BC.B.l = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_c_b()     { R.BC.B.l = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_c_d()     { R.BC.B.l = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_c_e()     { R.BC.B.l = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_c_h()     { R.BC.B.l = R.HL.B.h; }
template <class T> void CPUCore<T>::ld_c_l()     { R.BC.B.l = R.HL.B.l; }
template <class T> void CPUCore<T>::ld_c_ixh()   { R.BC.B.l = R.IX.B.h; }
template <class T> void CPUCore<T>::ld_c_ixl()   { R.BC.B.l = R.IX.B.l; }
template <class T> void CPUCore<T>::ld_c_iyh()   { R.BC.B.l = R.IY.B.h; }
template <class T> void CPUCore<T>::ld_c_iyl()   { R.BC.B.l = R.IY.B.l; }
template <class T> void CPUCore<T>::ld_d_d()     { }
template <class T> void CPUCore<T>::ld_d_a()     { R.DE.B.h = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_d_c()     { R.DE.B.h = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_d_b()     { R.DE.B.h = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_d_e()     { R.DE.B.h = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_d_h()     { R.DE.B.h = R.HL.B.h; }
template <class T> void CPUCore<T>::ld_d_l()     { R.DE.B.h = R.HL.B.l; }
template <class T> void CPUCore<T>::ld_d_ixh()   { R.DE.B.h = R.IX.B.h; }
template <class T> void CPUCore<T>::ld_d_ixl()   { R.DE.B.h = R.IX.B.l; }
template <class T> void CPUCore<T>::ld_d_iyh()   { R.DE.B.h = R.IY.B.h; }
template <class T> void CPUCore<T>::ld_d_iyl()   { R.DE.B.h = R.IY.B.l; }
template <class T> void CPUCore<T>::ld_e_e()     { }
template <class T> void CPUCore<T>::ld_e_a()     { R.DE.B.l = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_e_c()     { R.DE.B.l = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_e_b()     { R.DE.B.l = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_e_d()     { R.DE.B.l = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_e_h()     { R.DE.B.l = R.HL.B.h; }
template <class T> void CPUCore<T>::ld_e_l()     { R.DE.B.l = R.HL.B.l; }
template <class T> void CPUCore<T>::ld_e_ixh()   { R.DE.B.l = R.IX.B.h; }
template <class T> void CPUCore<T>::ld_e_ixl()   { R.DE.B.l = R.IX.B.l; }
template <class T> void CPUCore<T>::ld_e_iyh()   { R.DE.B.l = R.IY.B.h; }
template <class T> void CPUCore<T>::ld_e_iyl()   { R.DE.B.l = R.IY.B.l; }
template <class T> void CPUCore<T>::ld_h_h()     { }
template <class T> void CPUCore<T>::ld_h_a()     { R.HL.B.h = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_h_c()     { R.HL.B.h = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_h_b()     { R.HL.B.h = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_h_e()     { R.HL.B.h = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_h_d()     { R.HL.B.h = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_h_l()     { R.HL.B.h = R.HL.B.l; }
template <class T> void CPUCore<T>::ld_l_l()     { }
template <class T> void CPUCore<T>::ld_l_a()     { R.HL.B.l = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_l_c()     { R.HL.B.l = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_l_b()     { R.HL.B.l = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_l_e()     { R.HL.B.l = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_l_d()     { R.HL.B.l = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_l_h()     { R.HL.B.l = R.HL.B.h; }
template <class T> void CPUCore<T>::ld_ixh_a()   { R.IX.B.h = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_ixh_b()   { R.IX.B.h = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_ixh_c()   { R.IX.B.h = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_ixh_d()   { R.IX.B.h = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_ixh_e()   { R.IX.B.h = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_ixh_ixh() { }
template <class T> void CPUCore<T>::ld_ixh_ixl() { R.IX.B.h = R.IX.B.l; }
template <class T> void CPUCore<T>::ld_ixl_a()   { R.IX.B.l = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_ixl_b()   { R.IX.B.l = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_ixl_c()   { R.IX.B.l = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_ixl_d()   { R.IX.B.l = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_ixl_e()   { R.IX.B.l = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_ixl_ixh() { R.IX.B.l = R.IX.B.h; }
template <class T> void CPUCore<T>::ld_ixl_ixl() { }
template <class T> void CPUCore<T>::ld_iyh_a()   { R.IY.B.h = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_iyh_b()   { R.IY.B.h = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_iyh_c()   { R.IY.B.h = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_iyh_d()   { R.IY.B.h = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_iyh_e()   { R.IY.B.h = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_iyh_iyh() { }
template <class T> void CPUCore<T>::ld_iyh_iyl() { R.IY.B.h = R.IY.B.l; }
template <class T> void CPUCore<T>::ld_iyl_a()   { R.IY.B.l = R.AF.B.h; }
template <class T> void CPUCore<T>::ld_iyl_b()   { R.IY.B.l = R.BC.B.h; }
template <class T> void CPUCore<T>::ld_iyl_c()   { R.IY.B.l = R.BC.B.l; }
template <class T> void CPUCore<T>::ld_iyl_d()   { R.IY.B.l = R.DE.B.h; }
template <class T> void CPUCore<T>::ld_iyl_e()   { R.IY.B.l = R.DE.B.l; }
template <class T> void CPUCore<T>::ld_iyl_iyh() { R.IY.B.l = R.IY.B.h; }
template <class T> void CPUCore<T>::ld_iyl_iyl() { }

// LD SP,ss
template <class T> void CPUCore<T>::ld_sp_hl()   { R.SP.w = R.HL.w; }
template <class T> void CPUCore<T>::ld_sp_ix()   { R.SP.w = R.IX.w; }
template <class T> void CPUCore<T>::ld_sp_iy()   { R.SP.w = R.IY.w; }

// LD (ss),a
template <class T> inline void CPUCore<T>::WR_X_A(word x)
{
	WRMEM(x, R.AF.B.h);
}
template <class T> void CPUCore<T>::ld_xbc_a() { WR_X_A(R.BC.w); }
template <class T> void CPUCore<T>::ld_xde_a() { WR_X_A(R.DE.w); }
template <class T> void CPUCore<T>::ld_xhl_a() { WR_X_A(R.HL.w); }

// LD (HL),r
template <class T> inline void CPUCore<T>::WR_HL_X(byte val)
{
	WRMEM(R.HL.w, val);
}
template <class T> void CPUCore<T>::ld_xhl_b() { WR_HL_X(R.BC.B.h); }
template <class T> void CPUCore<T>::ld_xhl_c() { WR_HL_X(R.BC.B.l); }
template <class T> void CPUCore<T>::ld_xhl_d() { WR_HL_X(R.DE.B.h); }
template <class T> void CPUCore<T>::ld_xhl_e() { WR_HL_X(R.DE.B.l); }
template <class T> void CPUCore<T>::ld_xhl_h() { WR_HL_X(R.HL.B.h); }
template <class T> void CPUCore<T>::ld_xhl_l() { WR_HL_X(R.HL.B.l); }

// LD (HL),n
template <class T> void CPUCore<T>::ld_xhl_byte()
{ 
	byte val = RDMEM_OPCODE(R.PC.w++);
	WR_HL_X(val);
}

// LD (IX+e),r
template <class T> inline void CPUCore<T>::WR_X_X(byte val)
{
	WRMEM(memptr.w, val);
}
template <class T> inline void CPUCore<T>::WR_XIX(byte val)
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst; T::ADD_16_8_DELAY(); WR_X_X(val);
}
template <class T> void CPUCore<T>::ld_xix_a() { WR_XIX(R.AF.B.h); }
template <class T> void CPUCore<T>::ld_xix_b() { WR_XIX(R.BC.B.h); }
template <class T> void CPUCore<T>::ld_xix_c() { WR_XIX(R.BC.B.l); }
template <class T> void CPUCore<T>::ld_xix_d() { WR_XIX(R.DE.B.h); }
template <class T> void CPUCore<T>::ld_xix_e() { WR_XIX(R.DE.B.l); }
template <class T> void CPUCore<T>::ld_xix_h() { WR_XIX(R.HL.B.h); }
template <class T> void CPUCore<T>::ld_xix_l() { WR_XIX(R.HL.B.l); }

// LD (IY+e),r
template <class T> inline void CPUCore<T>::WR_XIY(byte val)
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst; T::ADD_16_8_DELAY(); WR_X_X(val);
}
template <class T> void CPUCore<T>::ld_xiy_a() { WR_XIY(R.AF.B.h); }
template <class T> void CPUCore<T>::ld_xiy_b() { WR_XIY(R.BC.B.h); }
template <class T> void CPUCore<T>::ld_xiy_c() { WR_XIY(R.BC.B.l); }
template <class T> void CPUCore<T>::ld_xiy_d() { WR_XIY(R.DE.B.h); }
template <class T> void CPUCore<T>::ld_xiy_e() { WR_XIY(R.DE.B.l); }
template <class T> void CPUCore<T>::ld_xiy_h() { WR_XIY(R.HL.B.h); }
template <class T> void CPUCore<T>::ld_xiy_l() { WR_XIY(R.HL.B.l); }

// LD (IX+e),n
template <class T> void CPUCore<T>::ld_xix_byte()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	byte val = RDMEM_OPCODE(R.PC.w++);
	T::PARALLEL_DELAY(); WR_X_X(val);
}

// LD (IY+e),n
template <class T> void CPUCore<T>::ld_xiy_byte()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	byte val = RDMEM_OPCODE(R.PC.w++);
	T::PARALLEL_DELAY(); WR_X_X(val);
}

// LD (nn),A
template <class T> void CPUCore<T>::ld_xbyte_a()
{
	z80regpair x;
	x.B.l = RDMEM_OPCODE(R.PC.w++);
	x.B.h = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.AF.B.h << 8;
	WR_X_A(x.w);
}

// LD (nn),ss
template <class T> inline void CPUCore<T>::WR_NN_Y(z80regpair reg)
{
	memptr.B.l = RDMEM_OPCODE(R.PC.w++);
	memptr.B.h = RDMEM_OPCODE(R.PC.w++);
	WRMEM(memptr.w++, reg.B.l);
	WRMEM(memptr.w,   reg.B.h);
}
template <class T> void CPUCore<T>::ld_xword_bc() { WR_NN_Y(R.BC); }
template <class T> void CPUCore<T>::ld_xword_de() { WR_NN_Y(R.DE); }
template <class T> void CPUCore<T>::ld_xword_hl() { WR_NN_Y(R.HL); }
template <class T> void CPUCore<T>::ld_xword_ix() { WR_NN_Y(R.IX); }
template <class T> void CPUCore<T>::ld_xword_iy() { WR_NN_Y(R.IY); }
template <class T> void CPUCore<T>::ld_xword_sp() { WR_NN_Y(R.SP); }

// LD A,(ss)
template <class T> inline void CPUCore<T>::RD_A_X(word x)
{
	R.AF.B.h = RDMEM(x);
}
template <class T> void CPUCore<T>::ld_a_xbc() { RD_A_X(R.BC.w); }
template <class T> void CPUCore<T>::ld_a_xde() { RD_A_X(R.DE.w); }
template <class T> void CPUCore<T>::ld_a_xhl() { RD_A_X(R.HL.w); }

// LD A,(IX+e)
template <class T> void CPUCore<T>::ld_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst; T::ADD_16_8_DELAY(); RD_A_X(memptr.w);
}

// LD A,(IY+e)
template <class T> void CPUCore<T>::ld_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst; T::ADD_16_8_DELAY(); RD_A_X(memptr.w);
}

// LD A,(nn)
template <class T> void CPUCore<T>::ld_a_xbyte()
{
	z80regpair x;
	x.B.l = RDMEM_OPCODE(R.PC.w++);
	x.B.h = RDMEM_OPCODE(R.PC.w++);
	memptr.w = x.w + 1;
	RD_A_X(x.w);
}

// LD r,n
template <class T> inline void CPUCore<T>::RD_R_N(byte& reg)
{
	reg = RDMEM_OPCODE(R.PC.w++);
}
template <class T> void CPUCore<T>::ld_a_byte()   { RD_R_N(R.AF.B.h); }
template <class T> void CPUCore<T>::ld_b_byte()   { RD_R_N(R.BC.B.h); }
template <class T> void CPUCore<T>::ld_c_byte()   { RD_R_N(R.BC.B.l); }
template <class T> void CPUCore<T>::ld_d_byte()   { RD_R_N(R.DE.B.h); }
template <class T> void CPUCore<T>::ld_e_byte()   { RD_R_N(R.DE.B.l); }
template <class T> void CPUCore<T>::ld_h_byte()   { RD_R_N(R.HL.B.h); }
template <class T> void CPUCore<T>::ld_l_byte()   { RD_R_N(R.HL.B.l); }
template <class T> void CPUCore<T>::ld_ixh_byte() { RD_R_N(R.IX.B.h); }
template <class T> void CPUCore<T>::ld_ixl_byte() { RD_R_N(R.IX.B.l); }
template <class T> void CPUCore<T>::ld_iyh_byte() { RD_R_N(R.IY.B.h); }
template <class T> void CPUCore<T>::ld_iyl_byte() { RD_R_N(R.IY.B.l); }

// LD r,(hl)
template <class T> inline void CPUCore<T>::RD_R_HL(byte& reg)
{
	reg = RDMEM(R.HL.w);
}
template <class T> void CPUCore<T>::ld_b_xhl() { RD_R_HL(R.BC.B.h); }
template <class T> void CPUCore<T>::ld_c_xhl() { RD_R_HL(R.BC.B.l); }
template <class T> void CPUCore<T>::ld_d_xhl() { RD_R_HL(R.DE.B.h); }
template <class T> void CPUCore<T>::ld_e_xhl() { RD_R_HL(R.DE.B.l); }
template <class T> void CPUCore<T>::ld_h_xhl() { RD_R_HL(R.HL.B.h); }
template <class T> void CPUCore<T>::ld_l_xhl() { RD_R_HL(R.HL.B.l); }

// LD r,(IX+e)
template <class T> inline void CPUCore<T>::RD_R_X(byte& reg)
{
	reg = RDMEM(memptr.w);
}
template <class T> inline void CPUCore<T>::RD_R_XIX(byte& reg)
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst; T::ADD_16_8_DELAY(); RD_R_X(reg);
}
template <class T> void CPUCore<T>::ld_b_xix() { RD_R_XIX(R.BC.B.h); }
template <class T> void CPUCore<T>::ld_c_xix() { RD_R_XIX(R.BC.B.l); }
template <class T> void CPUCore<T>::ld_d_xix() { RD_R_XIX(R.DE.B.h); }
template <class T> void CPUCore<T>::ld_e_xix() { RD_R_XIX(R.DE.B.l); }
template <class T> void CPUCore<T>::ld_h_xix() { RD_R_XIX(R.HL.B.h); }
template <class T> void CPUCore<T>::ld_l_xix() { RD_R_XIX(R.HL.B.l); }

// LD r,(IY+e)
template <class T> inline void CPUCore<T>::RD_R_XIY(byte& reg)
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst; T::ADD_16_8_DELAY(); RD_R_X(reg);
}
template <class T> void CPUCore<T>::ld_b_xiy() { RD_R_XIY(R.BC.B.h); }
template <class T> void CPUCore<T>::ld_c_xiy() { RD_R_XIY(R.BC.B.l); }
template <class T> void CPUCore<T>::ld_d_xiy() { RD_R_XIY(R.DE.B.h); }
template <class T> void CPUCore<T>::ld_e_xiy() { RD_R_XIY(R.DE.B.l); }
template <class T> void CPUCore<T>::ld_h_xiy() { RD_R_XIY(R.HL.B.h); }
template <class T> void CPUCore<T>::ld_l_xiy() { RD_R_XIY(R.HL.B.l); }

// LD ss,(nn)
template <class T> inline void CPUCore<T>::RD_P_XX(z80regpair& reg)
{
	memptr.B.l = RDMEM_OPCODE(R.PC.w++);
	memptr.B.h = RDMEM_OPCODE(R.PC.w++);
	reg.B.l = RDMEM(memptr.w++);
	reg.B.h = RDMEM(memptr.w);
}
template <class T> void CPUCore<T>::ld_bc_xword() { RD_P_XX(R.BC); }
template <class T> void CPUCore<T>::ld_de_xword() { RD_P_XX(R.DE); }
template <class T> void CPUCore<T>::ld_hl_xword() { RD_P_XX(R.HL); }
template <class T> void CPUCore<T>::ld_ix_xword() { RD_P_XX(R.IX); }
template <class T> void CPUCore<T>::ld_iy_xword() { RD_P_XX(R.IY); }
template <class T> void CPUCore<T>::ld_sp_xword() { RD_P_XX(R.SP); }

// LD ss,nn
template <class T> inline void CPUCore<T>::RD_P_NN(z80regpair& reg)
{
	reg.B.l = RDMEM_OPCODE(R.PC.w++);
	reg.B.h = RDMEM_OPCODE(R.PC.w++);
}
template <class T> void CPUCore<T>::ld_bc_word() { RD_P_NN(R.BC); }
template <class T> void CPUCore<T>::ld_de_word() { RD_P_NN(R.DE); }
template <class T> void CPUCore<T>::ld_hl_word() { RD_P_NN(R.HL); }
template <class T> void CPUCore<T>::ld_ix_word() { RD_P_NN(R.IX); }
template <class T> void CPUCore<T>::ld_iy_word() { RD_P_NN(R.IY); }
template <class T> void CPUCore<T>::ld_sp_word() { RD_P_NN(R.SP); }


// ADC A,r
template <class T> inline void CPUCore<T>::ADC(byte reg)
{
	int res = R.AF.B.h + reg + ((R.AF.B.l & C_FLAG) ? 1 : 0);
	R.AF.B.l = ZSXYTable[res & 0xFF] |
	           ((res & 0x100) ? C_FLAG : 0) |
	           ((R.AF.B.h ^ res ^ reg) & H_FLAG) |
	           (((reg ^ R.AF.B.h ^ 0x80) & (reg ^ res) & 0x80) ? V_FLAG : 0);
	R.AF.B.h = res;
}
template <class T> void CPUCore<T>::adc_a_a()   { ADC(R.AF.B.h); }
template <class T> void CPUCore<T>::adc_a_b()   { ADC(R.BC.B.h); }
template <class T> void CPUCore<T>::adc_a_c()   { ADC(R.BC.B.l); }
template <class T> void CPUCore<T>::adc_a_d()   { ADC(R.DE.B.h); }
template <class T> void CPUCore<T>::adc_a_e()   { ADC(R.DE.B.l); }
template <class T> void CPUCore<T>::adc_a_h()   { ADC(R.HL.B.h); }
template <class T> void CPUCore<T>::adc_a_l()   { ADC(R.HL.B.l); }
template <class T> void CPUCore<T>::adc_a_ixl() { ADC(R.IX.B.l); }
template <class T> void CPUCore<T>::adc_a_ixh() { ADC(R.IX.B.h); }
template <class T> void CPUCore<T>::adc_a_iyl() { ADC(R.IY.B.l); }
template <class T> void CPUCore<T>::adc_a_iyh() { ADC(R.IY.B.h); }
template <class T> void CPUCore<T>::adc_a_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	ADC(n);
}
template <class T> inline void CPUCore<T>::adc_a_x(word x)
{
	byte n = RDMEM(x);
	ADC(n);
}
template <class T> void CPUCore<T>::adc_a_xhl() { adc_a_x(R.HL.w); }
template <class T> void CPUCore<T>::adc_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); adc_a_x(memptr.w);
}
template <class T> void CPUCore<T>::adc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); adc_a_x(memptr.w);
}

// ADD A,r
template <class T> inline void CPUCore<T>::ADD(byte reg)
{
	int res = R.AF.B.h + reg;
	R.AF.B.l = ZSXYTable[res & 0xFF] |
	           ((res & 0x100) ? C_FLAG : 0) |
	           ((R.AF.B.h ^ res ^ reg) & H_FLAG) |
	           (((reg ^ R.AF.B.h ^ 0x80) & (reg ^ res) & 0x80) ? V_FLAG : 0);
	R.AF.B.h = res;
}
template <class T> void CPUCore<T>::add_a_a()   { ADD(R.AF.B.h); }
template <class T> void CPUCore<T>::add_a_b()   { ADD(R.BC.B.h); }
template <class T> void CPUCore<T>::add_a_c()   { ADD(R.BC.B.l); }
template <class T> void CPUCore<T>::add_a_d()   { ADD(R.DE.B.h); }
template <class T> void CPUCore<T>::add_a_e()   { ADD(R.DE.B.l); }
template <class T> void CPUCore<T>::add_a_h()   { ADD(R.HL.B.h); }
template <class T> void CPUCore<T>::add_a_l()   { ADD(R.HL.B.l); }
template <class T> void CPUCore<T>::add_a_ixl() { ADD(R.IX.B.l); }
template <class T> void CPUCore<T>::add_a_ixh() { ADD(R.IX.B.h); }
template <class T> void CPUCore<T>::add_a_iyl() { ADD(R.IY.B.l); }
template <class T> void CPUCore<T>::add_a_iyh() { ADD(R.IY.B.h); }
template <class T> void CPUCore<T>::add_a_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	ADD(n);
}
template <class T> inline void CPUCore<T>::add_a_x(word x)
{
	byte n = RDMEM(x);
	ADD(n);
}
template <class T> void CPUCore<T>::add_a_xhl() { add_a_x(R.HL.w); }
template <class T> void CPUCore<T>::add_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); add_a_x(memptr.w);
}
template <class T> void CPUCore<T>::add_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); add_a_x(memptr.w);
}

// AND r
template <class T> inline void CPUCore<T>::AND(byte reg)
{
	R.AF.B.h &= reg;
	R.AF.B.l = ZSPXYTable[R.AF.B.h] | H_FLAG;
}
template <class T> void CPUCore<T>::and_a()   { AND(R.AF.B.h); }
template <class T> void CPUCore<T>::and_b()   { AND(R.BC.B.h); }
template <class T> void CPUCore<T>::and_c()   { AND(R.BC.B.l); }
template <class T> void CPUCore<T>::and_d()   { AND(R.DE.B.h); }
template <class T> void CPUCore<T>::and_e()   { AND(R.DE.B.l); }
template <class T> void CPUCore<T>::and_h()   { AND(R.HL.B.h); }
template <class T> void CPUCore<T>::and_l()   { AND(R.HL.B.l); }
template <class T> void CPUCore<T>::and_ixh() { AND(R.IX.B.h); }
template <class T> void CPUCore<T>::and_ixl() { AND(R.IX.B.l); }
template <class T> void CPUCore<T>::and_iyh() { AND(R.IY.B.h); }
template <class T> void CPUCore<T>::and_iyl() { AND(R.IY.B.l); }
template <class T> void CPUCore<T>::and_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	AND(n);
}
template <class T> inline void CPUCore<T>::and_x(word x)
{
	byte n = RDMEM(x);
	AND(n);
}
template <class T> void CPUCore<T>::and_xhl() { and_x(R.HL.w); }
template <class T> void CPUCore<T>::and_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); and_x(memptr.w);
}
template <class T> void CPUCore<T>::and_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); and_x(memptr.w);
}

// CP r
template <class T> inline void CPUCore<T>::CP(byte reg)
{
	int q = R.AF.B.h - reg;
	R.AF.B.l = ZSTable[q & 0xFF] |
	           (reg & (X_FLAG | Y_FLAG)) |	// XY from operand, not from result
	           ((q & 0x100) ? C_FLAG : 0) |
	           N_FLAG |
	           ((R.AF.B.h ^ q ^ reg) & H_FLAG) |
	           (((reg ^ R.AF.B.h) & (R.AF.B.h ^ q) & 0x80) ? V_FLAG : 0);
}
template <class T> void CPUCore<T>::cp_a()   { CP(R.AF.B.h); }
template <class T> void CPUCore<T>::cp_b()   { CP(R.BC.B.h); }
template <class T> void CPUCore<T>::cp_c()   { CP(R.BC.B.l); }
template <class T> void CPUCore<T>::cp_d()   { CP(R.DE.B.h); }
template <class T> void CPUCore<T>::cp_e()   { CP(R.DE.B.l); }
template <class T> void CPUCore<T>::cp_h()   { CP(R.HL.B.h); }
template <class T> void CPUCore<T>::cp_l()   { CP(R.HL.B.l); }
template <class T> void CPUCore<T>::cp_ixh() { CP(R.IX.B.h); }
template <class T> void CPUCore<T>::cp_ixl() { CP(R.IX.B.l); }
template <class T> void CPUCore<T>::cp_iyh() { CP(R.IY.B.h); }
template <class T> void CPUCore<T>::cp_iyl() { CP(R.IY.B.l); }
template <class T> void CPUCore<T>::cp_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	CP(n);
}
template <class T> inline void CPUCore<T>::cp_x(word x)
{
	byte n = RDMEM(x);
	CP(n);
}
template <class T> void CPUCore<T>::cp_xhl() { cp_x(R.HL.w); }
template <class T> void CPUCore<T>::cp_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); cp_x(memptr.w);
}
template <class T> void CPUCore<T>::cp_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); cp_x(memptr.w);
}

// OR r
template <class T> inline void CPUCore<T>::OR(byte reg)
{
	R.AF.B.h |= reg;
	R.AF.B.l = ZSPXYTable[R.AF.B.h];
}
template <class T> void CPUCore<T>::or_a()   { OR(R.AF.B.h); }
template <class T> void CPUCore<T>::or_b()   { OR(R.BC.B.h); }
template <class T> void CPUCore<T>::or_c()   { OR(R.BC.B.l); }
template <class T> void CPUCore<T>::or_d()   { OR(R.DE.B.h); }
template <class T> void CPUCore<T>::or_e()   { OR(R.DE.B.l); }
template <class T> void CPUCore<T>::or_h()   { OR(R.HL.B.h); }
template <class T> void CPUCore<T>::or_l()   { OR(R.HL.B.l); }
template <class T> void CPUCore<T>::or_ixh() { OR(R.IX.B.h); }
template <class T> void CPUCore<T>::or_ixl() { OR(R.IX.B.l); }
template <class T> void CPUCore<T>::or_iyh() { OR(R.IY.B.h); }
template <class T> void CPUCore<T>::or_iyl() { OR(R.IY.B.l); }
template <class T> void CPUCore<T>::or_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	OR(n);
}
template <class T> inline void CPUCore<T>::or_x(word x)
{
	byte n = RDMEM(x);
	OR(n);
}
template <class T> void CPUCore<T>::or_xhl() { or_x(R.HL.w); }
template <class T> void CPUCore<T>::or_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); or_x(memptr.w);
}
template <class T> void CPUCore<T>::or_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); or_x(memptr.w);
}

// SBC A,r
template <class T> inline void CPUCore<T>::SBC(byte reg)
{
	int res = R.AF.B.h - reg - ((R.AF.B.l & C_FLAG) ? 1 : 0);
	R.AF.B.l = ZSXYTable[res & 0xFF] |
	           ((res & 0x100) ? C_FLAG : 0) |
	           N_FLAG |
	           ((R.AF.B.h ^ res ^ reg) & H_FLAG) |
	           (((reg ^ R.AF.B.h) & (R.AF.B.h ^ res) & 0x80) ? V_FLAG : 0);
	R.AF.B.h = res;
}
template <class T> void CPUCore<T>::sbc_a_a()   { SBC(R.AF.B.h); }
template <class T> void CPUCore<T>::sbc_a_b()   { SBC(R.BC.B.h); }
template <class T> void CPUCore<T>::sbc_a_c()   { SBC(R.BC.B.l); }
template <class T> void CPUCore<T>::sbc_a_d()   { SBC(R.DE.B.h); }
template <class T> void CPUCore<T>::sbc_a_e()   { SBC(R.DE.B.l); }
template <class T> void CPUCore<T>::sbc_a_h()   { SBC(R.HL.B.h); }
template <class T> void CPUCore<T>::sbc_a_l()   { SBC(R.HL.B.l); }
template <class T> void CPUCore<T>::sbc_a_ixh() { SBC(R.IX.B.h); }
template <class T> void CPUCore<T>::sbc_a_ixl() { SBC(R.IX.B.l); }
template <class T> void CPUCore<T>::sbc_a_iyh() { SBC(R.IY.B.h); }
template <class T> void CPUCore<T>::sbc_a_iyl() { SBC(R.IY.B.l); }
template <class T> void CPUCore<T>::sbc_a_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	SBC(n);
}
template <class T> inline void CPUCore<T>::sbc_a_x(word x)
{
	byte n = RDMEM(x);
	SBC(n);
}
template <class T> void CPUCore<T>::sbc_a_xhl() { sbc_a_x(R.HL.w); }
template <class T> void CPUCore<T>::sbc_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); sbc_a_x(memptr.w);
}
template <class T> void CPUCore<T>::sbc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); sbc_a_x(memptr.w);
}

// SUB r
template <class T> inline void CPUCore<T>::SUB(byte reg)
{
	int res = R.AF.B.h - reg;
	R.AF.B.l = ZSXYTable[res & 0xFF] |
	           ((res & 0x100) ? C_FLAG : 0) |
	           N_FLAG |
	           ((R.AF.B.h ^ res ^ reg) & H_FLAG) |
	           (((reg ^ R.AF.B.h) & (R.AF.B.h ^ res) & 0x80) ? V_FLAG : 0);
	R.AF.B.h = res;
}
template <class T> void CPUCore<T>::sub_a()   { SUB(R.AF.B.h); }
template <class T> void CPUCore<T>::sub_b()   { SUB(R.BC.B.h); }
template <class T> void CPUCore<T>::sub_c()   { SUB(R.BC.B.l); }
template <class T> void CPUCore<T>::sub_d()   { SUB(R.DE.B.h); }
template <class T> void CPUCore<T>::sub_e()   { SUB(R.DE.B.l); }
template <class T> void CPUCore<T>::sub_h()   { SUB(R.HL.B.h); }
template <class T> void CPUCore<T>::sub_l()   { SUB(R.HL.B.l); }
template <class T> void CPUCore<T>::sub_ixh() { SUB(R.IX.B.h); }
template <class T> void CPUCore<T>::sub_ixl() { SUB(R.IX.B.l); }
template <class T> void CPUCore<T>::sub_iyh() { SUB(R.IY.B.h); }
template <class T> void CPUCore<T>::sub_iyl() { SUB(R.IY.B.l); }
template <class T> void CPUCore<T>::sub_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	SUB(n);
}
template <class T> inline void CPUCore<T>::sub_x(word x)
{
	byte n = RDMEM(x);
	SUB(n);
}
template <class T> void CPUCore<T>::sub_xhl() { sub_x(R.HL.w); }
template <class T> void CPUCore<T>::sub_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); sub_x(memptr.w);
}
template <class T> void CPUCore<T>::sub_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); sub_x(memptr.w);
}

// XOR r
template <class T> inline void CPUCore<T>::XOR(byte reg)
{
	R.AF.B.h ^= reg;
	R.AF.B.l = ZSPXYTable[R.AF.B.h];
	
}
template <class T> void CPUCore<T>::xor_a()   { XOR(R.AF.B.h); }
template <class T> void CPUCore<T>::xor_b()   { XOR(R.BC.B.h); }
template <class T> void CPUCore<T>::xor_c()   { XOR(R.BC.B.l); }
template <class T> void CPUCore<T>::xor_d()   { XOR(R.DE.B.h); }
template <class T> void CPUCore<T>::xor_e()   { XOR(R.DE.B.l); }
template <class T> void CPUCore<T>::xor_h()   { XOR(R.HL.B.h); }
template <class T> void CPUCore<T>::xor_l()   { XOR(R.HL.B.l); }
template <class T> void CPUCore<T>::xor_ixh() { XOR(R.IX.B.h); }
template <class T> void CPUCore<T>::xor_ixl() { XOR(R.IX.B.l); }
template <class T> void CPUCore<T>::xor_iyh() { XOR(R.IY.B.h); }
template <class T> void CPUCore<T>::xor_iyl() { XOR(R.IY.B.l); }
template <class T> void CPUCore<T>::xor_byte()
{
	byte n = RDMEM_OPCODE(R.PC.w++);
	XOR(n);
}
template <class T> inline void CPUCore<T>::xor_x(word x)
{
	byte n = RDMEM(x);
	XOR(n);
}
template <class T> void CPUCore<T>::xor_xhl() { xor_x(R.HL.w); }
template <class T> void CPUCore<T>::xor_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); xor_x(memptr.w);
}
template <class T> void CPUCore<T>::xor_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); xor_x(memptr.w);
}


// DEC r
template <class T> inline void CPUCore<T>::DEC(byte &reg)
{
	reg--;
	R.AF.B.l = (R.AF.B.l & C_FLAG) |
	           ((reg == 0x7f) ? V_FLAG : 0) |
	           (((reg & 0x0f) == 0x0f) ? H_FLAG : 0) |
	           ZSXYTable[reg] |
	           N_FLAG;
}
template <class T> void CPUCore<T>::dec_a()   { DEC(R.AF.B.h); }
template <class T> void CPUCore<T>::dec_b()   { DEC(R.BC.B.h); }
template <class T> void CPUCore<T>::dec_c()   { DEC(R.BC.B.l); }
template <class T> void CPUCore<T>::dec_d()   { DEC(R.DE.B.h); }
template <class T> void CPUCore<T>::dec_e()   { DEC(R.DE.B.l); }
template <class T> void CPUCore<T>::dec_h()   { DEC(R.HL.B.h); }
template <class T> void CPUCore<T>::dec_l()   { DEC(R.HL.B.l); }
template <class T> void CPUCore<T>::dec_ixh() { DEC(R.IX.B.h); }
template <class T> void CPUCore<T>::dec_ixl() { DEC(R.IX.B.l); }
template <class T> void CPUCore<T>::dec_iyh() { DEC(R.IY.B.h); }
template <class T> void CPUCore<T>::dec_iyl() { DEC(R.IY.B.l); }

template <class T> inline void CPUCore<T>::DEC_X(word x)
{
	byte val = RDMEM(x);
	DEC(val); T::INC_DELAY();
	WRMEM(x, val);
}
template <class T> void CPUCore<T>::dec_xhl() { DEC_X(R.HL.w); }
template <class T> void CPUCore<T>::dec_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); DEC_X(memptr.w);
}
template <class T> void CPUCore<T>::dec_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); DEC_X(memptr.w);
}

// INC r
template <class T> inline void CPUCore<T>::INC(byte &reg)
{
	reg++;
	R.AF.B.l = (R.AF.B.l & C_FLAG) |
	           ((reg == 0x80) ? V_FLAG : 0) |
	           (((reg & 0x0f) == 0x00) ? H_FLAG : 0) |
	           ZSXYTable[reg];
}
template <class T> void CPUCore<T>::inc_a()   { INC(R.AF.B.h); }
template <class T> void CPUCore<T>::inc_b()   { INC(R.BC.B.h); }
template <class T> void CPUCore<T>::inc_c()   { INC(R.BC.B.l); }
template <class T> void CPUCore<T>::inc_d()   { INC(R.DE.B.h); }
template <class T> void CPUCore<T>::inc_e()   { INC(R.DE.B.l); }
template <class T> void CPUCore<T>::inc_h()   { INC(R.HL.B.h); }
template <class T> void CPUCore<T>::inc_l()   { INC(R.HL.B.l); }
template <class T> void CPUCore<T>::inc_ixh() { INC(R.IX.B.h); }
template <class T> void CPUCore<T>::inc_ixl() { INC(R.IX.B.l); }
template <class T> void CPUCore<T>::inc_iyh() { INC(R.IY.B.h); }
template <class T> void CPUCore<T>::inc_iyl() { INC(R.IY.B.l); }

template <class T> inline void CPUCore<T>::INC_X(word x) {
	byte val = RDMEM(x);
	INC(val); T::INC_DELAY();
	WRMEM(x, val);
}
template <class T> void CPUCore<T>::inc_xhl() { INC_X(R.HL.w); }
template <class T> void CPUCore<T>::inc_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IX.w + ofst;
	T::ADD_16_8_DELAY(); INC_X(memptr.w);
}
template <class T> void CPUCore<T>::inc_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	memptr.w = R.IY.w + ofst;
	T::ADD_16_8_DELAY(); INC_X(memptr.w);
}


// ADC HL,ss
template <class T> inline void CPUCore<T>::ADCW(word reg)
{
	memptr.w = R.HL.w + 1;
	int res = R.HL.w + reg + ((R.AF.B.l & C_FLAG) ? 1 : 0);
	R.AF.B.l = (((R.HL.w ^ res ^ reg) >> 8) & H_FLAG) |
	           ((res & 0x10000) ? C_FLAG : 0) |
	           ((res & 0xffff) ? 0 : Z_FLAG) |
	           (((reg ^ R.HL.w ^ 0x8000) & (reg ^ res) & 0x8000) ? V_FLAG : 0) |
	           ((res >> 8) & (S_FLAG | X_FLAG | Y_FLAG));
	R.HL.w = res;
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::adc_hl_bc() { ADCW(R.BC.w); }
template <class T> void CPUCore<T>::adc_hl_de() { ADCW(R.DE.w); }
template <class T> void CPUCore<T>::adc_hl_hl() { ADCW(R.HL.w); }
template <class T> void CPUCore<T>::adc_hl_sp() { ADCW(R.SP.w); }

// ADD HL/IX/IY,ss
template <class T> inline void CPUCore<T>::ADDW(word &reg1, word reg2)
{
	memptr.w = reg1 + 1;
	int res = reg1 + reg2;
	R.AF.B.l = (R.AF.B.l & (S_FLAG | Z_FLAG | V_FLAG)) |
	           (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	           ((res & 0x10000) ? C_FLAG : 0) |
	           ((res >> 8) & (X_FLAG | Y_FLAG));
	reg1 = res;
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::add_hl_bc() { ADDW(R.HL.w, R.BC.w); }
template <class T> void CPUCore<T>::add_hl_de() { ADDW(R.HL.w, R.DE.w); }
template <class T> void CPUCore<T>::add_hl_hl() { ADDW(R.HL.w, R.HL.w); }
template <class T> void CPUCore<T>::add_hl_sp() { ADDW(R.HL.w, R.SP.w); }
template <class T> void CPUCore<T>::add_ix_bc() { ADDW(R.IX.w, R.BC.w); }
template <class T> void CPUCore<T>::add_ix_de() { ADDW(R.IX.w, R.DE.w); }
template <class T> void CPUCore<T>::add_ix_ix() { ADDW(R.IX.w, R.IX.w); }
template <class T> void CPUCore<T>::add_ix_sp() { ADDW(R.IX.w, R.SP.w); }
template <class T> void CPUCore<T>::add_iy_bc() { ADDW(R.IY.w, R.BC.w); }
template <class T> void CPUCore<T>::add_iy_de() { ADDW(R.IY.w, R.DE.w); }
template <class T> void CPUCore<T>::add_iy_iy() { ADDW(R.IY.w, R.IY.w); }
template <class T> void CPUCore<T>::add_iy_sp() { ADDW(R.IY.w, R.SP.w); }

// SBC HL,ss
template <class T> inline void CPUCore<T>::SBCW(word reg)
{
	memptr.w = R.HL.w + 1;
	int res = R.HL.w - reg - ((R.AF.B.l & C_FLAG) ? 1 : 0);
	R.AF.B.l = (((R.HL.w ^ res ^ reg) >> 8) & H_FLAG) |
	           ((res & 0x10000) ? C_FLAG : 0) |
	           ((res & 0xffff) ? 0 : Z_FLAG) |
	           (((reg ^ R.HL.w) & (R.HL.w ^ res) & 0x8000) ? V_FLAG : 0) |
		   ((res >> 8) & (S_FLAG | X_FLAG | Y_FLAG)) |
	           N_FLAG;
	R.HL.w = res;
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::sbc_hl_bc() { SBCW(R.BC.w); }
template <class T> void CPUCore<T>::sbc_hl_de() { SBCW(R.DE.w); }
template <class T> void CPUCore<T>::sbc_hl_hl() { SBCW(R.HL.w); }
template <class T> void CPUCore<T>::sbc_hl_sp() { SBCW(R.SP.w); }


// DEC ss
template <class T> void CPUCore<T>::dec_bc() { --R.BC.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_de() { --R.DE.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_hl() { --R.HL.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_ix() { --R.IX.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_iy() { --R.IY.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_sp() { --R.SP.w; T::INC_16_DELAY(); }

// INC ss
template <class T> void CPUCore<T>::inc_bc() { ++R.BC.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_de() { ++R.DE.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_hl() { ++R.HL.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_ix() { ++R.IX.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_iy() { ++R.IY.w; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_sp() { ++R.SP.w; T::INC_16_DELAY(); }


// BIT n,r
template <class T> inline void CPUCore<T>::BIT(byte b, byte reg)
{
	R.AF.B.l = (R.AF.B.l & C_FLAG) | 
	           ZSPTable[reg & (1 << b)] |
		   (reg & (X_FLAG | Y_FLAG)) |
		   H_FLAG;
}
template <class T> void CPUCore<T>::bit_0_a() { BIT(0, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_0_b() { BIT(0, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_0_c() { BIT(0, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_0_d() { BIT(0, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_0_e() { BIT(0, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_0_h() { BIT(0, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_0_l() { BIT(0, R.HL.B.l); }
template <class T> void CPUCore<T>::bit_1_a() { BIT(1, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_1_b() { BIT(1, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_1_c() { BIT(1, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_1_d() { BIT(1, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_1_e() { BIT(1, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_1_h() { BIT(1, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_1_l() { BIT(1, R.HL.B.l); }
template <class T> void CPUCore<T>::bit_2_a() { BIT(2, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_2_b() { BIT(2, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_2_c() { BIT(2, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_2_d() { BIT(2, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_2_e() { BIT(2, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_2_h() { BIT(2, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_2_l() { BIT(2, R.HL.B.l); }
template <class T> void CPUCore<T>::bit_3_a() { BIT(3, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_3_b() { BIT(3, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_3_c() { BIT(3, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_3_d() { BIT(3, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_3_e() { BIT(3, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_3_h() { BIT(3, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_3_l() { BIT(3, R.HL.B.l); }
template <class T> void CPUCore<T>::bit_4_a() { BIT(4, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_4_b() { BIT(4, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_4_c() { BIT(4, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_4_d() { BIT(4, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_4_e() { BIT(4, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_4_h() { BIT(4, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_4_l() { BIT(4, R.HL.B.l); }
template <class T> void CPUCore<T>::bit_5_a() { BIT(5, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_5_b() { BIT(5, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_5_c() { BIT(5, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_5_d() { BIT(5, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_5_e() { BIT(5, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_5_h() { BIT(5, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_5_l() { BIT(5, R.HL.B.l); }
template <class T> void CPUCore<T>::bit_6_a() { BIT(6, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_6_b() { BIT(6, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_6_c() { BIT(6, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_6_d() { BIT(6, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_6_e() { BIT(6, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_6_h() { BIT(6, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_6_l() { BIT(6, R.HL.B.l); }
template <class T> void CPUCore<T>::bit_7_a() { BIT(7, R.AF.B.h); }
template <class T> void CPUCore<T>::bit_7_b() { BIT(7, R.BC.B.h); }
template <class T> void CPUCore<T>::bit_7_c() { BIT(7, R.BC.B.l); }
template <class T> void CPUCore<T>::bit_7_d() { BIT(7, R.DE.B.h); }
template <class T> void CPUCore<T>::bit_7_e() { BIT(7, R.DE.B.l); }
template <class T> void CPUCore<T>::bit_7_h() { BIT(7, R.HL.B.h); }
template <class T> void CPUCore<T>::bit_7_l() { BIT(7, R.HL.B.l); }

template <class T> inline void CPUCore<T>::BIT_HL(byte bit)
{
	byte n = RDMEM(R.HL.w);
	R.AF.B.l = (R.AF.B.l & C_FLAG) |
		    ZSPTable[n & (1 << bit)] |
		    H_FLAG |
		    memptr.B.h & (X_FLAG | Y_FLAG);
	T::SMALL_DELAY();
}
template <class T> void CPUCore<T>::bit_0_xhl() { BIT_HL(0); }
template <class T> void CPUCore<T>::bit_1_xhl() { BIT_HL(1); }
template <class T> void CPUCore<T>::bit_2_xhl() { BIT_HL(2); }
template <class T> void CPUCore<T>::bit_3_xhl() { BIT_HL(3); }
template <class T> void CPUCore<T>::bit_4_xhl() { BIT_HL(4); }
template <class T> void CPUCore<T>::bit_5_xhl() { BIT_HL(5); }
template <class T> void CPUCore<T>::bit_6_xhl() { BIT_HL(6); }
template <class T> void CPUCore<T>::bit_7_xhl() { BIT_HL(7); }

template <class T> inline void CPUCore<T>::BIT_IX(byte bit)
{
	word addr = R.IX.w + ofst;
	byte n = RDMEM(addr);
	memptr.w = addr;
	R.AF.B.l = (R.AF.B.l & C_FLAG) | 
		   ZSPTable[n & (1 << bit)] | 
		   H_FLAG |
		   memptr.B.h & (X_FLAG | Y_FLAG);
	T::SMALL_DELAY();
}
template <class T> void CPUCore<T>::bit_0_xix() { BIT_IX(0); }
template <class T> void CPUCore<T>::bit_1_xix() { BIT_IX(1); }
template <class T> void CPUCore<T>::bit_2_xix() { BIT_IX(2); }
template <class T> void CPUCore<T>::bit_3_xix() { BIT_IX(3); }
template <class T> void CPUCore<T>::bit_4_xix() { BIT_IX(4); }
template <class T> void CPUCore<T>::bit_5_xix() { BIT_IX(5); }
template <class T> void CPUCore<T>::bit_6_xix() { BIT_IX(6); }
template <class T> void CPUCore<T>::bit_7_xix() { BIT_IX(7); }

template <class T> inline void CPUCore<T>::BIT_IY(byte bit)
{
	word addr = R.IY.w + ofst;
	byte n = RDMEM(addr);
	memptr.w = addr;
	R.AF.B.l = (R.AF.B.l & C_FLAG) | 
		   ZSPTable[n & (1 << bit)] | 
		   H_FLAG |
		   memptr.B.h & (X_FLAG | Y_FLAG);
	T::SMALL_DELAY();
}
template <class T> void CPUCore<T>::bit_0_xiy() { BIT_IY(0); }
template <class T> void CPUCore<T>::bit_1_xiy() { BIT_IY(1); }
template <class T> void CPUCore<T>::bit_2_xiy() { BIT_IY(2); }
template <class T> void CPUCore<T>::bit_3_xiy() { BIT_IY(3); }
template <class T> void CPUCore<T>::bit_4_xiy() { BIT_IY(4); }
template <class T> void CPUCore<T>::bit_5_xiy() { BIT_IY(5); }
template <class T> void CPUCore<T>::bit_6_xiy() { BIT_IY(6); }
template <class T> void CPUCore<T>::bit_7_xiy() { BIT_IY(7); }


// RES n,r
template <class T> inline void CPUCore<T>::RES(byte b, byte& reg)
{
	reg &= ~(1 << b);
}
template <class T> void CPUCore<T>::res_0_a() { RES(0, R.AF.B.h); }
template <class T> void CPUCore<T>::res_0_b() { RES(0, R.BC.B.h); }
template <class T> void CPUCore<T>::res_0_c() { RES(0, R.BC.B.l); }
template <class T> void CPUCore<T>::res_0_d() { RES(0, R.DE.B.h); }
template <class T> void CPUCore<T>::res_0_e() { RES(0, R.DE.B.l); }
template <class T> void CPUCore<T>::res_0_h() { RES(0, R.HL.B.h); }
template <class T> void CPUCore<T>::res_0_l() { RES(0, R.HL.B.l); }
template <class T> void CPUCore<T>::res_1_a() { RES(1, R.AF.B.h); }
template <class T> void CPUCore<T>::res_1_b() { RES(1, R.BC.B.h); }
template <class T> void CPUCore<T>::res_1_c() { RES(1, R.BC.B.l); }
template <class T> void CPUCore<T>::res_1_d() { RES(1, R.DE.B.h); }
template <class T> void CPUCore<T>::res_1_e() { RES(1, R.DE.B.l); }
template <class T> void CPUCore<T>::res_1_h() { RES(1, R.HL.B.h); }
template <class T> void CPUCore<T>::res_1_l() { RES(1, R.HL.B.l); }
template <class T> void CPUCore<T>::res_2_a() { RES(2, R.AF.B.h); }
template <class T> void CPUCore<T>::res_2_b() { RES(2, R.BC.B.h); }
template <class T> void CPUCore<T>::res_2_c() { RES(2, R.BC.B.l); }
template <class T> void CPUCore<T>::res_2_d() { RES(2, R.DE.B.h); }
template <class T> void CPUCore<T>::res_2_e() { RES(2, R.DE.B.l); }
template <class T> void CPUCore<T>::res_2_h() { RES(2, R.HL.B.h); }
template <class T> void CPUCore<T>::res_2_l() { RES(2, R.HL.B.l); }
template <class T> void CPUCore<T>::res_3_a() { RES(3, R.AF.B.h); }
template <class T> void CPUCore<T>::res_3_b() { RES(3, R.BC.B.h); }
template <class T> void CPUCore<T>::res_3_c() { RES(3, R.BC.B.l); }
template <class T> void CPUCore<T>::res_3_d() { RES(3, R.DE.B.h); }
template <class T> void CPUCore<T>::res_3_e() { RES(3, R.DE.B.l); }
template <class T> void CPUCore<T>::res_3_h() { RES(3, R.HL.B.h); }
template <class T> void CPUCore<T>::res_3_l() { RES(3, R.HL.B.l); }
template <class T> void CPUCore<T>::res_4_a() { RES(4, R.AF.B.h); }
template <class T> void CPUCore<T>::res_4_b() { RES(4, R.BC.B.h); }
template <class T> void CPUCore<T>::res_4_c() { RES(4, R.BC.B.l); }
template <class T> void CPUCore<T>::res_4_d() { RES(4, R.DE.B.h); }
template <class T> void CPUCore<T>::res_4_e() { RES(4, R.DE.B.l); }
template <class T> void CPUCore<T>::res_4_h() { RES(4, R.HL.B.h); }
template <class T> void CPUCore<T>::res_4_l() { RES(4, R.HL.B.l); }
template <class T> void CPUCore<T>::res_5_a() { RES(5, R.AF.B.h); }
template <class T> void CPUCore<T>::res_5_b() { RES(5, R.BC.B.h); }
template <class T> void CPUCore<T>::res_5_c() { RES(5, R.BC.B.l); }
template <class T> void CPUCore<T>::res_5_d() { RES(5, R.DE.B.h); }
template <class T> void CPUCore<T>::res_5_e() { RES(5, R.DE.B.l); }
template <class T> void CPUCore<T>::res_5_h() { RES(5, R.HL.B.h); }
template <class T> void CPUCore<T>::res_5_l() { RES(5, R.HL.B.l); }
template <class T> void CPUCore<T>::res_6_a() { RES(6, R.AF.B.h); }
template <class T> void CPUCore<T>::res_6_b() { RES(6, R.BC.B.h); }
template <class T> void CPUCore<T>::res_6_c() { RES(6, R.BC.B.l); }
template <class T> void CPUCore<T>::res_6_d() { RES(6, R.DE.B.h); }
template <class T> void CPUCore<T>::res_6_e() { RES(6, R.DE.B.l); }
template <class T> void CPUCore<T>::res_6_h() { RES(6, R.HL.B.h); }
template <class T> void CPUCore<T>::res_6_l() { RES(6, R.HL.B.l); }
template <class T> void CPUCore<T>::res_7_a() { RES(7, R.AF.B.h); }
template <class T> void CPUCore<T>::res_7_b() { RES(7, R.BC.B.h); }
template <class T> void CPUCore<T>::res_7_c() { RES(7, R.BC.B.l); }
template <class T> void CPUCore<T>::res_7_d() { RES(7, R.DE.B.h); }
template <class T> void CPUCore<T>::res_7_e() { RES(7, R.DE.B.l); }
template <class T> void CPUCore<T>::res_7_h() { RES(7, R.HL.B.h); }
template <class T> void CPUCore<T>::res_7_l() { RES(7, R.HL.B.l); }

template <class T> inline void CPUCore<T>::RES_X(byte bit, byte& reg, word x)
{
	reg = RDMEM(x);
	RES(bit, reg); 
	T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::RES_X_(byte bit, byte& reg, word x)
{
	memptr.w = x; RES_X(bit, reg, x);
}
template <class T> inline void CPUCore<T>::RES_X_X(byte bit, byte& reg)
{
	RES_X_(bit, reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::RES_X_Y(byte bit, byte& reg)
{
	RES_X_(bit, reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::res_0_xhl()   { byte dummy; RES_X(0, dummy, R.HL.w); }
template <class T> void CPUCore<T>::res_1_xhl()   { byte dummy; RES_X(1, dummy, R.HL.w); }
template <class T> void CPUCore<T>::res_2_xhl()   { byte dummy; RES_X(2, dummy, R.HL.w); }
template <class T> void CPUCore<T>::res_3_xhl()   { byte dummy; RES_X(3, dummy, R.HL.w); }
template <class T> void CPUCore<T>::res_4_xhl()   { byte dummy; RES_X(4, dummy, R.HL.w); }
template <class T> void CPUCore<T>::res_5_xhl()   { byte dummy; RES_X(5, dummy, R.HL.w); }
template <class T> void CPUCore<T>::res_6_xhl()   { byte dummy; RES_X(6, dummy, R.HL.w); }
template <class T> void CPUCore<T>::res_7_xhl()   { byte dummy; RES_X(7, dummy, R.HL.w); }

template <class T> void CPUCore<T>::res_0_xix()   { byte dummy; RES_X_X(0, dummy); }
template <class T> void CPUCore<T>::res_1_xix()   { byte dummy; RES_X_X(1, dummy); }
template <class T> void CPUCore<T>::res_2_xix()   { byte dummy; RES_X_X(2, dummy); }
template <class T> void CPUCore<T>::res_3_xix()   { byte dummy; RES_X_X(3, dummy); }
template <class T> void CPUCore<T>::res_4_xix()   { byte dummy; RES_X_X(4, dummy); }
template <class T> void CPUCore<T>::res_5_xix()   { byte dummy; RES_X_X(5, dummy); }
template <class T> void CPUCore<T>::res_6_xix()   { byte dummy; RES_X_X(6, dummy); }
template <class T> void CPUCore<T>::res_7_xix()   { byte dummy; RES_X_X(7, dummy); }

template <class T> void CPUCore<T>::res_0_xiy()   { byte dummy; RES_X_Y(0, dummy); }
template <class T> void CPUCore<T>::res_1_xiy()   { byte dummy; RES_X_Y(1, dummy); }
template <class T> void CPUCore<T>::res_2_xiy()   { byte dummy; RES_X_Y(2, dummy); }
template <class T> void CPUCore<T>::res_3_xiy()   { byte dummy; RES_X_Y(3, dummy); }
template <class T> void CPUCore<T>::res_4_xiy()   { byte dummy; RES_X_Y(4, dummy); }
template <class T> void CPUCore<T>::res_5_xiy()   { byte dummy; RES_X_Y(5, dummy); }
template <class T> void CPUCore<T>::res_6_xiy()   { byte dummy; RES_X_Y(6, dummy); }
template <class T> void CPUCore<T>::res_7_xiy()   { byte dummy; RES_X_Y(7, dummy); }

template <class T> void CPUCore<T>::res_0_xix_a() { RES_X_X(0, R.AF.B.h); }
template <class T> void CPUCore<T>::res_0_xix_b() { RES_X_X(0, R.BC.B.h); }
template <class T> void CPUCore<T>::res_0_xix_c() { RES_X_X(0, R.BC.B.l); }
template <class T> void CPUCore<T>::res_0_xix_d() { RES_X_X(0, R.DE.B.h); }
template <class T> void CPUCore<T>::res_0_xix_e() { RES_X_X(0, R.DE.B.l); }
template <class T> void CPUCore<T>::res_0_xix_h() { RES_X_X(0, R.HL.B.h); }
template <class T> void CPUCore<T>::res_0_xix_l() { RES_X_X(0, R.HL.B.l); }
template <class T> void CPUCore<T>::res_1_xix_a() { RES_X_X(1, R.AF.B.h); }
template <class T> void CPUCore<T>::res_1_xix_b() { RES_X_X(1, R.BC.B.h); }
template <class T> void CPUCore<T>::res_1_xix_c() { RES_X_X(1, R.BC.B.l); }
template <class T> void CPUCore<T>::res_1_xix_d() { RES_X_X(1, R.DE.B.h); }
template <class T> void CPUCore<T>::res_1_xix_e() { RES_X_X(1, R.DE.B.l); }
template <class T> void CPUCore<T>::res_1_xix_h() { RES_X_X(1, R.HL.B.h); }
template <class T> void CPUCore<T>::res_1_xix_l() { RES_X_X(1, R.HL.B.l); }
template <class T> void CPUCore<T>::res_2_xix_a() { RES_X_X(2, R.AF.B.h); }
template <class T> void CPUCore<T>::res_2_xix_b() { RES_X_X(2, R.BC.B.h); }
template <class T> void CPUCore<T>::res_2_xix_c() { RES_X_X(2, R.BC.B.l); }
template <class T> void CPUCore<T>::res_2_xix_d() { RES_X_X(2, R.DE.B.h); }
template <class T> void CPUCore<T>::res_2_xix_e() { RES_X_X(2, R.DE.B.l); }
template <class T> void CPUCore<T>::res_2_xix_h() { RES_X_X(2, R.HL.B.h); }
template <class T> void CPUCore<T>::res_2_xix_l() { RES_X_X(2, R.HL.B.l); }
template <class T> void CPUCore<T>::res_3_xix_a() { RES_X_X(3, R.AF.B.h); }
template <class T> void CPUCore<T>::res_3_xix_b() { RES_X_X(3, R.BC.B.h); }
template <class T> void CPUCore<T>::res_3_xix_c() { RES_X_X(3, R.BC.B.l); }
template <class T> void CPUCore<T>::res_3_xix_d() { RES_X_X(3, R.DE.B.h); }
template <class T> void CPUCore<T>::res_3_xix_e() { RES_X_X(3, R.DE.B.l); }
template <class T> void CPUCore<T>::res_3_xix_h() { RES_X_X(3, R.HL.B.h); }
template <class T> void CPUCore<T>::res_3_xix_l() { RES_X_X(3, R.HL.B.l); }
template <class T> void CPUCore<T>::res_4_xix_a() { RES_X_X(4, R.AF.B.h); }
template <class T> void CPUCore<T>::res_4_xix_b() { RES_X_X(4, R.BC.B.h); }
template <class T> void CPUCore<T>::res_4_xix_c() { RES_X_X(4, R.BC.B.l); }
template <class T> void CPUCore<T>::res_4_xix_d() { RES_X_X(4, R.DE.B.h); }
template <class T> void CPUCore<T>::res_4_xix_e() { RES_X_X(4, R.DE.B.l); }
template <class T> void CPUCore<T>::res_4_xix_h() { RES_X_X(4, R.HL.B.h); }
template <class T> void CPUCore<T>::res_4_xix_l() { RES_X_X(4, R.HL.B.l); }
template <class T> void CPUCore<T>::res_5_xix_a() { RES_X_X(5, R.AF.B.h); }
template <class T> void CPUCore<T>::res_5_xix_b() { RES_X_X(5, R.BC.B.h); }
template <class T> void CPUCore<T>::res_5_xix_c() { RES_X_X(5, R.BC.B.l); }
template <class T> void CPUCore<T>::res_5_xix_d() { RES_X_X(5, R.DE.B.h); }
template <class T> void CPUCore<T>::res_5_xix_e() { RES_X_X(5, R.DE.B.l); }
template <class T> void CPUCore<T>::res_5_xix_h() { RES_X_X(5, R.HL.B.h); }
template <class T> void CPUCore<T>::res_5_xix_l() { RES_X_X(5, R.HL.B.l); }
template <class T> void CPUCore<T>::res_6_xix_a() { RES_X_X(6, R.AF.B.h); }
template <class T> void CPUCore<T>::res_6_xix_b() { RES_X_X(6, R.BC.B.h); }
template <class T> void CPUCore<T>::res_6_xix_c() { RES_X_X(6, R.BC.B.l); }
template <class T> void CPUCore<T>::res_6_xix_d() { RES_X_X(6, R.DE.B.h); }
template <class T> void CPUCore<T>::res_6_xix_e() { RES_X_X(6, R.DE.B.l); }
template <class T> void CPUCore<T>::res_6_xix_h() { RES_X_X(6, R.HL.B.h); }
template <class T> void CPUCore<T>::res_6_xix_l() { RES_X_X(6, R.HL.B.l); }
template <class T> void CPUCore<T>::res_7_xix_a() { RES_X_X(7, R.AF.B.h); }
template <class T> void CPUCore<T>::res_7_xix_b() { RES_X_X(7, R.BC.B.h); }
template <class T> void CPUCore<T>::res_7_xix_c() { RES_X_X(7, R.BC.B.l); }
template <class T> void CPUCore<T>::res_7_xix_d() { RES_X_X(7, R.DE.B.h); }
template <class T> void CPUCore<T>::res_7_xix_e() { RES_X_X(7, R.DE.B.l); }
template <class T> void CPUCore<T>::res_7_xix_h() { RES_X_X(7, R.HL.B.h); }
template <class T> void CPUCore<T>::res_7_xix_l() { RES_X_X(7, R.HL.B.l); }

template <class T> void CPUCore<T>::res_0_xiy_a() { RES_X_Y(0, R.AF.B.h); }
template <class T> void CPUCore<T>::res_0_xiy_b() { RES_X_Y(0, R.BC.B.h); }
template <class T> void CPUCore<T>::res_0_xiy_c() { RES_X_Y(0, R.BC.B.l); }
template <class T> void CPUCore<T>::res_0_xiy_d() { RES_X_Y(0, R.DE.B.h); }
template <class T> void CPUCore<T>::res_0_xiy_e() { RES_X_Y(0, R.DE.B.l); }
template <class T> void CPUCore<T>::res_0_xiy_h() { RES_X_Y(0, R.HL.B.h); }
template <class T> void CPUCore<T>::res_0_xiy_l() { RES_X_Y(0, R.HL.B.l); }
template <class T> void CPUCore<T>::res_1_xiy_a() { RES_X_Y(1, R.AF.B.h); }
template <class T> void CPUCore<T>::res_1_xiy_b() { RES_X_Y(1, R.BC.B.h); }
template <class T> void CPUCore<T>::res_1_xiy_c() { RES_X_Y(1, R.BC.B.l); }
template <class T> void CPUCore<T>::res_1_xiy_d() { RES_X_Y(1, R.DE.B.h); }
template <class T> void CPUCore<T>::res_1_xiy_e() { RES_X_Y(1, R.DE.B.l); }
template <class T> void CPUCore<T>::res_1_xiy_h() { RES_X_Y(1, R.HL.B.h); }
template <class T> void CPUCore<T>::res_1_xiy_l() { RES_X_Y(1, R.HL.B.l); }
template <class T> void CPUCore<T>::res_2_xiy_a() { RES_X_Y(2, R.AF.B.h); }
template <class T> void CPUCore<T>::res_2_xiy_b() { RES_X_Y(2, R.BC.B.h); }
template <class T> void CPUCore<T>::res_2_xiy_c() { RES_X_Y(2, R.BC.B.l); }
template <class T> void CPUCore<T>::res_2_xiy_d() { RES_X_Y(2, R.DE.B.h); }
template <class T> void CPUCore<T>::res_2_xiy_e() { RES_X_Y(2, R.DE.B.l); }
template <class T> void CPUCore<T>::res_2_xiy_h() { RES_X_Y(2, R.HL.B.h); }
template <class T> void CPUCore<T>::res_2_xiy_l() { RES_X_Y(2, R.HL.B.l); }
template <class T> void CPUCore<T>::res_3_xiy_a() { RES_X_Y(3, R.AF.B.h); }
template <class T> void CPUCore<T>::res_3_xiy_b() { RES_X_Y(3, R.BC.B.h); }
template <class T> void CPUCore<T>::res_3_xiy_c() { RES_X_Y(3, R.BC.B.l); }
template <class T> void CPUCore<T>::res_3_xiy_d() { RES_X_Y(3, R.DE.B.h); }
template <class T> void CPUCore<T>::res_3_xiy_e() { RES_X_Y(3, R.DE.B.l); }
template <class T> void CPUCore<T>::res_3_xiy_h() { RES_X_Y(3, R.HL.B.h); }
template <class T> void CPUCore<T>::res_3_xiy_l() { RES_X_Y(3, R.HL.B.l); }
template <class T> void CPUCore<T>::res_4_xiy_a() { RES_X_Y(4, R.AF.B.h); }
template <class T> void CPUCore<T>::res_4_xiy_b() { RES_X_Y(4, R.BC.B.h); }
template <class T> void CPUCore<T>::res_4_xiy_c() { RES_X_Y(4, R.BC.B.l); }
template <class T> void CPUCore<T>::res_4_xiy_d() { RES_X_Y(4, R.DE.B.h); }
template <class T> void CPUCore<T>::res_4_xiy_e() { RES_X_Y(4, R.DE.B.l); }
template <class T> void CPUCore<T>::res_4_xiy_h() { RES_X_Y(4, R.HL.B.h); }
template <class T> void CPUCore<T>::res_4_xiy_l() { RES_X_Y(4, R.HL.B.l); }
template <class T> void CPUCore<T>::res_5_xiy_a() { RES_X_Y(5, R.AF.B.h); }
template <class T> void CPUCore<T>::res_5_xiy_b() { RES_X_Y(5, R.BC.B.h); }
template <class T> void CPUCore<T>::res_5_xiy_c() { RES_X_Y(5, R.BC.B.l); }
template <class T> void CPUCore<T>::res_5_xiy_d() { RES_X_Y(5, R.DE.B.h); }
template <class T> void CPUCore<T>::res_5_xiy_e() { RES_X_Y(5, R.DE.B.l); }
template <class T> void CPUCore<T>::res_5_xiy_h() { RES_X_Y(5, R.HL.B.h); }
template <class T> void CPUCore<T>::res_5_xiy_l() { RES_X_Y(5, R.HL.B.l); }
template <class T> void CPUCore<T>::res_6_xiy_a() { RES_X_Y(6, R.AF.B.h); }
template <class T> void CPUCore<T>::res_6_xiy_b() { RES_X_Y(6, R.BC.B.h); }
template <class T> void CPUCore<T>::res_6_xiy_c() { RES_X_Y(6, R.BC.B.l); }
template <class T> void CPUCore<T>::res_6_xiy_d() { RES_X_Y(6, R.DE.B.h); }
template <class T> void CPUCore<T>::res_6_xiy_e() { RES_X_Y(6, R.DE.B.l); }
template <class T> void CPUCore<T>::res_6_xiy_h() { RES_X_Y(6, R.HL.B.h); }
template <class T> void CPUCore<T>::res_6_xiy_l() { RES_X_Y(6, R.HL.B.l); }
template <class T> void CPUCore<T>::res_7_xiy_a() { RES_X_Y(7, R.AF.B.h); }
template <class T> void CPUCore<T>::res_7_xiy_b() { RES_X_Y(7, R.BC.B.h); }
template <class T> void CPUCore<T>::res_7_xiy_c() { RES_X_Y(7, R.BC.B.l); }
template <class T> void CPUCore<T>::res_7_xiy_d() { RES_X_Y(7, R.DE.B.h); }
template <class T> void CPUCore<T>::res_7_xiy_e() { RES_X_Y(7, R.DE.B.l); }
template <class T> void CPUCore<T>::res_7_xiy_h() { RES_X_Y(7, R.HL.B.h); }
template <class T> void CPUCore<T>::res_7_xiy_l() { RES_X_Y(7, R.HL.B.l); }


// SET n,r
template <class T> inline void CPUCore<T>::SET(byte b, byte &reg)
{
	reg |= (1 << b);
}
template <class T> void CPUCore<T>::set_0_a() { SET(0, R.AF.B.h); }
template <class T> void CPUCore<T>::set_0_b() { SET(0, R.BC.B.h); }
template <class T> void CPUCore<T>::set_0_c() { SET(0, R.BC.B.l); }
template <class T> void CPUCore<T>::set_0_d() { SET(0, R.DE.B.h); }
template <class T> void CPUCore<T>::set_0_e() { SET(0, R.DE.B.l); }
template <class T> void CPUCore<T>::set_0_h() { SET(0, R.HL.B.h); }
template <class T> void CPUCore<T>::set_0_l() { SET(0, R.HL.B.l); }
template <class T> void CPUCore<T>::set_1_a() { SET(1, R.AF.B.h); }
template <class T> void CPUCore<T>::set_1_b() { SET(1, R.BC.B.h); }
template <class T> void CPUCore<T>::set_1_c() { SET(1, R.BC.B.l); }
template <class T> void CPUCore<T>::set_1_d() { SET(1, R.DE.B.h); }
template <class T> void CPUCore<T>::set_1_e() { SET(1, R.DE.B.l); }
template <class T> void CPUCore<T>::set_1_h() { SET(1, R.HL.B.h); }
template <class T> void CPUCore<T>::set_1_l() { SET(1, R.HL.B.l); }
template <class T> void CPUCore<T>::set_2_a() { SET(2, R.AF.B.h); }
template <class T> void CPUCore<T>::set_2_b() { SET(2, R.BC.B.h); }
template <class T> void CPUCore<T>::set_2_c() { SET(2, R.BC.B.l); }
template <class T> void CPUCore<T>::set_2_d() { SET(2, R.DE.B.h); }
template <class T> void CPUCore<T>::set_2_e() { SET(2, R.DE.B.l); }
template <class T> void CPUCore<T>::set_2_h() { SET(2, R.HL.B.h); }
template <class T> void CPUCore<T>::set_2_l() { SET(2, R.HL.B.l); }
template <class T> void CPUCore<T>::set_3_a() { SET(3, R.AF.B.h); }
template <class T> void CPUCore<T>::set_3_b() { SET(3, R.BC.B.h); }
template <class T> void CPUCore<T>::set_3_c() { SET(3, R.BC.B.l); }
template <class T> void CPUCore<T>::set_3_d() { SET(3, R.DE.B.h); }
template <class T> void CPUCore<T>::set_3_e() { SET(3, R.DE.B.l); }
template <class T> void CPUCore<T>::set_3_h() { SET(3, R.HL.B.h); }
template <class T> void CPUCore<T>::set_3_l() { SET(3, R.HL.B.l); }
template <class T> void CPUCore<T>::set_4_a() { SET(4, R.AF.B.h); }
template <class T> void CPUCore<T>::set_4_b() { SET(4, R.BC.B.h); }
template <class T> void CPUCore<T>::set_4_c() { SET(4, R.BC.B.l); }
template <class T> void CPUCore<T>::set_4_d() { SET(4, R.DE.B.h); }
template <class T> void CPUCore<T>::set_4_e() { SET(4, R.DE.B.l); }
template <class T> void CPUCore<T>::set_4_h() { SET(4, R.HL.B.h); }
template <class T> void CPUCore<T>::set_4_l() { SET(4, R.HL.B.l); }
template <class T> void CPUCore<T>::set_5_a() { SET(5, R.AF.B.h); }
template <class T> void CPUCore<T>::set_5_b() { SET(5, R.BC.B.h); }
template <class T> void CPUCore<T>::set_5_c() { SET(5, R.BC.B.l); }
template <class T> void CPUCore<T>::set_5_d() { SET(5, R.DE.B.h); }
template <class T> void CPUCore<T>::set_5_e() { SET(5, R.DE.B.l); }
template <class T> void CPUCore<T>::set_5_h() { SET(5, R.HL.B.h); }
template <class T> void CPUCore<T>::set_5_l() { SET(5, R.HL.B.l); }
template <class T> void CPUCore<T>::set_6_a() { SET(6, R.AF.B.h); }
template <class T> void CPUCore<T>::set_6_b() { SET(6, R.BC.B.h); }
template <class T> void CPUCore<T>::set_6_c() { SET(6, R.BC.B.l); }
template <class T> void CPUCore<T>::set_6_d() { SET(6, R.DE.B.h); }
template <class T> void CPUCore<T>::set_6_e() { SET(6, R.DE.B.l); }
template <class T> void CPUCore<T>::set_6_h() { SET(6, R.HL.B.h); }
template <class T> void CPUCore<T>::set_6_l() { SET(6, R.HL.B.l); }
template <class T> void CPUCore<T>::set_7_a() { SET(7, R.AF.B.h); }
template <class T> void CPUCore<T>::set_7_b() { SET(7, R.BC.B.h); }
template <class T> void CPUCore<T>::set_7_c() { SET(7, R.BC.B.l); }
template <class T> void CPUCore<T>::set_7_d() { SET(7, R.DE.B.h); }
template <class T> void CPUCore<T>::set_7_e() { SET(7, R.DE.B.l); }
template <class T> void CPUCore<T>::set_7_h() { SET(7, R.HL.B.h); }
template <class T> void CPUCore<T>::set_7_l() { SET(7, R.HL.B.l); }

template <class T> inline void CPUCore<T>::SET_X(byte bit, byte& reg, word x)
{
	reg = RDMEM(x);
	SET(bit, reg); 
	T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::SET_X_(byte bit, byte& reg, word x)
{
	memptr.w = x; SET_X(bit, reg, x);
}
template <class T> inline void CPUCore<T>::SET_X_X(byte bit, byte& reg)
{
	SET_X_(bit, reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::SET_X_Y(byte bit, byte& reg)
{
	SET_X_(bit, reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::set_0_xhl()   { byte dummy; SET_X(0, dummy, R.HL.w); }
template <class T> void CPUCore<T>::set_1_xhl()   { byte dummy; SET_X(1, dummy, R.HL.w); }
template <class T> void CPUCore<T>::set_2_xhl()   { byte dummy; SET_X(2, dummy, R.HL.w); }
template <class T> void CPUCore<T>::set_3_xhl()   { byte dummy; SET_X(3, dummy, R.HL.w); }
template <class T> void CPUCore<T>::set_4_xhl()   { byte dummy; SET_X(4, dummy, R.HL.w); }
template <class T> void CPUCore<T>::set_5_xhl()   { byte dummy; SET_X(5, dummy, R.HL.w); }
template <class T> void CPUCore<T>::set_6_xhl()   { byte dummy; SET_X(6, dummy, R.HL.w); }
template <class T> void CPUCore<T>::set_7_xhl()   { byte dummy; SET_X(7, dummy, R.HL.w); }

template <class T> void CPUCore<T>::set_0_xix()   { byte dummy; SET_X_X(0, dummy); }
template <class T> void CPUCore<T>::set_1_xix()   { byte dummy; SET_X_X(1, dummy); }
template <class T> void CPUCore<T>::set_2_xix()   { byte dummy; SET_X_X(2, dummy); }
template <class T> void CPUCore<T>::set_3_xix()   { byte dummy; SET_X_X(3, dummy); }
template <class T> void CPUCore<T>::set_4_xix()   { byte dummy; SET_X_X(4, dummy); }
template <class T> void CPUCore<T>::set_5_xix()   { byte dummy; SET_X_X(5, dummy); }
template <class T> void CPUCore<T>::set_6_xix()   { byte dummy; SET_X_X(6, dummy); }
template <class T> void CPUCore<T>::set_7_xix()   { byte dummy; SET_X_X(7, dummy); }

template <class T> void CPUCore<T>::set_0_xiy()   { byte dummy; SET_X_Y(0, dummy); }
template <class T> void CPUCore<T>::set_1_xiy()   { byte dummy; SET_X_Y(1, dummy); }
template <class T> void CPUCore<T>::set_2_xiy()   { byte dummy; SET_X_Y(2, dummy); }
template <class T> void CPUCore<T>::set_3_xiy()   { byte dummy; SET_X_Y(3, dummy); }
template <class T> void CPUCore<T>::set_4_xiy()   { byte dummy; SET_X_Y(4, dummy); }
template <class T> void CPUCore<T>::set_5_xiy()   { byte dummy; SET_X_Y(5, dummy); }
template <class T> void CPUCore<T>::set_6_xiy()   { byte dummy; SET_X_Y(6, dummy); }
template <class T> void CPUCore<T>::set_7_xiy()   { byte dummy; SET_X_Y(7, dummy); }

template <class T> void CPUCore<T>::set_0_xix_a() { SET_X_X(0, R.AF.B.h); }
template <class T> void CPUCore<T>::set_0_xix_b() { SET_X_X(0, R.BC.B.h); }
template <class T> void CPUCore<T>::set_0_xix_c() { SET_X_X(0, R.BC.B.l); }
template <class T> void CPUCore<T>::set_0_xix_d() { SET_X_X(0, R.DE.B.h); }
template <class T> void CPUCore<T>::set_0_xix_e() { SET_X_X(0, R.DE.B.l); }
template <class T> void CPUCore<T>::set_0_xix_h() { SET_X_X(0, R.HL.B.h); }
template <class T> void CPUCore<T>::set_0_xix_l() { SET_X_X(0, R.HL.B.l); }
template <class T> void CPUCore<T>::set_1_xix_a() { SET_X_X(1, R.AF.B.h); }
template <class T> void CPUCore<T>::set_1_xix_b() { SET_X_X(1, R.BC.B.h); }
template <class T> void CPUCore<T>::set_1_xix_c() { SET_X_X(1, R.BC.B.l); }
template <class T> void CPUCore<T>::set_1_xix_d() { SET_X_X(1, R.DE.B.h); }
template <class T> void CPUCore<T>::set_1_xix_e() { SET_X_X(1, R.DE.B.l); }
template <class T> void CPUCore<T>::set_1_xix_h() { SET_X_X(1, R.HL.B.h); }
template <class T> void CPUCore<T>::set_1_xix_l() { SET_X_X(1, R.HL.B.l); }
template <class T> void CPUCore<T>::set_2_xix_a() { SET_X_X(2, R.AF.B.h); }
template <class T> void CPUCore<T>::set_2_xix_b() { SET_X_X(2, R.BC.B.h); }
template <class T> void CPUCore<T>::set_2_xix_c() { SET_X_X(2, R.BC.B.l); }
template <class T> void CPUCore<T>::set_2_xix_d() { SET_X_X(2, R.DE.B.h); }
template <class T> void CPUCore<T>::set_2_xix_e() { SET_X_X(2, R.DE.B.l); }
template <class T> void CPUCore<T>::set_2_xix_h() { SET_X_X(2, R.HL.B.h); }
template <class T> void CPUCore<T>::set_2_xix_l() { SET_X_X(2, R.HL.B.l); }
template <class T> void CPUCore<T>::set_3_xix_a() { SET_X_X(3, R.AF.B.h); }
template <class T> void CPUCore<T>::set_3_xix_b() { SET_X_X(3, R.BC.B.h); }
template <class T> void CPUCore<T>::set_3_xix_c() { SET_X_X(3, R.BC.B.l); }
template <class T> void CPUCore<T>::set_3_xix_d() { SET_X_X(3, R.DE.B.h); }
template <class T> void CPUCore<T>::set_3_xix_e() { SET_X_X(3, R.DE.B.l); }
template <class T> void CPUCore<T>::set_3_xix_h() { SET_X_X(3, R.HL.B.h); }
template <class T> void CPUCore<T>::set_3_xix_l() { SET_X_X(3, R.HL.B.l); }
template <class T> void CPUCore<T>::set_4_xix_a() { SET_X_X(4, R.AF.B.h); }
template <class T> void CPUCore<T>::set_4_xix_b() { SET_X_X(4, R.BC.B.h); }
template <class T> void CPUCore<T>::set_4_xix_c() { SET_X_X(4, R.BC.B.l); }
template <class T> void CPUCore<T>::set_4_xix_d() { SET_X_X(4, R.DE.B.h); }
template <class T> void CPUCore<T>::set_4_xix_e() { SET_X_X(4, R.DE.B.l); }
template <class T> void CPUCore<T>::set_4_xix_h() { SET_X_X(4, R.HL.B.h); }
template <class T> void CPUCore<T>::set_4_xix_l() { SET_X_X(4, R.HL.B.l); }
template <class T> void CPUCore<T>::set_5_xix_a() { SET_X_X(5, R.AF.B.h); }
template <class T> void CPUCore<T>::set_5_xix_b() { SET_X_X(5, R.BC.B.h); }
template <class T> void CPUCore<T>::set_5_xix_c() { SET_X_X(5, R.BC.B.l); }
template <class T> void CPUCore<T>::set_5_xix_d() { SET_X_X(5, R.DE.B.h); }
template <class T> void CPUCore<T>::set_5_xix_e() { SET_X_X(5, R.DE.B.l); }
template <class T> void CPUCore<T>::set_5_xix_h() { SET_X_X(5, R.HL.B.h); }
template <class T> void CPUCore<T>::set_5_xix_l() { SET_X_X(5, R.HL.B.l); }
template <class T> void CPUCore<T>::set_6_xix_a() { SET_X_X(6, R.AF.B.h); }
template <class T> void CPUCore<T>::set_6_xix_b() { SET_X_X(6, R.BC.B.h); }
template <class T> void CPUCore<T>::set_6_xix_c() { SET_X_X(6, R.BC.B.l); }
template <class T> void CPUCore<T>::set_6_xix_d() { SET_X_X(6, R.DE.B.h); }
template <class T> void CPUCore<T>::set_6_xix_e() { SET_X_X(6, R.DE.B.l); }
template <class T> void CPUCore<T>::set_6_xix_h() { SET_X_X(6, R.HL.B.h); }
template <class T> void CPUCore<T>::set_6_xix_l() { SET_X_X(6, R.HL.B.l); }
template <class T> void CPUCore<T>::set_7_xix_a() { SET_X_X(7, R.AF.B.h); }
template <class T> void CPUCore<T>::set_7_xix_b() { SET_X_X(7, R.BC.B.h); }
template <class T> void CPUCore<T>::set_7_xix_c() { SET_X_X(7, R.BC.B.l); }
template <class T> void CPUCore<T>::set_7_xix_d() { SET_X_X(7, R.DE.B.h); }
template <class T> void CPUCore<T>::set_7_xix_e() { SET_X_X(7, R.DE.B.l); }
template <class T> void CPUCore<T>::set_7_xix_h() { SET_X_X(7, R.HL.B.h); }
template <class T> void CPUCore<T>::set_7_xix_l() { SET_X_X(7, R.HL.B.l); }

template <class T> void CPUCore<T>::set_0_xiy_a() { SET_X_Y(0, R.AF.B.h); }
template <class T> void CPUCore<T>::set_0_xiy_b() { SET_X_Y(0, R.BC.B.h); }
template <class T> void CPUCore<T>::set_0_xiy_c() { SET_X_Y(0, R.BC.B.l); }
template <class T> void CPUCore<T>::set_0_xiy_d() { SET_X_Y(0, R.DE.B.h); }
template <class T> void CPUCore<T>::set_0_xiy_e() { SET_X_Y(0, R.DE.B.l); }
template <class T> void CPUCore<T>::set_0_xiy_h() { SET_X_Y(0, R.HL.B.h); }
template <class T> void CPUCore<T>::set_0_xiy_l() { SET_X_Y(0, R.HL.B.l); }
template <class T> void CPUCore<T>::set_1_xiy_a() { SET_X_Y(1, R.AF.B.h); }
template <class T> void CPUCore<T>::set_1_xiy_b() { SET_X_Y(1, R.BC.B.h); }
template <class T> void CPUCore<T>::set_1_xiy_c() { SET_X_Y(1, R.BC.B.l); }
template <class T> void CPUCore<T>::set_1_xiy_d() { SET_X_Y(1, R.DE.B.h); }
template <class T> void CPUCore<T>::set_1_xiy_e() { SET_X_Y(1, R.DE.B.l); }
template <class T> void CPUCore<T>::set_1_xiy_h() { SET_X_Y(1, R.HL.B.h); }
template <class T> void CPUCore<T>::set_1_xiy_l() { SET_X_Y(1, R.HL.B.l); }
template <class T> void CPUCore<T>::set_2_xiy_a() { SET_X_Y(2, R.AF.B.h); }
template <class T> void CPUCore<T>::set_2_xiy_b() { SET_X_Y(2, R.BC.B.h); }
template <class T> void CPUCore<T>::set_2_xiy_c() { SET_X_Y(2, R.BC.B.l); }
template <class T> void CPUCore<T>::set_2_xiy_d() { SET_X_Y(2, R.DE.B.h); }
template <class T> void CPUCore<T>::set_2_xiy_e() { SET_X_Y(2, R.DE.B.l); }
template <class T> void CPUCore<T>::set_2_xiy_h() { SET_X_Y(2, R.HL.B.h); }
template <class T> void CPUCore<T>::set_2_xiy_l() { SET_X_Y(2, R.HL.B.l); }
template <class T> void CPUCore<T>::set_3_xiy_a() { SET_X_Y(3, R.AF.B.h); }
template <class T> void CPUCore<T>::set_3_xiy_b() { SET_X_Y(3, R.BC.B.h); }
template <class T> void CPUCore<T>::set_3_xiy_c() { SET_X_Y(3, R.BC.B.l); }
template <class T> void CPUCore<T>::set_3_xiy_d() { SET_X_Y(3, R.DE.B.h); }
template <class T> void CPUCore<T>::set_3_xiy_e() { SET_X_Y(3, R.DE.B.l); }
template <class T> void CPUCore<T>::set_3_xiy_h() { SET_X_Y(3, R.HL.B.h); }
template <class T> void CPUCore<T>::set_3_xiy_l() { SET_X_Y(3, R.HL.B.l); }
template <class T> void CPUCore<T>::set_4_xiy_a() { SET_X_Y(4, R.AF.B.h); }
template <class T> void CPUCore<T>::set_4_xiy_b() { SET_X_Y(4, R.BC.B.h); }
template <class T> void CPUCore<T>::set_4_xiy_c() { SET_X_Y(4, R.BC.B.l); }
template <class T> void CPUCore<T>::set_4_xiy_d() { SET_X_Y(4, R.DE.B.h); }
template <class T> void CPUCore<T>::set_4_xiy_e() { SET_X_Y(4, R.DE.B.l); }
template <class T> void CPUCore<T>::set_4_xiy_h() { SET_X_Y(4, R.HL.B.h); }
template <class T> void CPUCore<T>::set_4_xiy_l() { SET_X_Y(4, R.HL.B.l); }
template <class T> void CPUCore<T>::set_5_xiy_a() { SET_X_Y(5, R.AF.B.h); }
template <class T> void CPUCore<T>::set_5_xiy_b() { SET_X_Y(5, R.BC.B.h); }
template <class T> void CPUCore<T>::set_5_xiy_c() { SET_X_Y(5, R.BC.B.l); }
template <class T> void CPUCore<T>::set_5_xiy_d() { SET_X_Y(5, R.DE.B.h); }
template <class T> void CPUCore<T>::set_5_xiy_e() { SET_X_Y(5, R.DE.B.l); }
template <class T> void CPUCore<T>::set_5_xiy_h() { SET_X_Y(5, R.HL.B.h); }
template <class T> void CPUCore<T>::set_5_xiy_l() { SET_X_Y(5, R.HL.B.l); }
template <class T> void CPUCore<T>::set_6_xiy_a() { SET_X_Y(6, R.AF.B.h); }
template <class T> void CPUCore<T>::set_6_xiy_b() { SET_X_Y(6, R.BC.B.h); }
template <class T> void CPUCore<T>::set_6_xiy_c() { SET_X_Y(6, R.BC.B.l); }
template <class T> void CPUCore<T>::set_6_xiy_d() { SET_X_Y(6, R.DE.B.h); }
template <class T> void CPUCore<T>::set_6_xiy_e() { SET_X_Y(6, R.DE.B.l); }
template <class T> void CPUCore<T>::set_6_xiy_h() { SET_X_Y(6, R.HL.B.h); }
template <class T> void CPUCore<T>::set_6_xiy_l() { SET_X_Y(6, R.HL.B.l); }
template <class T> void CPUCore<T>::set_7_xiy_a() { SET_X_Y(7, R.AF.B.h); }
template <class T> void CPUCore<T>::set_7_xiy_b() { SET_X_Y(7, R.BC.B.h); }
template <class T> void CPUCore<T>::set_7_xiy_c() { SET_X_Y(7, R.BC.B.l); }
template <class T> void CPUCore<T>::set_7_xiy_d() { SET_X_Y(7, R.DE.B.h); }
template <class T> void CPUCore<T>::set_7_xiy_e() { SET_X_Y(7, R.DE.B.l); }
template <class T> void CPUCore<T>::set_7_xiy_h() { SET_X_Y(7, R.HL.B.h); }
template <class T> void CPUCore<T>::set_7_xiy_l() { SET_X_Y(7, R.HL.B.l); }


// RL r
template <class T> inline void CPUCore<T>::RL(byte &reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | ((R.AF.B.l & C_FLAG) ? 0x01 : 0);
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::rl_a() { RL(R.AF.B.h); }
template <class T> void CPUCore<T>::rl_b() { RL(R.BC.B.h); }
template <class T> void CPUCore<T>::rl_c() { RL(R.BC.B.l); }
template <class T> void CPUCore<T>::rl_d() { RL(R.DE.B.h); }
template <class T> void CPUCore<T>::rl_e() { RL(R.DE.B.l); }
template <class T> void CPUCore<T>::rl_h() { RL(R.HL.B.h); }
template <class T> void CPUCore<T>::rl_l() { RL(R.HL.B.l); }

template <class T> inline void CPUCore<T>::RL_X(byte& reg, word x)
{
	reg = RDMEM(x);
	RL(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::RL_X_(byte& reg, word x)
{
	memptr.w = x; RL_X(reg, x);
}
template <class T> inline void CPUCore<T>::RL_X_X(byte& reg)
{
	RL_X_(reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::RL_X_Y(byte& reg)
{
	RL_X_(reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::rl_xhl() { byte dummy; RL_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::rl_xix  () { byte dummy; RL_X_X(dummy); }
template <class T> void CPUCore<T>::rl_xix_a() { RL_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::rl_xix_b() { RL_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::rl_xix_c() { RL_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::rl_xix_d() { RL_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::rl_xix_e() { RL_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::rl_xix_h() { RL_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::rl_xix_l() { RL_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::rl_xiy  () { byte dummy; RL_X_Y(dummy); }
template <class T> void CPUCore<T>::rl_xiy_a() { RL_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::rl_xiy_b() { RL_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::rl_xiy_c() { RL_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::rl_xiy_d() { RL_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::rl_xiy_e() { RL_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::rl_xiy_h() { RL_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::rl_xiy_l() { RL_X_Y(R.HL.B.l); }


// RLC r
template <class T> inline void CPUCore<T>::RLC(byte &reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | c;
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::rlc_a() { RLC(R.AF.B.h); }
template <class T> void CPUCore<T>::rlc_b() { RLC(R.BC.B.h); }
template <class T> void CPUCore<T>::rlc_c() { RLC(R.BC.B.l); }
template <class T> void CPUCore<T>::rlc_d() { RLC(R.DE.B.h); }
template <class T> void CPUCore<T>::rlc_e() { RLC(R.DE.B.l); }
template <class T> void CPUCore<T>::rlc_h() { RLC(R.HL.B.h); }
template <class T> void CPUCore<T>::rlc_l() { RLC(R.HL.B.l); }

template <class T> inline void CPUCore<T>::RLC_X(byte& reg, word x)
{
	reg = RDMEM(x);
	RLC(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::RLC_X_(byte& reg, word x)
{
	memptr.w = x; RLC_X(reg, x);
}
template <class T> inline void CPUCore<T>::RLC_X_X(byte& reg)
{
	RLC_X_(reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::RLC_X_Y(byte& reg)
{
	RLC_X_(reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::rlc_xhl() { byte dummy; RLC_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::rlc_xix  () { byte dummy; RLC_X_X(dummy); }
template <class T> void CPUCore<T>::rlc_xix_a() { RLC_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::rlc_xix_b() { RLC_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::rlc_xix_c() { RLC_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::rlc_xix_d() { RLC_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::rlc_xix_e() { RLC_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::rlc_xix_h() { RLC_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::rlc_xix_l() { RLC_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::rlc_xiy  () { byte dummy; RLC_X_Y(dummy); }
template <class T> void CPUCore<T>::rlc_xiy_a() { RLC_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::rlc_xiy_b() { RLC_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::rlc_xiy_c() { RLC_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::rlc_xiy_d() { RLC_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::rlc_xiy_e() { RLC_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::rlc_xiy_h() { RLC_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::rlc_xiy_l() { RLC_X_Y(R.HL.B.l); }


// RR r
template <class T> inline void CPUCore<T>::RR(byte &reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | ((R.AF.B.l & C_FLAG) ? 0x80 : 0);
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::rr_a() { RR(R.AF.B.h); }
template <class T> void CPUCore<T>::rr_b() { RR(R.BC.B.h); }
template <class T> void CPUCore<T>::rr_c() { RR(R.BC.B.l); }
template <class T> void CPUCore<T>::rr_d() { RR(R.DE.B.h); }
template <class T> void CPUCore<T>::rr_e() { RR(R.DE.B.l); }
template <class T> void CPUCore<T>::rr_h() { RR(R.HL.B.h); }
template <class T> void CPUCore<T>::rr_l() { RR(R.HL.B.l); }

template <class T> inline void CPUCore<T>::RR_X(byte& reg, word x)
{
	reg = RDMEM(x);
	RR(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::RR_X_(byte& reg, word x)
{
	memptr.w = x; RR_X(reg, x);
}
template <class T> inline void CPUCore<T>::RR_X_X(byte& reg)
{
	RR_X_(reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::RR_X_Y(byte& reg)
{
	RR_X_(reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::rr_xhl() { byte dummy; RR_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::rr_xix  () { byte dummy; RR_X_X(dummy); }
template <class T> void CPUCore<T>::rr_xix_a() { RR_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::rr_xix_b() { RR_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::rr_xix_c() { RR_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::rr_xix_d() { RR_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::rr_xix_e() { RR_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::rr_xix_h() { RR_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::rr_xix_l() { RR_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::rr_xiy  () { byte dummy; RR_X_Y(dummy); }
template <class T> void CPUCore<T>::rr_xiy_a() { RR_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::rr_xiy_b() { RR_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::rr_xiy_c() { RR_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::rr_xiy_d() { RR_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::rr_xiy_e() { RR_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::rr_xiy_h() { RR_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::rr_xiy_l() { RR_X_Y(R.HL.B.l); }


// RRC r
template <class T> inline void CPUCore<T>::RRC(byte &reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (c << 7);
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::rrc_a() { RRC(R.AF.B.h); }
template <class T> void CPUCore<T>::rrc_b() { RRC(R.BC.B.h); }
template <class T> void CPUCore<T>::rrc_c() { RRC(R.BC.B.l); }
template <class T> void CPUCore<T>::rrc_d() { RRC(R.DE.B.h); }
template <class T> void CPUCore<T>::rrc_e() { RRC(R.DE.B.l); }
template <class T> void CPUCore<T>::rrc_h() { RRC(R.HL.B.h); }
template <class T> void CPUCore<T>::rrc_l() { RRC(R.HL.B.l); }

template <class T> inline void CPUCore<T>::RRC_X(byte& reg, word x)
{
	reg = RDMEM(x);
	RRC(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::RRC_X_(byte& reg, word x)
{
	memptr.w = x; RRC_X(reg, x);
}
template <class T> inline void CPUCore<T>::RRC_X_X(byte& reg)
{
	RRC_X_(reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::RRC_X_Y(byte& reg)
{
	RRC_X_(reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::rrc_xhl() { byte dummy; RRC_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::rrc_xix  () { byte dummy; RRC_X_X(dummy); }
template <class T> void CPUCore<T>::rrc_xix_a() { RRC_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::rrc_xix_b() { RRC_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::rrc_xix_c() { RRC_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::rrc_xix_d() { RRC_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::rrc_xix_e() { RRC_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::rrc_xix_h() { RRC_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::rrc_xix_l() { RRC_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::rrc_xiy  () { byte dummy; RRC_X_Y(dummy); }
template <class T> void CPUCore<T>::rrc_xiy_a() { RRC_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::rrc_xiy_b() { RRC_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::rrc_xiy_c() { RRC_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::rrc_xiy_d() { RRC_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::rrc_xiy_e() { RRC_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::rrc_xiy_h() { RRC_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::rrc_xiy_l() { RRC_X_Y(R.HL.B.l); }


// SLA r
template <class T> inline void CPUCore<T>::SLA(byte &reg)
{
	byte c = reg >> 7;
	reg <<= 1;
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::sla_a() { SLA(R.AF.B.h); }
template <class T> void CPUCore<T>::sla_b() { SLA(R.BC.B.h); }
template <class T> void CPUCore<T>::sla_c() { SLA(R.BC.B.l); }
template <class T> void CPUCore<T>::sla_d() { SLA(R.DE.B.h); }
template <class T> void CPUCore<T>::sla_e() { SLA(R.DE.B.l); }
template <class T> void CPUCore<T>::sla_h() { SLA(R.HL.B.h); }
template <class T> void CPUCore<T>::sla_l() { SLA(R.HL.B.l); }

template <class T> inline void CPUCore<T>::SLA_X(byte& reg, word x)
{
	reg = RDMEM(x);
	SLA(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::SLA_X_(byte& reg, word x)
{
	memptr.w = x; SLA_X(reg, x);
}
template <class T> inline void CPUCore<T>::SLA_X_X(byte& reg)
{
	SLA_X_(reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::SLA_X_Y(byte& reg)
{
	SLA_X_(reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::sla_xhl() { byte dummy; SLA_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::sla_xix  () { byte dummy; SLA_X_X(dummy); }
template <class T> void CPUCore<T>::sla_xix_a() { SLA_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::sla_xix_b() { SLA_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::sla_xix_c() { SLA_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::sla_xix_d() { SLA_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::sla_xix_e() { SLA_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::sla_xix_h() { SLA_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::sla_xix_l() { SLA_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::sla_xiy  () { byte dummy; SLA_X_Y(dummy); }
template <class T> void CPUCore<T>::sla_xiy_a() { SLA_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::sla_xiy_b() { SLA_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::sla_xiy_c() { SLA_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::sla_xiy_d() { SLA_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::sla_xiy_e() { SLA_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::sla_xiy_h() { SLA_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::sla_xiy_l() { SLA_X_Y(R.HL.B.l); }


// SLL r
template <class T> inline void CPUCore<T>::SLL(byte &reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | 1;
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::sll_a() { SLL(R.AF.B.h); }
template <class T> void CPUCore<T>::sll_b() { SLL(R.BC.B.h); }
template <class T> void CPUCore<T>::sll_c() { SLL(R.BC.B.l); }
template <class T> void CPUCore<T>::sll_d() { SLL(R.DE.B.h); }
template <class T> void CPUCore<T>::sll_e() { SLL(R.DE.B.l); }
template <class T> void CPUCore<T>::sll_h() { SLL(R.HL.B.h); }
template <class T> void CPUCore<T>::sll_l() { SLL(R.HL.B.l); }

template <class T> inline void CPUCore<T>::SLL_X(byte& reg, word x)
{
	reg = RDMEM(x);
	SLL(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::SLL_X_(byte& reg, word x)
{
	memptr.w = x; SLL_X(reg, x);
}
template <class T> inline void CPUCore<T>::SLL_X_X(byte& reg) { SLL_X_(reg, R.IX.w + ofst); }
template <class T> inline void CPUCore<T>::SLL_X_Y(byte& reg) { SLL_X_(reg, R.IY.w + ofst); }

template <class T> void CPUCore<T>::sll_xhl() { byte dummy; SLL_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::sll_xix  () { byte dummy; SLL_X_X(dummy); }
template <class T> void CPUCore<T>::sll_xix_a() { SLL_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::sll_xix_b() { SLL_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::sll_xix_c() { SLL_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::sll_xix_d() { SLL_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::sll_xix_e() { SLL_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::sll_xix_h() { SLL_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::sll_xix_l() { SLL_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::sll_xiy  () { byte dummy; SLL_X_Y(dummy); }
template <class T> void CPUCore<T>::sll_xiy_a() { SLL_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::sll_xiy_b() { SLL_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::sll_xiy_c() { SLL_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::sll_xiy_d() { SLL_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::sll_xiy_e() { SLL_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::sll_xiy_h() { SLL_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::sll_xiy_l() { SLL_X_Y(R.HL.B.l); }


// SRA r
template <class T> inline void CPUCore<T>::SRA(byte &reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (reg & 0x80);
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::sra_a() { SRA(R.AF.B.h); }
template <class T> void CPUCore<T>::sra_b() { SRA(R.BC.B.h); }
template <class T> void CPUCore<T>::sra_c() { SRA(R.BC.B.l); }
template <class T> void CPUCore<T>::sra_d() { SRA(R.DE.B.h); }
template <class T> void CPUCore<T>::sra_e() { SRA(R.DE.B.l); }
template <class T> void CPUCore<T>::sra_h() { SRA(R.HL.B.h); }
template <class T> void CPUCore<T>::sra_l() { SRA(R.HL.B.l); }

template <class T> inline void CPUCore<T>::SRA_X(byte& reg, word x)
{
	reg = RDMEM(x);
	SRA(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::SRA_X_(byte& reg, word x)
{
	memptr.w = x; SRA_X(reg, x);
}
template <class T> inline void CPUCore<T>::SRA_X_X(byte& reg)
{
	SRA_X_(reg, R.IX.w + ofst);
}
template <class T> inline void CPUCore<T>::SRA_X_Y(byte& reg)
{
	SRA_X_(reg, R.IY.w + ofst);
}

template <class T> void CPUCore<T>::sra_xhl() { byte dummy; SRA_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::sra_xix  () { byte dummy; SRA_X_X(dummy); }
template <class T> void CPUCore<T>::sra_xix_a() { SRA_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::sra_xix_b() { SRA_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::sra_xix_c() { SRA_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::sra_xix_d() { SRA_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::sra_xix_e() { SRA_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::sra_xix_h() { SRA_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::sra_xix_l() { SRA_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::sra_xiy  () { byte dummy; SRA_X_Y(dummy); }
template <class T> void CPUCore<T>::sra_xiy_a() { SRA_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::sra_xiy_b() { SRA_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::sra_xiy_c() { SRA_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::sra_xiy_d() { SRA_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::sra_xiy_e() { SRA_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::sra_xiy_h() { SRA_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::sra_xiy_l() { SRA_X_Y(R.HL.B.l); }


// SRL R
template <class T> inline void CPUCore<T>::SRL(byte &reg)
{
	byte c = reg & 1;
	reg >>= 1;
	R.AF.B.l = ZSPXYTable[reg] | (c ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::srl_a() { SRL(R.AF.B.h); }
template <class T> void CPUCore<T>::srl_b() { SRL(R.BC.B.h); }
template <class T> void CPUCore<T>::srl_c() { SRL(R.BC.B.l); }
template <class T> void CPUCore<T>::srl_d() { SRL(R.DE.B.h); }
template <class T> void CPUCore<T>::srl_e() { SRL(R.DE.B.l); }
template <class T> void CPUCore<T>::srl_h() { SRL(R.HL.B.h); }
template <class T> void CPUCore<T>::srl_l() { SRL(R.HL.B.l); }

template <class T> inline void CPUCore<T>::SRL_X(byte& reg, word x)
{
	reg = RDMEM(x);
	SRL(reg); T::INC_DELAY();
	WRMEM(x, reg);
}
template <class T> inline void CPUCore<T>::SRL_X_(byte& reg, word x)
{
	memptr.w = x; SRL_X(reg, x);
}
template <class T> inline void CPUCore<T>::SRL_X_X(byte& reg) { SRL_X_(reg, R.IX.w + ofst); }
template <class T> inline void CPUCore<T>::SRL_X_Y(byte& reg) { SRL_X_(reg, R.IY.w + ofst); }

template <class T> void CPUCore<T>::srl_xhl() { byte dummy; SRL_X(dummy, R.HL.w); }

template <class T> void CPUCore<T>::srl_xix  () { byte dummy; SRL_X_X(dummy); }
template <class T> void CPUCore<T>::srl_xix_a() { SRL_X_X(R.AF.B.h); }
template <class T> void CPUCore<T>::srl_xix_b() { SRL_X_X(R.BC.B.h); }
template <class T> void CPUCore<T>::srl_xix_c() { SRL_X_X(R.BC.B.l); }
template <class T> void CPUCore<T>::srl_xix_d() { SRL_X_X(R.DE.B.h); }
template <class T> void CPUCore<T>::srl_xix_e() { SRL_X_X(R.DE.B.l); }
template <class T> void CPUCore<T>::srl_xix_h() { SRL_X_X(R.HL.B.h); }
template <class T> void CPUCore<T>::srl_xix_l() { SRL_X_X(R.HL.B.l); }

template <class T> void CPUCore<T>::srl_xiy  () { byte dummy; SRL_X_Y(dummy); }
template <class T> void CPUCore<T>::srl_xiy_a() { SRL_X_Y(R.AF.B.h); }
template <class T> void CPUCore<T>::srl_xiy_b() { SRL_X_Y(R.BC.B.h); }
template <class T> void CPUCore<T>::srl_xiy_c() { SRL_X_Y(R.BC.B.l); }
template <class T> void CPUCore<T>::srl_xiy_d() { SRL_X_Y(R.DE.B.h); }
template <class T> void CPUCore<T>::srl_xiy_e() { SRL_X_Y(R.DE.B.l); }
template <class T> void CPUCore<T>::srl_xiy_h() { SRL_X_Y(R.HL.B.h); }
template <class T> void CPUCore<T>::srl_xiy_l() { SRL_X_Y(R.HL.B.l); }


// RLA RLCA RRA RRCA
template <class T> void CPUCore<T>::rla()
{
	byte c = R.AF.B.l & C_FLAG;
	R.AF.B.l = (R.AF.B.l & (S_FLAG | Z_FLAG | P_FLAG)) |
	           ((R.AF.B.h & 0x80) ? C_FLAG : 0);
	R.AF.B.h = (R.AF.B.h << 1) | (c ? 1 : 0);
	R.AF.B.l |= R.AF.B.h & (X_FLAG | Y_FLAG);
}
template <class T> void CPUCore<T>::rlca()
{
	R.AF.B.h = (R.AF.B.h << 1) | (R.AF.B.h >> 7);
	R.AF.B.l = (R.AF.B.l & (S_FLAG | Z_FLAG | P_FLAG)) |
	           (R.AF.B.h & (Y_FLAG | X_FLAG | C_FLAG));
}
template <class T> void CPUCore<T>::rra()
{
	byte c = R.AF.B.l & C_FLAG;
	R.AF.B.l = (R.AF.B.l & (S_FLAG | Z_FLAG | P_FLAG)) |
	           ((R.AF.B.h & 0x01) ? C_FLAG : 0);
	R.AF.B.h = (R.AF.B.h >> 1) | (c ? 0x80 : 0);
	R.AF.B.l |= R.AF.B.h & (X_FLAG | Y_FLAG);
}
template <class T> void CPUCore<T>::rrca()
{
	R.AF.B.l = (R.AF.B.l & (S_FLAG | Z_FLAG | P_FLAG)) |
	           (R.AF.B.h &  C_FLAG);
	R.AF.B.h = (R.AF.B.h >> 1) | (R.AF.B.h << 7);
	R.AF.B.l |= R.AF.B.h & (X_FLAG | Y_FLAG);
}


// RLD
template <class T> void CPUCore<T>::rld()
{
	byte val = RDMEM(R.HL.w);
	memptr.w = R.HL.w + 1;
	T::RLD_DELAY();
	WRMEM(R.HL.w, (val << 4) | (R.AF.B.h & 0x0F));
	R.AF.B.h = (R.AF.B.h & 0xF0) | (val >> 4);
	R.AF.B.l = (R.AF.B.l & C_FLAG) | ZSPXYTable[R.AF.B.h];
}

// RRD
template <class T> void CPUCore<T>::rrd()
{
	byte val = RDMEM(R.HL.w);
	memptr.w = R.HL.w + 1;
	T::RLD_DELAY();
	WRMEM(R.HL.w, (val >> 4) | (R.AF.B.h << 4));
	R.AF.B.h = (R.AF.B.h & 0xF0) | (val & 0x0F);
	R.AF.B.l = (R.AF.B.l & C_FLAG) | ZSPXYTable[R.AF.B.h];
}


// PUSH ss
template <class T> inline void CPUCore<T>::PUSH2(z80regpair reg)
{
	WRMEM(--R.SP.w, reg.B.h);
	WRMEM(--R.SP.w, reg.B.l);
}
template <class T> inline void CPUCore<T>::PUSH(z80regpair reg)
{
	T::PUSH_DELAY();
	PUSH2(reg);
}
template <class T> void CPUCore<T>::push_af() { PUSH(R.AF); }
template <class T> void CPUCore<T>::push_bc() { PUSH(R.BC); }
template <class T> void CPUCore<T>::push_de() { PUSH(R.DE); }
template <class T> void CPUCore<T>::push_hl() { PUSH(R.HL); }
template <class T> void CPUCore<T>::push_ix() { PUSH(R.IX); }
template <class T> void CPUCore<T>::push_iy() { PUSH(R.IY); }


// POP ss
template <class T> inline void CPUCore<T>::POP(z80regpair& reg)
{
	reg.B.l = RDMEM(R.SP.w++);
	reg.B.h = RDMEM(R.SP.w++);
}
template <class T> void CPUCore<T>::pop_af() { POP(R.AF); }
template <class T> void CPUCore<T>::pop_bc() { POP(R.BC); }
template <class T> void CPUCore<T>::pop_de() { POP(R.DE); }
template <class T> void CPUCore<T>::pop_hl() { POP(R.HL); }
template <class T> void CPUCore<T>::pop_ix() { POP(R.IX); }
template <class T> void CPUCore<T>::pop_iy() { POP(R.IY); }


// CALL nn / CALL cc,nn
template <class T> inline void CPUCore<T>::CALL()
{
	memptr.B.l = RDMEM_OPCODE(R.PC.w++);
	memptr.B.h = RDMEM_OPCODE(R.PC.w++);
	T::SMALL_DELAY();
	PUSH2(R.PC);
	R.PC.w = memptr.w;
}
template <class T> inline void CPUCore<T>::SKIP_JP()
{
	memptr.B.l = RDMEM_OPCODE(R.PC.w++);
	memptr.B.h = RDMEM_OPCODE(R.PC.w++);
}
template <class T> void CPUCore<T>::call()    { CALL(); }
template <class T> void CPUCore<T>::call_c()  { if (C())  CALL(); else SKIP_JP(); }
template <class T> void CPUCore<T>::call_m()  { if (M())  CALL(); else SKIP_JP(); }
template <class T> void CPUCore<T>::call_nc() { if (NC()) CALL(); else SKIP_JP(); }
template <class T> void CPUCore<T>::call_nz() { if (NZ()) CALL(); else SKIP_JP(); }
template <class T> void CPUCore<T>::call_p()  { if (P())  CALL(); else SKIP_JP(); }
template <class T> void CPUCore<T>::call_pe() { if (PE()) CALL(); else SKIP_JP(); }
template <class T> void CPUCore<T>::call_po() { if (PO()) CALL(); else SKIP_JP(); }
template <class T> void CPUCore<T>::call_z()  { if (Z())  CALL(); else SKIP_JP(); }


// RST n
template <class T> inline void CPUCore<T>::RST(word x)
{
	PUSH(R.PC); memptr.w = x; R.PC.w = x;
}
template <class T> void CPUCore<T>::rst_00() { RST(0x00); }
template <class T> void CPUCore<T>::rst_08() { RST(0x08); }
template <class T> void CPUCore<T>::rst_10() { RST(0x10); }
template <class T> void CPUCore<T>::rst_18() { RST(0x18); }
template <class T> void CPUCore<T>::rst_20() { RST(0x20); }
template <class T> void CPUCore<T>::rst_28() { RST(0x28); }
template <class T> void CPUCore<T>::rst_30() { RST(0x30); }
template <class T> void CPUCore<T>::rst_38() { RST(0x38); }


// RET
template <class T> inline void CPUCore<T>::RET()
{
	R.PC.B.l = RDMEM(R.SP.w++);
	R.PC.B.h = RDMEM(R.SP.w++);
	memptr.w = R.PC.w;
}
template <class T> void CPUCore<T>::ret()    { RET(); }
template <class T> void CPUCore<T>::ret_c()  { T::SMALL_DELAY(); if (C())  RET(); }
template <class T> void CPUCore<T>::ret_m()  { T::SMALL_DELAY(); if (M())  RET(); }
template <class T> void CPUCore<T>::ret_nc() { T::SMALL_DELAY(); if (NC()) RET(); }
template <class T> void CPUCore<T>::ret_nz() { T::SMALL_DELAY(); if (NZ()) RET(); }
template <class T> void CPUCore<T>::ret_p()  { T::SMALL_DELAY(); if (P())  RET(); }
template <class T> void CPUCore<T>::ret_pe() { T::SMALL_DELAY(); if (PE()) RET(); }
template <class T> void CPUCore<T>::ret_po() { T::SMALL_DELAY(); if (PO()) RET(); }
template <class T> void CPUCore<T>::ret_z()  { T::SMALL_DELAY(); if (Z())  RET(); }

template <class T> void CPUCore<T>::reti()
{
	// same as retn
	R.IFF1 = R.nextIFF1 = R.IFF2;
	slowInstructions = 2;
	RET();
}
template <class T> void CPUCore<T>::retn()
{
	R.IFF1 = R.nextIFF1 = R.IFF2;
	slowInstructions = 2;
	RET(); 
}


// JP ss
template <class T> void CPUCore<T>::jp_hl() { R.PC.w = R.HL.w; }
template <class T> void CPUCore<T>::jp_ix() { R.PC.w = R.IX.w; }
template <class T> void CPUCore<T>::jp_iy() { R.PC.w = R.IY.w; }

// JP nn / JP cc,nn
template <class T> inline void CPUCore<T>::JP()
{
	memptr.B.l = RDMEM_OPCODE(R.PC.w++);
	memptr.B.h = RDMEM_OPCODE(R.PC.w);
	R.PC.w = memptr.w;
}
template <class T> void CPUCore<T>::jp()    { JP(); }
template <class T> void CPUCore<T>::jp_c()  { if (C())  JP(); else SKIP_JP(); }
template <class T> void CPUCore<T>::jp_m()  { if (M())  JP(); else SKIP_JP(); }
template <class T> void CPUCore<T>::jp_nc() { if (NC()) JP(); else SKIP_JP(); }
template <class T> void CPUCore<T>::jp_nz() { if (NZ()) JP(); else SKIP_JP(); }
template <class T> void CPUCore<T>::jp_p()  { if (P())  JP(); else SKIP_JP(); }
template <class T> void CPUCore<T>::jp_pe() { if (PE()) JP(); else SKIP_JP(); }
template <class T> void CPUCore<T>::jp_po() { if (PO()) JP(); else SKIP_JP(); }
template <class T> void CPUCore<T>::jp_z()  { if (Z())  JP(); else SKIP_JP(); }

// JR e
template <class T> inline void CPUCore<T>::JR()
{
	offset ofst = RDMEM_OPCODE(R.PC.w++);
	R.PC.w += ofst;
	memptr.w = R.PC.w;
	T::ADD_16_8_DELAY();
}
template <class T> inline void CPUCore<T>::SKIP_JR()
{
	byte dummy = RDMEM_OPCODE(R.PC.w++);
	dummy = dummy; // avoid warning
}
template <class T> void CPUCore<T>::jr()    { JR(); }
template <class T> void CPUCore<T>::jr_c()  { if (C())  JR(); else SKIP_JR(); }
template <class T> void CPUCore<T>::jr_nc() { if (NC()) JR(); else SKIP_JR(); }
template <class T> void CPUCore<T>::jr_nz() { if (NZ()) JR(); else SKIP_JR(); }
template <class T> void CPUCore<T>::jr_z()  { if (Z())  JR(); else SKIP_JR(); }

// DJNZ e
template <class T> void CPUCore<T>::djnz()
{
	T::SMALL_DELAY();
	if (--R.BC.B.h) JR(); else SKIP_JR();
}

// EX (SP),ss
template <class T> inline void CPUCore<T>::EX_SP(z80regpair& reg)
{
	memptr.B.l = RDMEM(R.SP.w++);
	memptr.B.h = RDMEM(R.SP.w);
	T::SMALL_DELAY();
	WRMEM(R.SP.w--, reg.B.h);
	reg.B.h = memptr.B.h;
	WRMEM(R.SP.w, reg.B.l);
	reg.B.l = memptr.B.l;
	T::EX_SP_HL_DELAY();
}
template <class T> void CPUCore<T>::ex_xsp_hl() { EX_SP(R.HL); }
template <class T> void CPUCore<T>::ex_xsp_ix() { EX_SP(R.IX); }
template <class T> void CPUCore<T>::ex_xsp_iy() { EX_SP(R.IY); }


// IN r,(c)
template <class T> inline void CPUCore<T>::IN(byte& reg)
{
	reg = READ_PORT(R.BC.w);
	R.AF.B.l = (R.AF.B.l & C_FLAG) |
	           ZSPXYTable[reg];
}
template <class T> void CPUCore<T>::in_a_c() { IN(R.AF.B.h); }
template <class T> void CPUCore<T>::in_b_c() { IN(R.BC.B.h); }
template <class T> void CPUCore<T>::in_c_c() { IN(R.BC.B.l); }
template <class T> void CPUCore<T>::in_d_c() { IN(R.DE.B.h); }
template <class T> void CPUCore<T>::in_e_c() { IN(R.DE.B.l); }
template <class T> void CPUCore<T>::in_h_c() { IN(R.HL.B.h); }
template <class T> void CPUCore<T>::in_l_c() { IN(R.HL.B.l); }
template <class T> void CPUCore<T>::in_0_c() { byte dummy; IN(dummy); } // discard result

// IN a,(n)
template <class T> void CPUCore<T>::in_a_byte()
{
	z80regpair y;
	y.B.l = RDMEM_OPCODE(R.PC.w++);
	y.B.h = R.AF.B.h;
	R.AF.B.h = READ_PORT(y.w);
}


// OUT (c),r
template <class T> inline void CPUCore<T>::OUT(byte val)
{
	WRITE_PORT(R.BC.w, val);
}
template <class T> void CPUCore<T>::out_c_a()   { OUT(R.AF.B.h); }
template <class T> void CPUCore<T>::out_c_b()   { OUT(R.BC.B.h); }
template <class T> void CPUCore<T>::out_c_c()   { OUT(R.BC.B.l); }
template <class T> void CPUCore<T>::out_c_d()   { OUT(R.DE.B.h); }
template <class T> void CPUCore<T>::out_c_e()   { OUT(R.DE.B.l); }
template <class T> void CPUCore<T>::out_c_h()   { OUT(R.HL.B.h); }
template <class T> void CPUCore<T>::out_c_l()   { OUT(R.HL.B.l); }
template <class T> void CPUCore<T>::out_c_0()   { OUT(0);        }

// OUT (n),a
template <class T> void CPUCore<T>::out_byte_a()
{
	z80regpair y;
	y.B.l = RDMEM_OPCODE(R.PC.w++);
	y.B.h = R.AF.B.h;
	WRITE_PORT(y.w, R.AF.B.h);
}


// block CP
template <class T> inline void CPUCore<T>::BLOCK_CP(bool increase, bool repeat)
{
	byte val = RDMEM(R.HL.w);
	T::BLOCK_DELAY();
	byte res = R.AF.B.h - val;
	if (increase) R.HL.w++; else R.HL.w--; 
	R.BC.w--;
	R.AF.B.l = (R.AF.B.l & C_FLAG) |
		   ((R.AF.B.h ^ val ^ res) & H_FLAG) |
		   ZSTable[res] |
		   N_FLAG;
	if (R.AF.B.l & H_FLAG) res -= 1;
	if (res & 0x02) R.AF.B.l |= Y_FLAG; // bit 1 -> flag 5
	if (res & 0x08) R.AF.B.l |= X_FLAG; // bit 3 -> flag 3
	if (R.BC.w)     R.AF.B.l |= V_FLAG;
	if (repeat && R.BC.w && !(R.AF.B.l & Z_FLAG)) {
		T::BLOCK_DELAY(); R.PC.w -= 2;
	}
}
template <class T> void CPUCore<T>::cpd()  { BLOCK_CP(false, false); }
template <class T> void CPUCore<T>::cpi()  { BLOCK_CP(true,  false); }
template <class T> void CPUCore<T>::cpdr() { BLOCK_CP(false, true ); }
template <class T> void CPUCore<T>::cpir() { BLOCK_CP(true,  true ); }


// block LD
template <class T> inline void CPUCore<T>::BLOCK_LD(bool increase, bool repeat)
{
	byte val = RDMEM(R.HL.w);
	WRMEM(R.DE.w, val);
	T::LDI_DELAY();
	R.AF.B.l &= S_FLAG | Z_FLAG | C_FLAG;
	if ((R.AF.B.h + val) & 0x02) R.AF.B.l |= Y_FLAG;	// bit 1 -> flag 5
	if ((R.AF.B.h + val) & 0x08) R.AF.B.l |= X_FLAG;	// bit 3 -> flag 3
	if (increase) { R.HL.w++; R.DE.w++; } else { R.HL.w--; R.DE.w--; }
	R.BC.w--;
	if (R.BC.w) R.AF.B.l |= V_FLAG;
	if (repeat && R.BC.w) { T::BLOCK_DELAY(); R.PC.w -= 2; }
}
template <class T> void CPUCore<T>::ldd()  { BLOCK_LD(false, false); }
template <class T> void CPUCore<T>::ldi()  { BLOCK_LD(true,  false); }
template <class T> void CPUCore<T>::lddr() { BLOCK_LD(false, true ); }
template <class T> void CPUCore<T>::ldir() { BLOCK_LD(true,  true ); }


// block IN
template <class T> inline void CPUCore<T>::BLOCK_IN(bool increase, bool repeat)
{
	T::SMALL_DELAY();
	byte val = READ_PORT(R.BC.w);
	R.BC.B.h--;
	WRMEM(R.HL.w, val);
	if (increase) R.HL.w++; else R.HL.w--; 
	R.AF.B.l = ZSTable[R.BC.B.h];
	if (val & S_FLAG) R.AF.B.l |= N_FLAG;
	int k = val + ((R.BC.B.l + (increase ? 1 : -1)) & 0xFF);
	if (k & 0x100) R.AF.B.l |= H_FLAG | C_FLAG;
	R.AF.B.l |= ZSPXYTable[(k & 0x07) ^ R.BC.B.h] & P_FLAG;
	if (repeat && R.BC.B.h) { T::BLOCK_DELAY(); R.PC.w -= 2; }
}
template <class T> void CPUCore<T>::ind()  { BLOCK_IN(false, false); }
template <class T> void CPUCore<T>::ini()  { BLOCK_IN(true,  false); }
template <class T> void CPUCore<T>::indr() { BLOCK_IN(false, true ); }
template <class T> void CPUCore<T>::inir() { BLOCK_IN(true,  true ); }


// block OUT
template <class T> inline void CPUCore<T>::BLOCK_OUT(bool increase, bool repeat)
{
	T::SMALL_DELAY();
	byte val = RDMEM(R.HL.w);
	if (increase) R.HL.w++; else R.HL.w--; 
	R.BC.B.h--;
	WRITE_PORT(R.BC.w, val);
	R.AF.B.l = ZSXYTable[R.BC.B.h];
	if (val & S_FLAG) R.AF.B.l |= N_FLAG;
	int k = val + R.HL.B.l;
	if (k & 0x100) R.AF.B.l |= H_FLAG | C_FLAG;
	R.AF.B.l |= ZSPXYTable[(k & 0x07) ^ R.BC.B.h] & P_FLAG;
	if (repeat && R.BC.B.h) { T::BLOCK_DELAY(); R.PC.w -= 2; }
}
template <class T> void CPUCore<T>::outd() { BLOCK_OUT(false, false); }
template <class T> void CPUCore<T>::outi() { BLOCK_OUT(true,  false); }
template <class T> void CPUCore<T>::otdr() { BLOCK_OUT(false, true ); }
template <class T> void CPUCore<T>::otir() { BLOCK_OUT(true,  true ); }


// various
template <class T> void CPUCore<T>::nop() { }
template <class T> void CPUCore<T>::ccf()
{
	R.AF.B.l = ((R.AF.B.l & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG) |
	            ((R.AF.B.l & C_FLAG) ? H_FLAG : 0)) |
	            (R.AF.B.h & (X_FLAG | Y_FLAG))                  ) ^ C_FLAG;
}
template <class T> void CPUCore<T>::cpl()
{
	R.AF.B.h ^= 0xFF;
	R.AF.B.l = (R.AF.B.l & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	           H_FLAG | N_FLAG |
		   (R.AF.B.h & (X_FLAG | Y_FLAG));

}
template <class T> void CPUCore<T>::daa()
{
	int i = R.AF.B.h;
	if (R.AF.B.l & C_FLAG) i |= 0x100;
	if (R.AF.B.l & H_FLAG) i |= 0x200;
	if (R.AF.B.l & N_FLAG) i |= 0x400;
	R.AF.w = DAATable[i];
}
template <class T> void CPUCore<T>::neg()
{
	 byte i = R.AF.B.h;
	 R.AF.B.h = 0;
	 SUB(i);
}
template <class T> void CPUCore<T>::scf()
{ 
	R.AF.B.l = (R.AF.B.l & (S_FLAG | Z_FLAG | P_FLAG)) |
	           C_FLAG |
	           (R.AF.B.h & (X_FLAG | Y_FLAG));
}

template <class T> void CPUCore<T>::ex_af_af()
{
	word i = R.AF.w;
	R.AF.w = R.AF2.w;
	R.AF2.w = i;
}
template <class T> void CPUCore<T>::ex_de_hl()
{
	word i = R.DE.w;
	R.DE.w = R.HL.w;
	R.HL.w = i;
}
template <class T> void CPUCore<T>::exx()
{
	word i;
	i = R.BC.w; R.BC.w = R.BC2.w; R.BC2.w = i;
	i = R.DE.w; R.DE.w = R.DE2.w; R.DE2.w = i;
	i = R.HL.w; R.HL.w = R.HL2.w; R.HL2.w = i;
}

template <class T> void CPUCore<T>::di()
{
	R.IFF1 = R.nextIFF1 = R.IFF2 = false;
}
template <class T> void CPUCore<T>::ei()
{
	R.IFF1 = false;		// no ints after this instruction
	R.nextIFF1 = true;	// but allow them after next instruction
	R.IFF2 = true;
	slowInstructions = 2;
}
template <class T> void CPUCore<T>::halt()
{
	R.HALT = true; slowInstructions = 2;
}
template <class T> void CPUCore<T>::im_0() { R.IM = 0; }
template <class T> void CPUCore<T>::im_1() { R.IM = 1; }
template <class T> void CPUCore<T>::im_2() { R.IM = 2; }

// LD A,I/R
template <class T> void CPUCore<T>::ld_a_i()
{
	T::SMALL_DELAY();
	R.AF.B.h = R.I;
	R.AF.B.l = (R.AF.B.l & C_FLAG) |
	           ZSXYTable[R.AF.B.h] |
		   (R.IFF2 ? V_FLAG : 0);
}
template <class T> void CPUCore<T>::ld_a_r()
{
	T::SMALL_DELAY();
	R.AF.B.h = (R.R & 0x7f) | (R.R2 & 0x80);
	R.AF.B.l = (R.AF.B.l & C_FLAG) |
	           ZSXYTable[R.AF.B.h] |
		   (R.IFF2 ? V_FLAG : 0);
}

// LD I/R,A
template <class T> void CPUCore<T>::ld_i_a()
{
	T::SMALL_DELAY();
	R.I = R.AF.B.h;
}
template <class T> void CPUCore<T>::ld_r_a()
{
	T::SMALL_DELAY();
	R.R = R.R2 = R.AF.B.h;
}

// MULUB A,r
template <class T> inline void CPUCore<T>::MULUB(byte reg)
{
	// TODO check flags
	T::clock += 12;
	R.HL.w = (word)R.AF.B.h * reg;
	R.AF.B.l = (R.AF.B.l & (N_FLAG | H_FLAG)) |
	           (R.HL.w ? 0 : Z_FLAG) |
		   ((R.HL.w & 0x8000) ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::mulub_a_xhl() { } // TODO
template <class T> void CPUCore<T>::mulub_a_a()   { } // TODO
template <class T> void CPUCore<T>::mulub_a_b()   { MULUB(R.BC.B.h); }
template <class T> void CPUCore<T>::mulub_a_c()   { MULUB(R.BC.B.l); }
template <class T> void CPUCore<T>::mulub_a_d()   { MULUB(R.DE.B.h); }
template <class T> void CPUCore<T>::mulub_a_e()   { MULUB(R.DE.B.l); }
template <class T> void CPUCore<T>::mulub_a_h()   { } // TODO
template <class T> void CPUCore<T>::mulub_a_l()   { } // TODO

// MULUW HL,ss
template <class T> inline void CPUCore<T>::MULUW(word reg)
{
	// TODO check flags
	T::clock += 34;
	unsigned long res = (unsigned long)R.HL.w * reg;
	R.DE.w = res >> 16;
	R.HL.w = res & 0xffff;
	R.AF.B.l = (R.AF.B.l & (N_FLAG | H_FLAG)) |
	           (res ? 0 : Z_FLAG) |
		   ((res & 0x80000000) ? C_FLAG : 0);
}
template <class T> void CPUCore<T>::muluw_hl_bc() { MULUW(R.BC.w); }
template <class T> void CPUCore<T>::muluw_hl_de() { } // TODO
template <class T> void CPUCore<T>::muluw_hl_hl() { } // TODO
template <class T> void CPUCore<T>::muluw_hl_sp() { MULUW(R.SP.w); }


// prefixes
template <class T> void CPUCore<T>::dd_cb()
{
	ofst = RDMEM_OPCODE(R.PC.w++);
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	T::DD_CB_DELAY();
	(this->*opcode_dd_cb[opcode])();
}

template <class T> void CPUCore<T>::fd_cb()
{
	ofst = RDMEM_OPCODE(R.PC.w++);
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	T::DD_CB_DELAY();
	(this->*opcode_fd_cb[opcode])();
}

template <class T> void CPUCore<T>::cb()
{
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	M1Cycle();
	(this->*opcode_cb[opcode])();
}

template <class T> void CPUCore<T>::ed()
{
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	M1Cycle();
	(this->*opcode_ed[opcode])();
}

template <class T> void CPUCore<T>::dd()
{
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	if ((opcode != 0xCB) && (opcode != 0xDD) && (opcode != 0xFD)) {
		M1Cycle();
	} else {
		T::SMALL_DELAY();
	}
	(this->*opcode_dd[opcode])();
}
template <class T> void CPUCore<T>::dd2()
{
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	T::SMALL_DELAY();
	(this->*opcode_dd[opcode])();
}

template <class T> void CPUCore<T>::fd()
{
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	if ((opcode != 0xCB) && (opcode != 0xDD) && (opcode != 0xFD)) {
		M1Cycle();
	} else {
		T::SMALL_DELAY();
	}
	(this->*opcode_fd[opcode])();
}
template <class T> void CPUCore<T>::fd2()
{
	byte opcode = RDMEM_OPCODE(R.PC.w++);
	T::SMALL_DELAY();
	(this->*opcode_fd[opcode])();
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

} // namespace openmsx

