// $Id$

#include "MSXCPUInterface.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "CliComm.hh"
#include "Event.hh"
#include "EventDistributor.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "Dasm.hh"
#include "CPUCore.hh"
#include "Z80.hh"
#include "R800.hh"
#include "StringOp.hh"
#include "likely.hh"
#include <iomanip>
#include <iostream>
#include <cassert>

using std::string;

namespace openmsx {

template <class T> CPUCore<T>::CPUCore(
		MSXMotherBoard& motherboard_, const string& name,
		const BooleanSetting& traceSetting_, const EmuTime& time)
	: T(time)
	, nmiEdge(false)
	, NMIStatus(0)
	, IRQStatus(0)
	, needReset(false)
	, interface(NULL)
	, motherboard(motherboard_)
	, scheduler(motherboard.getScheduler())
	, freqLocked(new BooleanSetting(motherboard.getCommandController(),
	        name + "_freq_locked",
	        "real (locked) or custom (unlocked) " + name + " frequency",
	        true))
	, freqValue(new IntegerSetting(motherboard.getCommandController(),
	        name + "_freq",
	        "custom " + name + " frequency (only valid when unlocked)",
	        T::CLOCK_FREQ, 1000000, 100000000))
	, freq(T::CLOCK_FREQ)
	, traceSetting(traceSetting_)
{
	if (freqLocked->getValue()) {
		// locked
		T::clock.setFreq(T::CLOCK_FREQ);
	} else {
		// unlocked
		T::clock.setFreq(freqValue->getValue());
	}

	freqLocked->addListener(this);
	freqValue->addListener(this);
	doReset(time);
}

template <class T> CPUCore<T>::~CPUCore()
{
	freqValue->removeListener(this);
	freqLocked->removeListener(this);
}

template <class T> void CPUCore<T>::setInterface(MSXCPUInterface* interf)
{
	interface = interf;
}

template <class T> void CPUCore<T>::warp(const EmuTime& time)
{
	assert(T::clock.getTime() < time);
	T::clock.reset(time);
}

template <class T> const EmuTime& CPUCore<T>::getCurrentTime() const
{
	return T::clock.getTime();
}

template <class T> void CPUCore<T>::invalidateMemCache(word start, unsigned size)
{
	unsigned first = start / CACHE_LINE_SIZE;
	unsigned num = (size + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
	memset(&readCacheLine  [first], 0, num * sizeof(byte*)); // NULL
	memset(&writeCacheLine [first], 0, num * sizeof(byte*)); //
	memset(&readCacheTried [first], 0, num * sizeof(bool));  // FALSE
	memset(&writeCacheTried[first], 0, num * sizeof(bool));  //
}

template <class T> CPU::CPURegs& CPUCore<T>::getRegisters()
{
	return R;
}

template <class T> void CPUCore<T>::scheduleReset()
{
	needReset = true;
	exitCPULoop();
}

template <class T> void CPUCore<T>::doReset(const EmuTime& time)
{
	// AF and SP are 0xFFFF
	// PC, R, IFF1, IFF2, HALT and IM are 0x0
	// all others are random
	R.AF  = 0xFFFF;
	R.BC  = 0xFFFF;
	R.DE  = 0xFFFF;
	R.HL  = 0xFFFF;
	R.IX  = 0xFFFF;
	R.IY  = 0xFFFF;
	R.PC  = 0x0000;
	R.SP  = 0xFFFF;
	R.AF2 = 0xFFFF;
	R.BC2 = 0xFFFF;
	R.DE2 = 0xFFFF;
	R.HL2 = 0xFFFF;
	R.nextIFF1 = false;
	R.IFF1     = false;
	R.IFF2     = false;
	R.HALT     = false;
	R.IM = 0;
	R.I  = 0x00;
	R.R  = 0x00;
	R.R2 = 0;
	memptr = 0xFFFF;
	invalidateMemCache(0x0000, 0x10000);

	assert(T::clock.getTime() <= time);
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
	T::clock.advance(time);
}

template <class T> void CPUCore<T>::doBreak2()
{
	assert(!breaked);
	breaked = true;

	motherboard.block();

	motherboard.getCliComm().update(CliComm::BREAK, "pc",
	                           "0x" + StringOp::toHexString(R.PC, 4));
	Event* breakEvent = new SimpleEvent<OPENMSX_BREAK_EVENT>();
	motherboard.getEventDistributor().distributeEvent(breakEvent);
}

template <class T> void CPUCore<T>::doStep()
{
	if (breaked) {
		breaked = false;
		step = true;
		motherboard.unblock();
	}
}

static inline char toHex(byte x)
{
	return (x < 10) ? (x + '0') : (x - 10 + 'A');
}
static void toHex(byte x, char* buf)
{
	buf[0] = toHex(x / 16);
	buf[1] = toHex(x & 15);
}

template <class T> void CPUCore<T>::disasmCommand(
	const std::vector<TclObject*>& tokens,
	TclObject& result) const
{
	word address = (tokens.size() < 3) ? R.PC : tokens[2]->getInt();
	byte outBuf[4];
	std::string dasmOutput;
	int len = dasm(*interface, address, outBuf, dasmOutput,
	               T::clock.getTime());
	result.addListElement(dasmOutput);
	char tmp[3]; tmp[2] = 0;
	for (int i = 0; i < len; ++i) {
		toHex(outBuf[i], tmp);
		result.addListElement(tmp);
	}
}

template <class T> void CPUCore<T>::doContinue()
{
	if (breaked) {
		breaked = false;
		continued = true;
		motherboard.getCliComm().update(CliComm::RESUME, "pc",
	                                   "0x" + StringOp::toHexString(R.PC, 4));
		motherboard.unblock();
	}
}

template <class T> void CPUCore<T>::doBreak()
{
	if (!breaked) {
		step = true;
		exitCPULoop();
	}
}

template <class T> void CPUCore<T>::update(const Setting* setting)
{
	if (setting == freqLocked.get()) {
		if (freqLocked->getValue()) {
			// locked
			T::clock.setFreq(freq);
		} else {
			// unlocked
			T::clock.setFreq(freqValue->getValue());
		}
	} else if (setting == freqValue.get()) {
		if (!freqLocked->getValue()) {
			T::clock.setFreq(freqValue->getValue());
		}
	} else {
		assert(false);
	}
}

template <class T> void CPUCore<T>::setFreq(unsigned freq_)
{
	freq = freq_;
	if (freqLocked->getValue()) {
		// locked
		T::clock.setFreq(freq);
	}
}


template <class T> inline byte CPUCore<T>::READ_PORT(word port)
{
	memptr = port + 1;
	T::PRE_IO(port);
	scheduler.schedule(T::clock.getTime());
	byte result = interface->readIO(port, T::clock.getTime());
	T::POST_IO(port);
	return result;
}

template <class T> inline void CPUCore<T>::WRITE_PORT(word port, byte value)
{
	memptr = port + 1;
	T::PRE_IO(port);
	scheduler.schedule(T::clock.getTime());
	interface->writeIO(port, value, T::clock.getTime());
	T::POST_IO(port);
}

template <class T> inline byte CPUCore<T>::RDMEM_common(word address)
{
	int line = address >> CACHE_LINE_BITS;
	if (likely(readCacheLine[line] != NULL)) {
		// cached, fast path
		T::POST_MEM(address);
		return readCacheLine[line][address&CACHE_LINE_LOW];
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
			T::POST_MEM(address);
			return readCacheLine[line][address&CACHE_LINE_LOW];
		}
	}
	// uncacheable
	scheduler.schedule(T::clock.getTime());
	byte result = interface->readMem(address, T::clock.getTime());
	T::POST_MEM(address);
	return result;
}

template <class T> inline void CPUCore<T>::WRMEM_common(word address, byte value)
{
	int line = address >> CACHE_LINE_BITS;
	if (likely(writeCacheLine[line] != NULL)) {
		// cached, fast path
		T::POST_MEM(address);
		writeCacheLine[line][address&CACHE_LINE_LOW] = value;
	} else {
		WRMEMslow(address, value);	// not inlined
	}
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
			T::POST_MEM(address);
			writeCacheLine[line][address&CACHE_LINE_LOW] = value;
			return;
		}
	}
	// uncacheable
	scheduler.schedule(T::clock.getTime());
	interface->writeMem(address, value, T::clock.getTime());
	T::POST_MEM(address);
}

template <class T> inline byte CPUCore<T>::RDMEM_OPCODE(word address)
{
	T::PRE_RDMEM_OPCODE(address);
	return RDMEM_common(address);
}
template <class T> inline word CPUCore<T>::RD_WORD_PC()
{
	word res = RDMEM_OPCODE(R.PC + 0);
	res     += RDMEM_OPCODE(R.PC + 1) << 8;
	R.PC += 2;
	return res;
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
	PUSH(R.PC);
	R.HALT = false;
	R.IFF1 = R.nextIFF1 = false;
	R.PC = 0x0066;
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
	PUSH(R.PC);
	word x = interface->dataBus() | (R.I << 8);
	R.PC  = RDMEM(x + 0);
	R.PC += RDMEM(x + 1) << 8;
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
	byte opcode = RDMEM_OPCODE(R.PC++);
	executeInstruction1(opcode);
}

template <class T> inline void CPUCore<T>::cpuTracePre()
{
	start_pc = R.PC;
}
template <class T> inline void CPUCore<T>::cpuTracePost()
{
	if (traceSetting.getValue()) {
		byte opbuf[4];
		string dasmOutput;
		dasm(*interface, start_pc, opbuf, dasmOutput, T::clock.getTime());
		std::cout << std::setfill('0') << std::hex << std::setw(4) << start_pc
		     << " : " << dasmOutput
		     << " AF=" << std::setw(4) << R.AF
		     << " BC=" << std::setw(4) << R.BC
		     << " DE=" << std::setw(4) << R.DE
		     << " HL=" << std::setw(4) << R.HL
		     << " IX=" << std::setw(4) << R.IX
		     << " IY=" << std::setw(4) << R.IY
		     << " SP=" << std::setw(4) << R.SP
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
	if (unlikely(nmiEdge)) {
		nmiEdge = false;
		nmi();	// NMI occured
	} else if (unlikely(R.IFF1 && IRQStatus)) {
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
	} else if (unlikely(R.HALT || paused)) {
		// in halt mode
		uint64 ticks = T::clock.getTicksTillUp(scheduler.getNext());
		int hltStates = T::haltStates();
		int halts = (ticks + hltStates - 1) / hltStates;	// rounded up
		R.R += halts;
		T::clock += halts * hltStates;
		slowInstructions = 2;
	} else {
		R.IFF1 = R.nextIFF1;
		cpuTracePre();
		executeFast();
		cpuTracePost();
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
		executeSlow();
		--slowInstructions;
		if (step) {
			step = false;
			doBreak2();
			return;
		}
	}

	if (!anyBreakPoints() && !traceSetting.getValue()) {
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
			if (unlikely(hitBreakPoint(R))) {
				doBreak2();
				return;
			} else {
				if (slowInstructions == 0) {
					scheduler.schedule(T::clock.getTime());
					cpuTracePre();
					executeFast();
					cpuTracePost();
				} else {
					slowInstructions--;
					scheduler.schedule(T::clock.getTime());
					executeSlow();
				}
			}
		}
	}
	if (needReset) {
		needReset = false;
		motherboard.doReset(T::clock.getTime());
	}
}


// conditions
template <class T> inline bool CPUCore<T>::C()  { return R.getF() & C_FLAG; }
template <class T> inline bool CPUCore<T>::NC() { return !C(); }
template <class T> inline bool CPUCore<T>::Z()  { return R.getF() & Z_FLAG; }
template <class T> inline bool CPUCore<T>::NZ() { return !Z(); }
template <class T> inline bool CPUCore<T>::M()  { return R.getF() & S_FLAG; }
template <class T> inline bool CPUCore<T>::P()  { return !M(); }
template <class T> inline bool CPUCore<T>::PE() { return R.getF() & V_FLAG; }
template <class T> inline bool CPUCore<T>::PO() { return !PE(); }


// LD r,r
template <class T> void CPUCore<T>::ld_a_a()     { }
template <class T> void CPUCore<T>::ld_a_b()     { R.setA( R.getB()); }
template <class T> void CPUCore<T>::ld_a_c()     { R.setA( R.getC()); }
template <class T> void CPUCore<T>::ld_a_d()     { R.setA( R.getD()); }
template <class T> void CPUCore<T>::ld_a_e()     { R.setA( R.getE()); }
template <class T> void CPUCore<T>::ld_a_h()     { R.setA( R.getH()); }
template <class T> void CPUCore<T>::ld_a_l()     { R.setA( R.getL()); }
template <class T> void CPUCore<T>::ld_a_ixh()   { R.setA( R.getIXh()); }
template <class T> void CPUCore<T>::ld_a_ixl()   { R.setA( R.getIXl()); }
template <class T> void CPUCore<T>::ld_a_iyh()   { R.setA( R.getIYh()); }
template <class T> void CPUCore<T>::ld_a_iyl()   { R.setA( R.getIYl()); }
template <class T> void CPUCore<T>::ld_b_b()     { }
template <class T> void CPUCore<T>::ld_b_a()     { R.setB(R.getA()); }
template <class T> void CPUCore<T>::ld_b_c()     { R.setB(R.getC()); }
template <class T> void CPUCore<T>::ld_b_d()     { R.setB(R.getD()); }
template <class T> void CPUCore<T>::ld_b_e()     { R.setB(R.getE()); }
template <class T> void CPUCore<T>::ld_b_h()     { R.setB(R.getH()); }
template <class T> void CPUCore<T>::ld_b_l()     { R.setB(R.getL()); }
template <class T> void CPUCore<T>::ld_b_ixh()   { R.setB(R.getIXh()); }
template <class T> void CPUCore<T>::ld_b_ixl()   { R.setB(R.getIXl()); }
template <class T> void CPUCore<T>::ld_b_iyh()   { R.setB(R.getIYh()); }
template <class T> void CPUCore<T>::ld_b_iyl()   { R.setB(R.getIYl()); }
template <class T> void CPUCore<T>::ld_c_c()     { }
template <class T> void CPUCore<T>::ld_c_a()     { R.setC(R.getA()); }
template <class T> void CPUCore<T>::ld_c_b()     { R.setC(R.getB()); }
template <class T> void CPUCore<T>::ld_c_d()     { R.setC(R.getD()); }
template <class T> void CPUCore<T>::ld_c_e()     { R.setC(R.getE()); }
template <class T> void CPUCore<T>::ld_c_h()     { R.setC(R.getH()); }
template <class T> void CPUCore<T>::ld_c_l()     { R.setC(R.getL()); }
template <class T> void CPUCore<T>::ld_c_ixh()   { R.setC(R.getIXh()); }
template <class T> void CPUCore<T>::ld_c_ixl()   { R.setC(R.getIXl()); }
template <class T> void CPUCore<T>::ld_c_iyh()   { R.setC(R.getIYh()); }
template <class T> void CPUCore<T>::ld_c_iyl()   { R.setC(R.getIYl()); }
template <class T> void CPUCore<T>::ld_d_d()     { }
template <class T> void CPUCore<T>::ld_d_a()     { R.setD(R.getA()); }
template <class T> void CPUCore<T>::ld_d_c()     { R.setD(R.getC()); }
template <class T> void CPUCore<T>::ld_d_b()     { R.setD(R.getB()); }
template <class T> void CPUCore<T>::ld_d_e()     { R.setD(R.getE()); }
template <class T> void CPUCore<T>::ld_d_h()     { R.setD(R.getH()); }
template <class T> void CPUCore<T>::ld_d_l()     { R.setD(R.getL()); }
template <class T> void CPUCore<T>::ld_d_ixh()   { R.setD(R.getIXh()); }
template <class T> void CPUCore<T>::ld_d_ixl()   { R.setD(R.getIXl()); }
template <class T> void CPUCore<T>::ld_d_iyh()   { R.setD(R.getIYh()); }
template <class T> void CPUCore<T>::ld_d_iyl()   { R.setD(R.getIYl()); }
template <class T> void CPUCore<T>::ld_e_e()     { }
template <class T> void CPUCore<T>::ld_e_a()     { R.setE(R.getA()); }
template <class T> void CPUCore<T>::ld_e_c()     { R.setE(R.getC()); }
template <class T> void CPUCore<T>::ld_e_b()     { R.setE(R.getB()); }
template <class T> void CPUCore<T>::ld_e_d()     { R.setE(R.getD()); }
template <class T> void CPUCore<T>::ld_e_h()     { R.setE(R.getH()); }
template <class T> void CPUCore<T>::ld_e_l()     { R.setE(R.getL()); }
template <class T> void CPUCore<T>::ld_e_ixh()   { R.setE(R.getIXh()); }
template <class T> void CPUCore<T>::ld_e_ixl()   { R.setE(R.getIXl()); }
template <class T> void CPUCore<T>::ld_e_iyh()   { R.setE(R.getIYh()); }
template <class T> void CPUCore<T>::ld_e_iyl()   { R.setE(R.getIYl()); }
template <class T> void CPUCore<T>::ld_h_h()     { }
template <class T> void CPUCore<T>::ld_h_a()     { R.setH(R.getA()); }
template <class T> void CPUCore<T>::ld_h_c()     { R.setH(R.getC()); }
template <class T> void CPUCore<T>::ld_h_b()     { R.setH(R.getB()); }
template <class T> void CPUCore<T>::ld_h_e()     { R.setH(R.getE()); }
template <class T> void CPUCore<T>::ld_h_d()     { R.setH(R.getD()); }
template <class T> void CPUCore<T>::ld_h_l()     { R.setH(R.getL()); }
template <class T> void CPUCore<T>::ld_l_l()     { }
template <class T> void CPUCore<T>::ld_l_a()     { R.setL(R.getA()); }
template <class T> void CPUCore<T>::ld_l_c()     { R.setL(R.getC()); }
template <class T> void CPUCore<T>::ld_l_b()     { R.setL(R.getB()); }
template <class T> void CPUCore<T>::ld_l_e()     { R.setL(R.getE()); }
template <class T> void CPUCore<T>::ld_l_d()     { R.setL(R.getD()); }
template <class T> void CPUCore<T>::ld_l_h()     { R.setL(R.getH()); }
template <class T> void CPUCore<T>::ld_ixh_a()   { R.setIXh(R.getA()); }
template <class T> void CPUCore<T>::ld_ixh_b()   { R.setIXh(R.getB()); }
template <class T> void CPUCore<T>::ld_ixh_c()   { R.setIXh(R.getC()); }
template <class T> void CPUCore<T>::ld_ixh_d()   { R.setIXh(R.getD()); }
template <class T> void CPUCore<T>::ld_ixh_e()   { R.setIXh(R.getE()); }
template <class T> void CPUCore<T>::ld_ixh_ixh() { }
template <class T> void CPUCore<T>::ld_ixh_ixl() { R.setIXh(R.getIXl()); }
template <class T> void CPUCore<T>::ld_ixl_a()   { R.setIXl(R.getA()); }
template <class T> void CPUCore<T>::ld_ixl_b()   { R.setIXl(R.getB()); }
template <class T> void CPUCore<T>::ld_ixl_c()   { R.setIXl(R.getC()); }
template <class T> void CPUCore<T>::ld_ixl_d()   { R.setIXl(R.getD()); }
template <class T> void CPUCore<T>::ld_ixl_e()   { R.setIXl(R.getE()); }
template <class T> void CPUCore<T>::ld_ixl_ixh() { R.setIXl(R.getIXh()); }
template <class T> void CPUCore<T>::ld_ixl_ixl() { }
template <class T> void CPUCore<T>::ld_iyh_a()   { R.setIYh(R.getA()); }
template <class T> void CPUCore<T>::ld_iyh_b()   { R.setIYh(R.getB()); }
template <class T> void CPUCore<T>::ld_iyh_c()   { R.setIYh(R.getC()); }
template <class T> void CPUCore<T>::ld_iyh_d()   { R.setIYh(R.getD()); }
template <class T> void CPUCore<T>::ld_iyh_e()   { R.setIYh(R.getE()); }
template <class T> void CPUCore<T>::ld_iyh_iyh() { }
template <class T> void CPUCore<T>::ld_iyh_iyl() { R.setIYh(R.getIYl()); }
template <class T> void CPUCore<T>::ld_iyl_a()   { R.setIYl(R.getA()); }
template <class T> void CPUCore<T>::ld_iyl_b()   { R.setIYl(R.getB()); }
template <class T> void CPUCore<T>::ld_iyl_c()   { R.setIYl(R.getC()); }
template <class T> void CPUCore<T>::ld_iyl_d()   { R.setIYl(R.getD()); }
template <class T> void CPUCore<T>::ld_iyl_e()   { R.setIYl(R.getE()); }
template <class T> void CPUCore<T>::ld_iyl_iyh() { R.setIYl(R.getIYh()); }
template <class T> void CPUCore<T>::ld_iyl_iyl() { }

// LD SP,ss
template <class T> void CPUCore<T>::ld_sp_hl()   { R.SP = R.HL; }
template <class T> void CPUCore<T>::ld_sp_ix()   { R.SP = R.IX; }
template <class T> void CPUCore<T>::ld_sp_iy()   { R.SP = R.IY; }

// LD (ss),a
template <class T> inline void CPUCore<T>::WR_X_A(word x)
{
	WRMEM(x, R.getA());
}
template <class T> void CPUCore<T>::ld_xbc_a() { WR_X_A(R.BC); }
template <class T> void CPUCore<T>::ld_xde_a() { WR_X_A(R.DE); }
template <class T> void CPUCore<T>::ld_xhl_a() { WR_X_A(R.HL); }

// LD (HL),r
template <class T> inline void CPUCore<T>::WR_HL_X(byte val)
{
	WRMEM(R.HL, val);
}
template <class T> void CPUCore<T>::ld_xhl_b() { WR_HL_X(R.getB()); }
template <class T> void CPUCore<T>::ld_xhl_c() { WR_HL_X(R.getC()); }
template <class T> void CPUCore<T>::ld_xhl_d() { WR_HL_X(R.getD()); }
template <class T> void CPUCore<T>::ld_xhl_e() { WR_HL_X(R.getE()); }
template <class T> void CPUCore<T>::ld_xhl_h() { WR_HL_X(R.getH()); }
template <class T> void CPUCore<T>::ld_xhl_l() { WR_HL_X(R.getL()); }

// LD (HL),n
template <class T> void CPUCore<T>::ld_xhl_byte()
{
	byte val = RDMEM_OPCODE(R.PC++);
	WR_HL_X(val);
}

// LD (IX+e),r
template <class T> inline void CPUCore<T>::WR_X_X(byte val)
{
	WRMEM(memptr, val);
}
template <class T> inline void CPUCore<T>::WR_XIX(byte val)
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst; T::ADD_16_8_DELAY(); WR_X_X(val);
}
template <class T> void CPUCore<T>::ld_xix_a() { WR_XIX(R.getA()); }
template <class T> void CPUCore<T>::ld_xix_b() { WR_XIX(R.getB()); }
template <class T> void CPUCore<T>::ld_xix_c() { WR_XIX(R.getC()); }
template <class T> void CPUCore<T>::ld_xix_d() { WR_XIX(R.getD()); }
template <class T> void CPUCore<T>::ld_xix_e() { WR_XIX(R.getE()); }
template <class T> void CPUCore<T>::ld_xix_h() { WR_XIX(R.getH()); }
template <class T> void CPUCore<T>::ld_xix_l() { WR_XIX(R.getL()); }

// LD (IY+e),r
template <class T> inline void CPUCore<T>::WR_XIY(byte val)
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst; T::ADD_16_8_DELAY(); WR_X_X(val);
}
template <class T> void CPUCore<T>::ld_xiy_a() { WR_XIY(R.getA()); }
template <class T> void CPUCore<T>::ld_xiy_b() { WR_XIY(R.getB()); }
template <class T> void CPUCore<T>::ld_xiy_c() { WR_XIY(R.getC()); }
template <class T> void CPUCore<T>::ld_xiy_d() { WR_XIY(R.getD()); }
template <class T> void CPUCore<T>::ld_xiy_e() { WR_XIY(R.getE()); }
template <class T> void CPUCore<T>::ld_xiy_h() { WR_XIY(R.getH()); }
template <class T> void CPUCore<T>::ld_xiy_l() { WR_XIY(R.getL()); }

// LD (IX+e),n
template <class T> void CPUCore<T>::ld_xix_byte()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	byte val = RDMEM_OPCODE(R.PC++);
	T::PARALLEL_DELAY(); WR_X_X(val);
}

// LD (IY+e),n
template <class T> void CPUCore<T>::ld_xiy_byte()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	byte val = RDMEM_OPCODE(R.PC++);
	T::PARALLEL_DELAY(); WR_X_X(val);
}

// LD (nn),A
template <class T> void CPUCore<T>::ld_xbyte_a()
{
	word x = RD_WORD_PC();
	memptr = R.getA() << 8;
	WRMEM(x, R.getA());
}

// LD (nn),ss
template <class T> inline void CPUCore<T>::WR_NN_Y(word reg)
{
	memptr  = RD_WORD_PC();
	WRMEM(memptr + 0, reg & 255);
	WRMEM(memptr + 1, reg >> 8);
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
	R.setA(RDMEM(x));
}
template <class T> void CPUCore<T>::ld_a_xbc() { RD_A_X(R.BC); }
template <class T> void CPUCore<T>::ld_a_xde() { RD_A_X(R.DE); }
template <class T> void CPUCore<T>::ld_a_xhl() { RD_A_X(R.HL); }

// LD A,(IX+e)
template <class T> void CPUCore<T>::ld_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst; T::ADD_16_8_DELAY(); RD_A_X(memptr);
}

// LD A,(IY+e)
template <class T> void CPUCore<T>::ld_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst; T::ADD_16_8_DELAY(); RD_A_X(memptr);
}

// LD A,(nn)
template <class T> void CPUCore<T>::ld_a_xbyte()
{
	memptr = RD_WORD_PC();
	RD_A_X(memptr);
	++memptr;
}

// LD r,n
template <class T> void CPUCore<T>::ld_a_byte()   { R.setA(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_b_byte()   { R.setB(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_c_byte()   { R.setC(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_d_byte()   { R.setD(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_e_byte()   { R.setE(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_h_byte()   { R.setH(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_l_byte()   { R.setL(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_ixh_byte() { R.setIXh(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_ixl_byte() { R.setIXl(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_iyh_byte() { R.setIYh(RDMEM_OPCODE(R.PC++)); }
template <class T> void CPUCore<T>::ld_iyl_byte() { R.setIYl(RDMEM_OPCODE(R.PC++)); }

// LD r,(hl)
template <class T> void CPUCore<T>::ld_b_xhl() { R.setB(RDMEM(R.HL)); }
template <class T> void CPUCore<T>::ld_c_xhl() { R.setC(RDMEM(R.HL)); }
template <class T> void CPUCore<T>::ld_d_xhl() { R.setD(RDMEM(R.HL)); }
template <class T> void CPUCore<T>::ld_e_xhl() { R.setE(RDMEM(R.HL)); }
template <class T> void CPUCore<T>::ld_h_xhl() { R.setH(RDMEM(R.HL)); }
template <class T> void CPUCore<T>::ld_l_xhl() { R.setL(RDMEM(R.HL)); }

// LD r,(IX+e)
template <class T> inline byte CPUCore<T>::RD_R_XIX()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst; T::ADD_16_8_DELAY();
	return RDMEM(memptr);
}
template <class T> void CPUCore<T>::ld_b_xix() { R.setB(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_c_xix() { R.setC(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_d_xix() { R.setD(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_e_xix() { R.setE(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_h_xix() { R.setH(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_l_xix() { R.setL(RD_R_XIX()); }

// LD r,(IY+e)
template <class T> inline byte CPUCore<T>::RD_R_XIY()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst; T::ADD_16_8_DELAY();
	return RDMEM(memptr);
}
template <class T> void CPUCore<T>::ld_b_xiy() { R.setB(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_c_xiy() { R.setC(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_d_xiy() { R.setD(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_e_xiy() { R.setE(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_h_xiy() { R.setH(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_l_xiy() { R.setL(RD_R_XIY()); }

// LD ss,(nn)
template <class T> inline word CPUCore<T>::RD_P_XX()
{
	memptr = RD_WORD_PC();
	word res = RDMEM(memptr + 0);
	res     += RDMEM(memptr + 1) << 8;
	++memptr;
	return res;
}
template <class T> void CPUCore<T>::ld_bc_xword() { R.BC = RD_P_XX(); }
template <class T> void CPUCore<T>::ld_de_xword() { R.DE = RD_P_XX(); }
template <class T> void CPUCore<T>::ld_hl_xword() { R.HL = RD_P_XX(); }
template <class T> void CPUCore<T>::ld_ix_xword() { R.IX = RD_P_XX(); }
template <class T> void CPUCore<T>::ld_iy_xword() { R.IY = RD_P_XX(); }
template <class T> void CPUCore<T>::ld_sp_xword() { R.SP = RD_P_XX(); }

// LD ss,nn
template <class T> void CPUCore<T>::ld_bc_word() { R.BC = RD_WORD_PC(); }
template <class T> void CPUCore<T>::ld_de_word() { R.DE = RD_WORD_PC(); }
template <class T> void CPUCore<T>::ld_hl_word() { R.HL = RD_WORD_PC(); }
template <class T> void CPUCore<T>::ld_ix_word() { R.IX = RD_WORD_PC(); }
template <class T> void CPUCore<T>::ld_iy_word() { R.IY = RD_WORD_PC(); }
template <class T> void CPUCore<T>::ld_sp_word() { R.SP = RD_WORD_PC(); }


// ADC A,r
template <class T> inline void CPUCore<T>::ADC(byte reg)
{
	int res = R.getA() + reg + ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA() ^ 0x80) & (reg ^ res) & 0x80) ? V_FLAG : 0));
	R.setA(res);
}
template <class T> void CPUCore<T>::adc_a_a()   { ADC(R.getA()); }
template <class T> void CPUCore<T>::adc_a_b()   { ADC(R.getB()); }
template <class T> void CPUCore<T>::adc_a_c()   { ADC(R.getC()); }
template <class T> void CPUCore<T>::adc_a_d()   { ADC(R.getD()); }
template <class T> void CPUCore<T>::adc_a_e()   { ADC(R.getE()); }
template <class T> void CPUCore<T>::adc_a_h()   { ADC(R.getH()); }
template <class T> void CPUCore<T>::adc_a_l()   { ADC(R.getL()); }
template <class T> void CPUCore<T>::adc_a_ixl() { ADC(R.getIXl()); }
template <class T> void CPUCore<T>::adc_a_ixh() { ADC(R.getIXh()); }
template <class T> void CPUCore<T>::adc_a_iyl() { ADC(R.getIYl()); }
template <class T> void CPUCore<T>::adc_a_iyh() { ADC(R.getIYh()); }
template <class T> void CPUCore<T>::adc_a_byte(){ ADC(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::adc_a_x(word x) { ADC(RDMEM(x)); }
template <class T> void CPUCore<T>::adc_a_xhl() { adc_a_x(R.HL); }
template <class T> void CPUCore<T>::adc_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); adc_a_x(memptr);
}
template <class T> void CPUCore<T>::adc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); adc_a_x(memptr);
}

// ADD A,r
template <class T> inline void CPUCore<T>::ADD(byte reg)
{
	int res = R.getA() + reg;
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA() ^ 0x80) & (reg ^ res) & 0x80) ? V_FLAG : 0));
	R.setA(res);
}
template <class T> void CPUCore<T>::add_a_a()   { ADD(R.getA()); }
template <class T> void CPUCore<T>::add_a_b()   { ADD(R.getB()); }
template <class T> void CPUCore<T>::add_a_c()   { ADD(R.getC()); }
template <class T> void CPUCore<T>::add_a_d()   { ADD(R.getD()); }
template <class T> void CPUCore<T>::add_a_e()   { ADD(R.getE()); }
template <class T> void CPUCore<T>::add_a_h()   { ADD(R.getH()); }
template <class T> void CPUCore<T>::add_a_l()   { ADD(R.getL()); }
template <class T> void CPUCore<T>::add_a_ixl() { ADD(R.getIXl()); }
template <class T> void CPUCore<T>::add_a_ixh() { ADD(R.getIXh()); }
template <class T> void CPUCore<T>::add_a_iyl() { ADD(R.getIYl()); }
template <class T> void CPUCore<T>::add_a_iyh() { ADD(R.getIYh()); }
template <class T> void CPUCore<T>::add_a_byte(){ ADD(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::add_a_x(word x) { ADD(RDMEM(x)); }
template <class T> void CPUCore<T>::add_a_xhl() { add_a_x(R.HL); }
template <class T> void CPUCore<T>::add_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); add_a_x(memptr);
}
template <class T> void CPUCore<T>::add_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); add_a_x(memptr);
}

// AND r
template <class T> inline void CPUCore<T>::AND(byte reg)
{
	R.setA(R.getA() & reg);
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
}
template <class T> void CPUCore<T>::and_a()   { AND(R.getA()); }
template <class T> void CPUCore<T>::and_b()   { AND(R.getB()); }
template <class T> void CPUCore<T>::and_c()   { AND(R.getC()); }
template <class T> void CPUCore<T>::and_d()   { AND(R.getD()); }
template <class T> void CPUCore<T>::and_e()   { AND(R.getE()); }
template <class T> void CPUCore<T>::and_h()   { AND(R.getH()); }
template <class T> void CPUCore<T>::and_l()   { AND(R.getL()); }
template <class T> void CPUCore<T>::and_ixh() { AND(R.getIXh()); }
template <class T> void CPUCore<T>::and_ixl() { AND(R.getIXl()); }
template <class T> void CPUCore<T>::and_iyh() { AND(R.getIYh()); }
template <class T> void CPUCore<T>::and_iyl() { AND(R.getIYl()); }
template <class T> void CPUCore<T>::and_byte(){ AND(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::and_x(word x) { AND(RDMEM(x)); }
template <class T> void CPUCore<T>::and_xhl() { and_x(R.HL); }
template <class T> void CPUCore<T>::and_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); and_x(memptr);
}
template <class T> void CPUCore<T>::and_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); and_x(memptr);
}

// CP r
template <class T> inline void CPUCore<T>::CP(byte reg)
{
	int q = R.getA() - reg;
	R.setF(ZSTable[q & 0xFF] |
	       (reg & (X_FLAG | Y_FLAG)) |	// XY from operand, not from result
	       ((q & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ q ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ q) & 0x80) ? V_FLAG : 0));
}
template <class T> void CPUCore<T>::cp_a()   { CP(R.getA()); }
template <class T> void CPUCore<T>::cp_b()   { CP(R.getB()); }
template <class T> void CPUCore<T>::cp_c()   { CP(R.getC()); }
template <class T> void CPUCore<T>::cp_d()   { CP(R.getD()); }
template <class T> void CPUCore<T>::cp_e()   { CP(R.getE()); }
template <class T> void CPUCore<T>::cp_h()   { CP(R.getH()); }
template <class T> void CPUCore<T>::cp_l()   { CP(R.getL()); }
template <class T> void CPUCore<T>::cp_ixh() { CP(R.getIXh()); }
template <class T> void CPUCore<T>::cp_ixl() { CP(R.getIXl()); }
template <class T> void CPUCore<T>::cp_iyh() { CP(R.getIYh()); }
template <class T> void CPUCore<T>::cp_iyl() { CP(R.getIYl()); }
template <class T> void CPUCore<T>::cp_byte(){ CP(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::cp_x(word x) { CP(RDMEM(x)); }
template <class T> void CPUCore<T>::cp_xhl() { cp_x(R.HL); }
template <class T> void CPUCore<T>::cp_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); cp_x(memptr);
}
template <class T> void CPUCore<T>::cp_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); cp_x(memptr);
}

// OR r
template <class T> inline void CPUCore<T>::OR(byte reg)
{
	R.setA(R.getA() | reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> void CPUCore<T>::or_a()   { OR(R.getA()); }
template <class T> void CPUCore<T>::or_b()   { OR(R.getB()); }
template <class T> void CPUCore<T>::or_c()   { OR(R.getC()); }
template <class T> void CPUCore<T>::or_d()   { OR(R.getD()); }
template <class T> void CPUCore<T>::or_e()   { OR(R.getE()); }
template <class T> void CPUCore<T>::or_h()   { OR(R.getH()); }
template <class T> void CPUCore<T>::or_l()   { OR(R.getL()); }
template <class T> void CPUCore<T>::or_ixh() { OR(R.getIXh()); }
template <class T> void CPUCore<T>::or_ixl() { OR(R.getIXl()); }
template <class T> void CPUCore<T>::or_iyh() { OR(R.getIYh()); }
template <class T> void CPUCore<T>::or_iyl() { OR(R.getIYl()); }
template <class T> void CPUCore<T>::or_byte(){ OR(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::or_x(word x) { OR(RDMEM(x)); }
template <class T> void CPUCore<T>::or_xhl() { or_x(R.HL); }
template <class T> void CPUCore<T>::or_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); or_x(memptr);
}
template <class T> void CPUCore<T>::or_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); or_x(memptr);
}

// SBC A,r
template <class T> inline void CPUCore<T>::SBC(byte reg)
{
	int res = R.getA() - reg - ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) ? V_FLAG : 0));
	R.setA(res);
}
template <class T> void CPUCore<T>::sbc_a_a()   { SBC(R.getA()); }
template <class T> void CPUCore<T>::sbc_a_b()   { SBC(R.getB()); }
template <class T> void CPUCore<T>::sbc_a_c()   { SBC(R.getC()); }
template <class T> void CPUCore<T>::sbc_a_d()   { SBC(R.getD()); }
template <class T> void CPUCore<T>::sbc_a_e()   { SBC(R.getE()); }
template <class T> void CPUCore<T>::sbc_a_h()   { SBC(R.getH()); }
template <class T> void CPUCore<T>::sbc_a_l()   { SBC(R.getL()); }
template <class T> void CPUCore<T>::sbc_a_ixh() { SBC(R.getIXh()); }
template <class T> void CPUCore<T>::sbc_a_ixl() { SBC(R.getIXl()); }
template <class T> void CPUCore<T>::sbc_a_iyh() { SBC(R.getIYh()); }
template <class T> void CPUCore<T>::sbc_a_iyl() { SBC(R.getIYl()); }
template <class T> void CPUCore<T>::sbc_a_byte(){ SBC(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::sbc_a_x(word x) { SBC(RDMEM(x)); }
template <class T> void CPUCore<T>::sbc_a_xhl() { sbc_a_x(R.HL); }
template <class T> void CPUCore<T>::sbc_a_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); sbc_a_x(memptr);
}
template <class T> void CPUCore<T>::sbc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); sbc_a_x(memptr);
}

// SUB r
template <class T> inline void CPUCore<T>::SUB(byte reg)
{
	int res = R.getA() - reg;
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) ? V_FLAG : 0));
	R.setA(res);
}
template <class T> void CPUCore<T>::sub_a()   { SUB(R.getA()); }
template <class T> void CPUCore<T>::sub_b()   { SUB(R.getB()); }
template <class T> void CPUCore<T>::sub_c()   { SUB(R.getC()); }
template <class T> void CPUCore<T>::sub_d()   { SUB(R.getD()); }
template <class T> void CPUCore<T>::sub_e()   { SUB(R.getE()); }
template <class T> void CPUCore<T>::sub_h()   { SUB(R.getH()); }
template <class T> void CPUCore<T>::sub_l()   { SUB(R.getL()); }
template <class T> void CPUCore<T>::sub_ixh() { SUB(R.getIXh()); }
template <class T> void CPUCore<T>::sub_ixl() { SUB(R.getIXl()); }
template <class T> void CPUCore<T>::sub_iyh() { SUB(R.getIYh()); }
template <class T> void CPUCore<T>::sub_iyl() { SUB(R.getIYl()); }
template <class T> void CPUCore<T>::sub_byte(){ SUB(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::sub_x(word x) { SUB(RDMEM(x)); }
template <class T> void CPUCore<T>::sub_xhl() { sub_x(R.HL); }
template <class T> void CPUCore<T>::sub_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); sub_x(memptr);
}
template <class T> void CPUCore<T>::sub_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); sub_x(memptr);
}

// XOR r
template <class T> inline void CPUCore<T>::XOR(byte reg)
{
	R.setA(R.getA() ^ reg);
	R.setF(ZSPXYTable[R.getA()]);

}
template <class T> void CPUCore<T>::xor_a()   { XOR(R.getA()); }
template <class T> void CPUCore<T>::xor_b()   { XOR(R.getB()); }
template <class T> void CPUCore<T>::xor_c()   { XOR(R.getC()); }
template <class T> void CPUCore<T>::xor_d()   { XOR(R.getD()); }
template <class T> void CPUCore<T>::xor_e()   { XOR(R.getE()); }
template <class T> void CPUCore<T>::xor_h()   { XOR(R.getH()); }
template <class T> void CPUCore<T>::xor_l()   { XOR(R.getL()); }
template <class T> void CPUCore<T>::xor_ixh() { XOR(R.getIXh()); }
template <class T> void CPUCore<T>::xor_ixl() { XOR(R.getIXl()); }
template <class T> void CPUCore<T>::xor_iyh() { XOR(R.getIYh()); }
template <class T> void CPUCore<T>::xor_iyl() { XOR(R.getIYl()); }
template <class T> void CPUCore<T>::xor_byte(){ XOR(RDMEM_OPCODE(R.PC++)); }
template <class T> inline void CPUCore<T>::xor_x(word x) { XOR(RDMEM(x)); }
template <class T> void CPUCore<T>::xor_xhl() { xor_x(R.HL); }
template <class T> void CPUCore<T>::xor_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); xor_x(memptr);
}
template <class T> void CPUCore<T>::xor_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); xor_x(memptr);
}


// DEC r
template <class T> inline byte CPUCore<T>::DEC(byte reg)
{
	reg--;
	R.setF((R.getF() & C_FLAG) |
	       ((reg == 0x7f) ? V_FLAG : 0) |
	       (((reg & 0x0f) == 0x0f) ? H_FLAG : 0) |
	       ZSXYTable[reg] |
	       N_FLAG);
	return reg;
}
template <class T> void CPUCore<T>::dec_a()   { R.setA(DEC(R.getA())); }
template <class T> void CPUCore<T>::dec_b()   { R.setB(DEC(R.getB())); }
template <class T> void CPUCore<T>::dec_c()   { R.setC(DEC(R.getC())); }
template <class T> void CPUCore<T>::dec_d()   { R.setD(DEC(R.getD())); }
template <class T> void CPUCore<T>::dec_e()   { R.setE(DEC(R.getE())); }
template <class T> void CPUCore<T>::dec_h()   { R.setH(DEC(R.getH())); }
template <class T> void CPUCore<T>::dec_l()   { R.setL(DEC(R.getL())); }
template <class T> void CPUCore<T>::dec_ixh() { R.setIXh(DEC(R.getIXh())); }
template <class T> void CPUCore<T>::dec_ixl() { R.setIXl(DEC(R.getIXl())); }
template <class T> void CPUCore<T>::dec_iyh() { R.setIYh(DEC(R.getIYh())); }
template <class T> void CPUCore<T>::dec_iyl() { R.setIYl(DEC(R.getIYl())); }

template <class T> inline void CPUCore<T>::DEC_X(word x)
{
	byte val = DEC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, val);
}
template <class T> void CPUCore<T>::dec_xhl() { DEC_X(R.HL); }
template <class T> void CPUCore<T>::dec_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); DEC_X(memptr);
}
template <class T> void CPUCore<T>::dec_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); DEC_X(memptr);
}

// INC r
template <class T> inline byte CPUCore<T>::INC(byte reg)
{
	reg++;
	R.setF((R.getF() & C_FLAG) |
	       ((reg == 0x80) ? V_FLAG : 0) |
	       (((reg & 0x0f) == 0x00) ? H_FLAG : 0) |
	       ZSXYTable[reg]);
	return reg;
}
template <class T> void CPUCore<T>::inc_a()   { R.setA(INC(R.getA())); }
template <class T> void CPUCore<T>::inc_b()   { R.setB(INC(R.getB())); }
template <class T> void CPUCore<T>::inc_c()   { R.setC(INC(R.getC())); }
template <class T> void CPUCore<T>::inc_d()   { R.setD(INC(R.getD())); }
template <class T> void CPUCore<T>::inc_e()   { R.setE(INC(R.getE())); }
template <class T> void CPUCore<T>::inc_h()   { R.setH(INC(R.getH())); }
template <class T> void CPUCore<T>::inc_l()   { R.setL(INC(R.getL())); }
template <class T> void CPUCore<T>::inc_ixh() { R.setIXh(INC(R.getIXh())); }
template <class T> void CPUCore<T>::inc_ixl() { R.setIXl(INC(R.getIXl())); }
template <class T> void CPUCore<T>::inc_iyh() { R.setIYh(INC(R.getIYh())); }
template <class T> void CPUCore<T>::inc_iyl() { R.setIYl(INC(R.getIYl())); }

template <class T> inline void CPUCore<T>::INC_X(word x)
{
	byte val = INC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, val);
}
template <class T> void CPUCore<T>::inc_xhl() { INC_X(R.HL); }
template <class T> void CPUCore<T>::inc_xix()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IX + ofst;
	T::ADD_16_8_DELAY(); INC_X(memptr);
}
template <class T> void CPUCore<T>::inc_xiy()
{
	offset ofst = RDMEM_OPCODE(R.PC++);
	memptr = R.IY + ofst;
	T::ADD_16_8_DELAY(); INC_X(memptr);
}


// ADC HL,ss
template <class T> inline void CPUCore<T>::ADCW(word reg)
{
	memptr = R.HL + 1;
	int res = R.HL + reg + ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF((((R.HL ^ res ^ reg) >> 8) & H_FLAG) |
	       ((res & 0x10000) ? C_FLAG : 0) |
	       ((res & 0xffff) ? 0 : Z_FLAG) |
	       (((reg ^ R.HL ^ 0x8000) & (reg ^ res) & 0x8000) ? V_FLAG : 0) |
	       ((res >> 8) & (S_FLAG | X_FLAG | Y_FLAG)));
	R.HL = res;
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::adc_hl_bc() { ADCW(R.BC); }
template <class T> void CPUCore<T>::adc_hl_de() { ADCW(R.DE); }
template <class T> void CPUCore<T>::adc_hl_hl() { ADCW(R.HL); }
template <class T> void CPUCore<T>::adc_hl_sp() { ADCW(R.SP); }

// ADD HL/IX/IY,ss
template <class T> inline void CPUCore<T>::ADDW(word &reg1, word reg2)
{
	memptr = reg1 + 1;
	int res = reg1 + reg2;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	       ((res & 0x10000) ? C_FLAG : 0) |
	       ((res >> 8) & (X_FLAG | Y_FLAG)));
	reg1 = res;
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::add_hl_bc() { ADDW(R.HL, R.BC); }
template <class T> void CPUCore<T>::add_hl_de() { ADDW(R.HL, R.DE); }
template <class T> void CPUCore<T>::add_hl_hl() { ADDW(R.HL, R.HL); }
template <class T> void CPUCore<T>::add_hl_sp() { ADDW(R.HL, R.SP); }
template <class T> void CPUCore<T>::add_ix_bc() { ADDW(R.IX, R.BC); }
template <class T> void CPUCore<T>::add_ix_de() { ADDW(R.IX, R.DE); }
template <class T> void CPUCore<T>::add_ix_ix() { ADDW(R.IX, R.IX); }
template <class T> void CPUCore<T>::add_ix_sp() { ADDW(R.IX, R.SP); }
template <class T> void CPUCore<T>::add_iy_bc() { ADDW(R.IY, R.BC); }
template <class T> void CPUCore<T>::add_iy_de() { ADDW(R.IY, R.DE); }
template <class T> void CPUCore<T>::add_iy_iy() { ADDW(R.IY, R.IY); }
template <class T> void CPUCore<T>::add_iy_sp() { ADDW(R.IY, R.SP); }

// SBC HL,ss
template <class T> inline void CPUCore<T>::SBCW(word reg)
{
	memptr = R.HL + 1;
	int res = R.HL - reg - ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF((((R.HL ^ res ^ reg) >> 8) & H_FLAG) |
	       ((res & 0x10000) ? C_FLAG : 0) |
	       ((res & 0xffff) ? 0 : Z_FLAG) |
	       (((reg ^ R.HL) & (R.HL ^ res) & 0x8000) ? V_FLAG : 0) |
	       ((res >> 8) & (S_FLAG | X_FLAG | Y_FLAG)) |
	       N_FLAG);
	R.HL = res;
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::sbc_hl_bc() { SBCW(R.BC); }
template <class T> void CPUCore<T>::sbc_hl_de() { SBCW(R.DE); }
template <class T> void CPUCore<T>::sbc_hl_hl() { SBCW(R.HL); }
template <class T> void CPUCore<T>::sbc_hl_sp() { SBCW(R.SP); }


// DEC ss
template <class T> void CPUCore<T>::dec_bc() { --R.BC; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_de() { --R.DE; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_hl() { --R.HL; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_ix() { --R.IX; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_iy() { --R.IY; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::dec_sp() { --R.SP; T::INC_16_DELAY(); }

// INC ss
template <class T> void CPUCore<T>::inc_bc() { ++R.BC; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_de() { ++R.DE; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_hl() { ++R.HL; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_ix() { ++R.IX; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_iy() { ++R.IY; T::INC_16_DELAY(); }
template <class T> void CPUCore<T>::inc_sp() { ++R.SP; T::INC_16_DELAY(); }


// BIT n,r
template <class T> inline void CPUCore<T>::BIT(byte b, byte reg)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[reg & (1 << b)] |
	       (reg & (X_FLAG | Y_FLAG)) |
	       H_FLAG);
}
template <class T> void CPUCore<T>::bit_0_a() { BIT(0, R.getA()); }
template <class T> void CPUCore<T>::bit_0_b() { BIT(0, R.getB()); }
template <class T> void CPUCore<T>::bit_0_c() { BIT(0, R.getC()); }
template <class T> void CPUCore<T>::bit_0_d() { BIT(0, R.getD()); }
template <class T> void CPUCore<T>::bit_0_e() { BIT(0, R.getE()); }
template <class T> void CPUCore<T>::bit_0_h() { BIT(0, R.getH()); }
template <class T> void CPUCore<T>::bit_0_l() { BIT(0, R.getL()); }
template <class T> void CPUCore<T>::bit_1_a() { BIT(1, R.getA()); }
template <class T> void CPUCore<T>::bit_1_b() { BIT(1, R.getB()); }
template <class T> void CPUCore<T>::bit_1_c() { BIT(1, R.getC()); }
template <class T> void CPUCore<T>::bit_1_d() { BIT(1, R.getD()); }
template <class T> void CPUCore<T>::bit_1_e() { BIT(1, R.getE()); }
template <class T> void CPUCore<T>::bit_1_h() { BIT(1, R.getH()); }
template <class T> void CPUCore<T>::bit_1_l() { BIT(1, R.getL()); }
template <class T> void CPUCore<T>::bit_2_a() { BIT(2, R.getA()); }
template <class T> void CPUCore<T>::bit_2_b() { BIT(2, R.getB()); }
template <class T> void CPUCore<T>::bit_2_c() { BIT(2, R.getC()); }
template <class T> void CPUCore<T>::bit_2_d() { BIT(2, R.getD()); }
template <class T> void CPUCore<T>::bit_2_e() { BIT(2, R.getE()); }
template <class T> void CPUCore<T>::bit_2_h() { BIT(2, R.getH()); }
template <class T> void CPUCore<T>::bit_2_l() { BIT(2, R.getL()); }
template <class T> void CPUCore<T>::bit_3_a() { BIT(3, R.getA()); }
template <class T> void CPUCore<T>::bit_3_b() { BIT(3, R.getB()); }
template <class T> void CPUCore<T>::bit_3_c() { BIT(3, R.getC()); }
template <class T> void CPUCore<T>::bit_3_d() { BIT(3, R.getD()); }
template <class T> void CPUCore<T>::bit_3_e() { BIT(3, R.getE()); }
template <class T> void CPUCore<T>::bit_3_h() { BIT(3, R.getH()); }
template <class T> void CPUCore<T>::bit_3_l() { BIT(3, R.getL()); }
template <class T> void CPUCore<T>::bit_4_a() { BIT(4, R.getA()); }
template <class T> void CPUCore<T>::bit_4_b() { BIT(4, R.getB()); }
template <class T> void CPUCore<T>::bit_4_c() { BIT(4, R.getC()); }
template <class T> void CPUCore<T>::bit_4_d() { BIT(4, R.getD()); }
template <class T> void CPUCore<T>::bit_4_e() { BIT(4, R.getE()); }
template <class T> void CPUCore<T>::bit_4_h() { BIT(4, R.getH()); }
template <class T> void CPUCore<T>::bit_4_l() { BIT(4, R.getL()); }
template <class T> void CPUCore<T>::bit_5_a() { BIT(5, R.getA()); }
template <class T> void CPUCore<T>::bit_5_b() { BIT(5, R.getB()); }
template <class T> void CPUCore<T>::bit_5_c() { BIT(5, R.getC()); }
template <class T> void CPUCore<T>::bit_5_d() { BIT(5, R.getD()); }
template <class T> void CPUCore<T>::bit_5_e() { BIT(5, R.getE()); }
template <class T> void CPUCore<T>::bit_5_h() { BIT(5, R.getH()); }
template <class T> void CPUCore<T>::bit_5_l() { BIT(5, R.getL()); }
template <class T> void CPUCore<T>::bit_6_a() { BIT(6, R.getA()); }
template <class T> void CPUCore<T>::bit_6_b() { BIT(6, R.getB()); }
template <class T> void CPUCore<T>::bit_6_c() { BIT(6, R.getC()); }
template <class T> void CPUCore<T>::bit_6_d() { BIT(6, R.getD()); }
template <class T> void CPUCore<T>::bit_6_e() { BIT(6, R.getE()); }
template <class T> void CPUCore<T>::bit_6_h() { BIT(6, R.getH()); }
template <class T> void CPUCore<T>::bit_6_l() { BIT(6, R.getL()); }
template <class T> void CPUCore<T>::bit_7_a() { BIT(7, R.getA()); }
template <class T> void CPUCore<T>::bit_7_b() { BIT(7, R.getB()); }
template <class T> void CPUCore<T>::bit_7_c() { BIT(7, R.getC()); }
template <class T> void CPUCore<T>::bit_7_d() { BIT(7, R.getD()); }
template <class T> void CPUCore<T>::bit_7_e() { BIT(7, R.getE()); }
template <class T> void CPUCore<T>::bit_7_h() { BIT(7, R.getH()); }
template <class T> void CPUCore<T>::bit_7_l() { BIT(7, R.getL()); }

template <class T> inline void CPUCore<T>::BIT_HL(byte bit)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(R.HL) & (1 << bit)] |
	       H_FLAG |
	       (memptr >> 8) & (X_FLAG | Y_FLAG));
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
	memptr = R.IX + ofst;
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(memptr) & (1 << bit)] |
	       H_FLAG |
	       (memptr >> 8) & (X_FLAG | Y_FLAG));
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
	memptr = R.IY + ofst;
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(memptr) & (1 << bit)] |
	       H_FLAG |
	       (memptr >> 8) & (X_FLAG | Y_FLAG));
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
template <class T> inline byte CPUCore<T>::RES(byte b, byte reg)
{
	return reg & ~(1 << b);
}
template <class T> void CPUCore<T>::res_0_a() { R.setA(RES(0, R.getA())); }
template <class T> void CPUCore<T>::res_0_b() { R.setB(RES(0, R.getB())); }
template <class T> void CPUCore<T>::res_0_c() { R.setC(RES(0, R.getC())); }
template <class T> void CPUCore<T>::res_0_d() { R.setD(RES(0, R.getD())); }
template <class T> void CPUCore<T>::res_0_e() { R.setE(RES(0, R.getE())); }
template <class T> void CPUCore<T>::res_0_h() { R.setH(RES(0, R.getH())); }
template <class T> void CPUCore<T>::res_0_l() { R.setL(RES(0, R.getL())); }
template <class T> void CPUCore<T>::res_1_a() { R.setA(RES(1, R.getA())); }
template <class T> void CPUCore<T>::res_1_b() { R.setB(RES(1, R.getB())); }
template <class T> void CPUCore<T>::res_1_c() { R.setC(RES(1, R.getC())); }
template <class T> void CPUCore<T>::res_1_d() { R.setD(RES(1, R.getD())); }
template <class T> void CPUCore<T>::res_1_e() { R.setE(RES(1, R.getE())); }
template <class T> void CPUCore<T>::res_1_h() { R.setH(RES(1, R.getH())); }
template <class T> void CPUCore<T>::res_1_l() { R.setL(RES(1, R.getL())); }
template <class T> void CPUCore<T>::res_2_a() { R.setA(RES(2, R.getA())); }
template <class T> void CPUCore<T>::res_2_b() { R.setB(RES(2, R.getB())); }
template <class T> void CPUCore<T>::res_2_c() { R.setC(RES(2, R.getC())); }
template <class T> void CPUCore<T>::res_2_d() { R.setD(RES(2, R.getD())); }
template <class T> void CPUCore<T>::res_2_e() { R.setE(RES(2, R.getE())); }
template <class T> void CPUCore<T>::res_2_h() { R.setH(RES(2, R.getH())); }
template <class T> void CPUCore<T>::res_2_l() { R.setL(RES(2, R.getL())); }
template <class T> void CPUCore<T>::res_3_a() { R.setA(RES(3, R.getA())); }
template <class T> void CPUCore<T>::res_3_b() { R.setB(RES(3, R.getB())); }
template <class T> void CPUCore<T>::res_3_c() { R.setC(RES(3, R.getC())); }
template <class T> void CPUCore<T>::res_3_d() { R.setD(RES(3, R.getD())); }
template <class T> void CPUCore<T>::res_3_e() { R.setE(RES(3, R.getE())); }
template <class T> void CPUCore<T>::res_3_h() { R.setH(RES(3, R.getH())); }
template <class T> void CPUCore<T>::res_3_l() { R.setL(RES(3, R.getL())); }
template <class T> void CPUCore<T>::res_4_a() { R.setA(RES(4, R.getA())); }
template <class T> void CPUCore<T>::res_4_b() { R.setB(RES(4, R.getB())); }
template <class T> void CPUCore<T>::res_4_c() { R.setC(RES(4, R.getC())); }
template <class T> void CPUCore<T>::res_4_d() { R.setD(RES(4, R.getD())); }
template <class T> void CPUCore<T>::res_4_e() { R.setE(RES(4, R.getE())); }
template <class T> void CPUCore<T>::res_4_h() { R.setH(RES(4, R.getH())); }
template <class T> void CPUCore<T>::res_4_l() { R.setL(RES(4, R.getL())); }
template <class T> void CPUCore<T>::res_5_a() { R.setA(RES(5, R.getA())); }
template <class T> void CPUCore<T>::res_5_b() { R.setB(RES(5, R.getB())); }
template <class T> void CPUCore<T>::res_5_c() { R.setC(RES(5, R.getC())); }
template <class T> void CPUCore<T>::res_5_d() { R.setD(RES(5, R.getD())); }
template <class T> void CPUCore<T>::res_5_e() { R.setE(RES(5, R.getE())); }
template <class T> void CPUCore<T>::res_5_h() { R.setH(RES(5, R.getH())); }
template <class T> void CPUCore<T>::res_5_l() { R.setL(RES(5, R.getL())); }
template <class T> void CPUCore<T>::res_6_a() { R.setA(RES(6, R.getA())); }
template <class T> void CPUCore<T>::res_6_b() { R.setB(RES(6, R.getB())); }
template <class T> void CPUCore<T>::res_6_c() { R.setC(RES(6, R.getC())); }
template <class T> void CPUCore<T>::res_6_d() { R.setD(RES(6, R.getD())); }
template <class T> void CPUCore<T>::res_6_e() { R.setE(RES(6, R.getE())); }
template <class T> void CPUCore<T>::res_6_h() { R.setH(RES(6, R.getH())); }
template <class T> void CPUCore<T>::res_6_l() { R.setL(RES(6, R.getL())); }
template <class T> void CPUCore<T>::res_7_a() { R.setA(RES(7, R.getA())); }
template <class T> void CPUCore<T>::res_7_b() { R.setB(RES(7, R.getB())); }
template <class T> void CPUCore<T>::res_7_c() { R.setC(RES(7, R.getC())); }
template <class T> void CPUCore<T>::res_7_d() { R.setD(RES(7, R.getD())); }
template <class T> void CPUCore<T>::res_7_e() { R.setE(RES(7, R.getE())); }
template <class T> void CPUCore<T>::res_7_h() { R.setH(RES(7, R.getH())); }
template <class T> void CPUCore<T>::res_7_l() { R.setL(RES(7, R.getL())); }

template <class T> inline byte CPUCore<T>::RES_X(byte bit, word x)
{
	byte res = RES(bit, RDMEM(x));
	T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RES_X_(byte bit, word x)
{
	memptr = x;
	return RES_X(bit, x);
}
template <class T> inline byte CPUCore<T>::RES_X_X(byte bit)
{
	return RES_X_(bit, R.IX + ofst);
}
template <class T> inline byte CPUCore<T>::RES_X_Y(byte bit)
{
	return RES_X_(bit, R.IY + ofst);
}

template <class T> void CPUCore<T>::res_0_xhl()   { RES_X(0, R.HL); }
template <class T> void CPUCore<T>::res_1_xhl()   { RES_X(1, R.HL); }
template <class T> void CPUCore<T>::res_2_xhl()   { RES_X(2, R.HL); }
template <class T> void CPUCore<T>::res_3_xhl()   { RES_X(3, R.HL); }
template <class T> void CPUCore<T>::res_4_xhl()   { RES_X(4, R.HL); }
template <class T> void CPUCore<T>::res_5_xhl()   { RES_X(5, R.HL); }
template <class T> void CPUCore<T>::res_6_xhl()   { RES_X(6, R.HL); }
template <class T> void CPUCore<T>::res_7_xhl()   { RES_X(7, R.HL); }

template <class T> void CPUCore<T>::res_0_xix()   { RES_X_X(0); }
template <class T> void CPUCore<T>::res_1_xix()   { RES_X_X(1); }
template <class T> void CPUCore<T>::res_2_xix()   { RES_X_X(2); }
template <class T> void CPUCore<T>::res_3_xix()   { RES_X_X(3); }
template <class T> void CPUCore<T>::res_4_xix()   { RES_X_X(4); }
template <class T> void CPUCore<T>::res_5_xix()   { RES_X_X(5); }
template <class T> void CPUCore<T>::res_6_xix()   { RES_X_X(6); }
template <class T> void CPUCore<T>::res_7_xix()   { RES_X_X(7); }

template <class T> void CPUCore<T>::res_0_xiy()   { RES_X_Y(0); }
template <class T> void CPUCore<T>::res_1_xiy()   { RES_X_Y(1); }
template <class T> void CPUCore<T>::res_2_xiy()   { RES_X_Y(2); }
template <class T> void CPUCore<T>::res_3_xiy()   { RES_X_Y(3); }
template <class T> void CPUCore<T>::res_4_xiy()   { RES_X_Y(4); }
template <class T> void CPUCore<T>::res_5_xiy()   { RES_X_Y(5); }
template <class T> void CPUCore<T>::res_6_xiy()   { RES_X_Y(6); }
template <class T> void CPUCore<T>::res_7_xiy()   { RES_X_Y(7); }

template <class T> void CPUCore<T>::res_0_xix_a() { R.setA(RES_X_X(0)); }
template <class T> void CPUCore<T>::res_0_xix_b() { R.setB(RES_X_X(0)); }
template <class T> void CPUCore<T>::res_0_xix_c() { R.setC(RES_X_X(0)); }
template <class T> void CPUCore<T>::res_0_xix_d() { R.setD(RES_X_X(0)); }
template <class T> void CPUCore<T>::res_0_xix_e() { R.setE(RES_X_X(0)); }
template <class T> void CPUCore<T>::res_0_xix_h() { R.setH(RES_X_X(0)); }
template <class T> void CPUCore<T>::res_0_xix_l() { R.setL(RES_X_X(0)); }
template <class T> void CPUCore<T>::res_1_xix_a() { R.setA(RES_X_X(1)); }
template <class T> void CPUCore<T>::res_1_xix_b() { R.setB(RES_X_X(1)); }
template <class T> void CPUCore<T>::res_1_xix_c() { R.setC(RES_X_X(1)); }
template <class T> void CPUCore<T>::res_1_xix_d() { R.setD(RES_X_X(1)); }
template <class T> void CPUCore<T>::res_1_xix_e() { R.setE(RES_X_X(1)); }
template <class T> void CPUCore<T>::res_1_xix_h() { R.setH(RES_X_X(1)); }
template <class T> void CPUCore<T>::res_1_xix_l() { R.setL(RES_X_X(1)); }
template <class T> void CPUCore<T>::res_2_xix_a() { R.setA(RES_X_X(2)); }
template <class T> void CPUCore<T>::res_2_xix_b() { R.setB(RES_X_X(2)); }
template <class T> void CPUCore<T>::res_2_xix_c() { R.setC(RES_X_X(2)); }
template <class T> void CPUCore<T>::res_2_xix_d() { R.setD(RES_X_X(2)); }
template <class T> void CPUCore<T>::res_2_xix_e() { R.setE(RES_X_X(2)); }
template <class T> void CPUCore<T>::res_2_xix_h() { R.setH(RES_X_X(2)); }
template <class T> void CPUCore<T>::res_2_xix_l() { R.setL(RES_X_X(2)); }
template <class T> void CPUCore<T>::res_3_xix_a() { R.setA(RES_X_X(3)); }
template <class T> void CPUCore<T>::res_3_xix_b() { R.setB(RES_X_X(3)); }
template <class T> void CPUCore<T>::res_3_xix_c() { R.setC(RES_X_X(3)); }
template <class T> void CPUCore<T>::res_3_xix_d() { R.setD(RES_X_X(3)); }
template <class T> void CPUCore<T>::res_3_xix_e() { R.setE(RES_X_X(3)); }
template <class T> void CPUCore<T>::res_3_xix_h() { R.setH(RES_X_X(3)); }
template <class T> void CPUCore<T>::res_3_xix_l() { R.setL(RES_X_X(3)); }
template <class T> void CPUCore<T>::res_4_xix_a() { R.setA(RES_X_X(4)); }
template <class T> void CPUCore<T>::res_4_xix_b() { R.setB(RES_X_X(4)); }
template <class T> void CPUCore<T>::res_4_xix_c() { R.setC(RES_X_X(4)); }
template <class T> void CPUCore<T>::res_4_xix_d() { R.setD(RES_X_X(4)); }
template <class T> void CPUCore<T>::res_4_xix_e() { R.setE(RES_X_X(4)); }
template <class T> void CPUCore<T>::res_4_xix_h() { R.setH(RES_X_X(4)); }
template <class T> void CPUCore<T>::res_4_xix_l() { R.setL(RES_X_X(4)); }
template <class T> void CPUCore<T>::res_5_xix_a() { R.setA(RES_X_X(5)); }
template <class T> void CPUCore<T>::res_5_xix_b() { R.setB(RES_X_X(5)); }
template <class T> void CPUCore<T>::res_5_xix_c() { R.setC(RES_X_X(5)); }
template <class T> void CPUCore<T>::res_5_xix_d() { R.setD(RES_X_X(5)); }
template <class T> void CPUCore<T>::res_5_xix_e() { R.setE(RES_X_X(5)); }
template <class T> void CPUCore<T>::res_5_xix_h() { R.setH(RES_X_X(5)); }
template <class T> void CPUCore<T>::res_5_xix_l() { R.setL(RES_X_X(5)); }
template <class T> void CPUCore<T>::res_6_xix_a() { R.setA(RES_X_X(6)); }
template <class T> void CPUCore<T>::res_6_xix_b() { R.setB(RES_X_X(6)); }
template <class T> void CPUCore<T>::res_6_xix_c() { R.setC(RES_X_X(6)); }
template <class T> void CPUCore<T>::res_6_xix_d() { R.setD(RES_X_X(6)); }
template <class T> void CPUCore<T>::res_6_xix_e() { R.setE(RES_X_X(6)); }
template <class T> void CPUCore<T>::res_6_xix_h() { R.setH(RES_X_X(6)); }
template <class T> void CPUCore<T>::res_6_xix_l() { R.setL(RES_X_X(6)); }
template <class T> void CPUCore<T>::res_7_xix_a() { R.setA(RES_X_X(7)); }
template <class T> void CPUCore<T>::res_7_xix_b() { R.setB(RES_X_X(7)); }
template <class T> void CPUCore<T>::res_7_xix_c() { R.setC(RES_X_X(7)); }
template <class T> void CPUCore<T>::res_7_xix_d() { R.setD(RES_X_X(7)); }
template <class T> void CPUCore<T>::res_7_xix_e() { R.setE(RES_X_X(7)); }
template <class T> void CPUCore<T>::res_7_xix_h() { R.setH(RES_X_X(7)); }
template <class T> void CPUCore<T>::res_7_xix_l() { R.setL(RES_X_X(7)); }

template <class T> void CPUCore<T>::res_0_xiy_a() { R.setA(RES_X_Y(0)); }
template <class T> void CPUCore<T>::res_0_xiy_b() { R.setB(RES_X_Y(0)); }
template <class T> void CPUCore<T>::res_0_xiy_c() { R.setC(RES_X_Y(0)); }
template <class T> void CPUCore<T>::res_0_xiy_d() { R.setD(RES_X_Y(0)); }
template <class T> void CPUCore<T>::res_0_xiy_e() { R.setE(RES_X_Y(0)); }
template <class T> void CPUCore<T>::res_0_xiy_h() { R.setH(RES_X_Y(0)); }
template <class T> void CPUCore<T>::res_0_xiy_l() { R.setL(RES_X_Y(0)); }
template <class T> void CPUCore<T>::res_1_xiy_a() { R.setA(RES_X_Y(1)); }
template <class T> void CPUCore<T>::res_1_xiy_b() { R.setB(RES_X_Y(1)); }
template <class T> void CPUCore<T>::res_1_xiy_c() { R.setC(RES_X_Y(1)); }
template <class T> void CPUCore<T>::res_1_xiy_d() { R.setD(RES_X_Y(1)); }
template <class T> void CPUCore<T>::res_1_xiy_e() { R.setE(RES_X_Y(1)); }
template <class T> void CPUCore<T>::res_1_xiy_h() { R.setH(RES_X_Y(1)); }
template <class T> void CPUCore<T>::res_1_xiy_l() { R.setL(RES_X_Y(1)); }
template <class T> void CPUCore<T>::res_2_xiy_a() { R.setA(RES_X_Y(2)); }
template <class T> void CPUCore<T>::res_2_xiy_b() { R.setB(RES_X_Y(2)); }
template <class T> void CPUCore<T>::res_2_xiy_c() { R.setC(RES_X_Y(2)); }
template <class T> void CPUCore<T>::res_2_xiy_d() { R.setD(RES_X_Y(2)); }
template <class T> void CPUCore<T>::res_2_xiy_e() { R.setE(RES_X_Y(2)); }
template <class T> void CPUCore<T>::res_2_xiy_h() { R.setH(RES_X_Y(2)); }
template <class T> void CPUCore<T>::res_2_xiy_l() { R.setL(RES_X_Y(2)); }
template <class T> void CPUCore<T>::res_3_xiy_a() { R.setA(RES_X_Y(3)); }
template <class T> void CPUCore<T>::res_3_xiy_b() { R.setB(RES_X_Y(3)); }
template <class T> void CPUCore<T>::res_3_xiy_c() { R.setC(RES_X_Y(3)); }
template <class T> void CPUCore<T>::res_3_xiy_d() { R.setD(RES_X_Y(3)); }
template <class T> void CPUCore<T>::res_3_xiy_e() { R.setE(RES_X_Y(3)); }
template <class T> void CPUCore<T>::res_3_xiy_h() { R.setH(RES_X_Y(3)); }
template <class T> void CPUCore<T>::res_3_xiy_l() { R.setL(RES_X_Y(3)); }
template <class T> void CPUCore<T>::res_4_xiy_a() { R.setA(RES_X_Y(4)); }
template <class T> void CPUCore<T>::res_4_xiy_b() { R.setB(RES_X_Y(4)); }
template <class T> void CPUCore<T>::res_4_xiy_c() { R.setC(RES_X_Y(4)); }
template <class T> void CPUCore<T>::res_4_xiy_d() { R.setD(RES_X_Y(4)); }
template <class T> void CPUCore<T>::res_4_xiy_e() { R.setE(RES_X_Y(4)); }
template <class T> void CPUCore<T>::res_4_xiy_h() { R.setH(RES_X_Y(4)); }
template <class T> void CPUCore<T>::res_4_xiy_l() { R.setL(RES_X_Y(4)); }
template <class T> void CPUCore<T>::res_5_xiy_a() { R.setA(RES_X_Y(5)); }
template <class T> void CPUCore<T>::res_5_xiy_b() { R.setB(RES_X_Y(5)); }
template <class T> void CPUCore<T>::res_5_xiy_c() { R.setC(RES_X_Y(5)); }
template <class T> void CPUCore<T>::res_5_xiy_d() { R.setD(RES_X_Y(5)); }
template <class T> void CPUCore<T>::res_5_xiy_e() { R.setE(RES_X_Y(5)); }
template <class T> void CPUCore<T>::res_5_xiy_h() { R.setH(RES_X_Y(5)); }
template <class T> void CPUCore<T>::res_5_xiy_l() { R.setL(RES_X_Y(5)); }
template <class T> void CPUCore<T>::res_6_xiy_a() { R.setA(RES_X_Y(6)); }
template <class T> void CPUCore<T>::res_6_xiy_b() { R.setB(RES_X_Y(6)); }
template <class T> void CPUCore<T>::res_6_xiy_c() { R.setC(RES_X_Y(6)); }
template <class T> void CPUCore<T>::res_6_xiy_d() { R.setD(RES_X_Y(6)); }
template <class T> void CPUCore<T>::res_6_xiy_e() { R.setE(RES_X_Y(6)); }
template <class T> void CPUCore<T>::res_6_xiy_h() { R.setH(RES_X_Y(6)); }
template <class T> void CPUCore<T>::res_6_xiy_l() { R.setL(RES_X_Y(6)); }
template <class T> void CPUCore<T>::res_7_xiy_a() { R.setA(RES_X_Y(7)); }
template <class T> void CPUCore<T>::res_7_xiy_b() { R.setB(RES_X_Y(7)); }
template <class T> void CPUCore<T>::res_7_xiy_c() { R.setC(RES_X_Y(7)); }
template <class T> void CPUCore<T>::res_7_xiy_d() { R.setD(RES_X_Y(7)); }
template <class T> void CPUCore<T>::res_7_xiy_e() { R.setE(RES_X_Y(7)); }
template <class T> void CPUCore<T>::res_7_xiy_h() { R.setH(RES_X_Y(7)); }
template <class T> void CPUCore<T>::res_7_xiy_l() { R.setL(RES_X_Y(7)); }


// SET n,r
template <class T> inline byte CPUCore<T>::SET(byte b, byte reg)
{
	return reg | (1 << b);
}
template <class T> void CPUCore<T>::set_0_a() { R.setA(SET(0, R.getA())); }
template <class T> void CPUCore<T>::set_0_b() { R.setB(SET(0, R.getB())); }
template <class T> void CPUCore<T>::set_0_c() { R.setC(SET(0, R.getC())); }
template <class T> void CPUCore<T>::set_0_d() { R.setD(SET(0, R.getD())); }
template <class T> void CPUCore<T>::set_0_e() { R.setE(SET(0, R.getE())); }
template <class T> void CPUCore<T>::set_0_h() { R.setH(SET(0, R.getH())); }
template <class T> void CPUCore<T>::set_0_l() { R.setL(SET(0, R.getL())); }
template <class T> void CPUCore<T>::set_1_a() { R.setA(SET(1, R.getA())); }
template <class T> void CPUCore<T>::set_1_b() { R.setB(SET(1, R.getB())); }
template <class T> void CPUCore<T>::set_1_c() { R.setC(SET(1, R.getC())); }
template <class T> void CPUCore<T>::set_1_d() { R.setD(SET(1, R.getD())); }
template <class T> void CPUCore<T>::set_1_e() { R.setE(SET(1, R.getE())); }
template <class T> void CPUCore<T>::set_1_h() { R.setH(SET(1, R.getH())); }
template <class T> void CPUCore<T>::set_1_l() { R.setL(SET(1, R.getL())); }
template <class T> void CPUCore<T>::set_2_a() { R.setA(SET(2, R.getA())); }
template <class T> void CPUCore<T>::set_2_b() { R.setB(SET(2, R.getB())); }
template <class T> void CPUCore<T>::set_2_c() { R.setC(SET(2, R.getC())); }
template <class T> void CPUCore<T>::set_2_d() { R.setD(SET(2, R.getD())); }
template <class T> void CPUCore<T>::set_2_e() { R.setE(SET(2, R.getE())); }
template <class T> void CPUCore<T>::set_2_h() { R.setH(SET(2, R.getH())); }
template <class T> void CPUCore<T>::set_2_l() { R.setL(SET(2, R.getL())); }
template <class T> void CPUCore<T>::set_3_a() { R.setA(SET(3, R.getA())); }
template <class T> void CPUCore<T>::set_3_b() { R.setB(SET(3, R.getB())); }
template <class T> void CPUCore<T>::set_3_c() { R.setC(SET(3, R.getC())); }
template <class T> void CPUCore<T>::set_3_d() { R.setD(SET(3, R.getD())); }
template <class T> void CPUCore<T>::set_3_e() { R.setE(SET(3, R.getE())); }
template <class T> void CPUCore<T>::set_3_h() { R.setH(SET(3, R.getH())); }
template <class T> void CPUCore<T>::set_3_l() { R.setL(SET(3, R.getL())); }
template <class T> void CPUCore<T>::set_4_a() { R.setA(SET(4, R.getA())); }
template <class T> void CPUCore<T>::set_4_b() { R.setB(SET(4, R.getB())); }
template <class T> void CPUCore<T>::set_4_c() { R.setC(SET(4, R.getC())); }
template <class T> void CPUCore<T>::set_4_d() { R.setD(SET(4, R.getD())); }
template <class T> void CPUCore<T>::set_4_e() { R.setE(SET(4, R.getE())); }
template <class T> void CPUCore<T>::set_4_h() { R.setH(SET(4, R.getH())); }
template <class T> void CPUCore<T>::set_4_l() { R.setL(SET(4, R.getL())); }
template <class T> void CPUCore<T>::set_5_a() { R.setA(SET(5, R.getA())); }
template <class T> void CPUCore<T>::set_5_b() { R.setB(SET(5, R.getB())); }
template <class T> void CPUCore<T>::set_5_c() { R.setC(SET(5, R.getC())); }
template <class T> void CPUCore<T>::set_5_d() { R.setD(SET(5, R.getD())); }
template <class T> void CPUCore<T>::set_5_e() { R.setE(SET(5, R.getE())); }
template <class T> void CPUCore<T>::set_5_h() { R.setH(SET(5, R.getH())); }
template <class T> void CPUCore<T>::set_5_l() { R.setL(SET(5, R.getL())); }
template <class T> void CPUCore<T>::set_6_a() { R.setA(SET(6, R.getA())); }
template <class T> void CPUCore<T>::set_6_b() { R.setB(SET(6, R.getB())); }
template <class T> void CPUCore<T>::set_6_c() { R.setC(SET(6, R.getC())); }
template <class T> void CPUCore<T>::set_6_d() { R.setD(SET(6, R.getD())); }
template <class T> void CPUCore<T>::set_6_e() { R.setE(SET(6, R.getE())); }
template <class T> void CPUCore<T>::set_6_h() { R.setH(SET(6, R.getH())); }
template <class T> void CPUCore<T>::set_6_l() { R.setL(SET(6, R.getL())); }
template <class T> void CPUCore<T>::set_7_a() { R.setA(SET(7, R.getA())); }
template <class T> void CPUCore<T>::set_7_b() { R.setB(SET(7, R.getB())); }
template <class T> void CPUCore<T>::set_7_c() { R.setC(SET(7, R.getC())); }
template <class T> void CPUCore<T>::set_7_d() { R.setD(SET(7, R.getD())); }
template <class T> void CPUCore<T>::set_7_e() { R.setE(SET(7, R.getE())); }
template <class T> void CPUCore<T>::set_7_h() { R.setH(SET(7, R.getH())); }
template <class T> void CPUCore<T>::set_7_l() { R.setL(SET(7, R.getL())); }

template <class T> inline byte CPUCore<T>::SET_X(byte bit, word x)
{
	byte res = SET(bit, RDMEM(x));
	T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SET_X_(byte bit, word x)
{
	memptr = x;
	return SET_X(bit, x);
}
template <class T> inline byte CPUCore<T>::SET_X_X(byte bit)
{
	return SET_X_(bit, R.IX + ofst);
}
template <class T> inline byte CPUCore<T>::SET_X_Y(byte bit)
{
	return SET_X_(bit, R.IY + ofst);
}

template <class T> void CPUCore<T>::set_0_xhl()   { SET_X(0, R.HL); }
template <class T> void CPUCore<T>::set_1_xhl()   { SET_X(1, R.HL); }
template <class T> void CPUCore<T>::set_2_xhl()   { SET_X(2, R.HL); }
template <class T> void CPUCore<T>::set_3_xhl()   { SET_X(3, R.HL); }
template <class T> void CPUCore<T>::set_4_xhl()   { SET_X(4, R.HL); }
template <class T> void CPUCore<T>::set_5_xhl()   { SET_X(5, R.HL); }
template <class T> void CPUCore<T>::set_6_xhl()   { SET_X(6, R.HL); }
template <class T> void CPUCore<T>::set_7_xhl()   { SET_X(7, R.HL); }

template <class T> void CPUCore<T>::set_0_xix()   { SET_X_X(0); }
template <class T> void CPUCore<T>::set_1_xix()   { SET_X_X(1); }
template <class T> void CPUCore<T>::set_2_xix()   { SET_X_X(2); }
template <class T> void CPUCore<T>::set_3_xix()   { SET_X_X(3); }
template <class T> void CPUCore<T>::set_4_xix()   { SET_X_X(4); }
template <class T> void CPUCore<T>::set_5_xix()   { SET_X_X(5); }
template <class T> void CPUCore<T>::set_6_xix()   { SET_X_X(6); }
template <class T> void CPUCore<T>::set_7_xix()   { SET_X_X(7); }

template <class T> void CPUCore<T>::set_0_xiy()   { SET_X_Y(0); }
template <class T> void CPUCore<T>::set_1_xiy()   { SET_X_Y(1); }
template <class T> void CPUCore<T>::set_2_xiy()   { SET_X_Y(2); }
template <class T> void CPUCore<T>::set_3_xiy()   { SET_X_Y(3); }
template <class T> void CPUCore<T>::set_4_xiy()   { SET_X_Y(4); }
template <class T> void CPUCore<T>::set_5_xiy()   { SET_X_Y(5); }
template <class T> void CPUCore<T>::set_6_xiy()   { SET_X_Y(6); }
template <class T> void CPUCore<T>::set_7_xiy()   { SET_X_Y(7); }

template <class T> void CPUCore<T>::set_0_xix_a() { R.setA(SET_X_X(0)); }
template <class T> void CPUCore<T>::set_0_xix_b() { R.setB(SET_X_X(0)); }
template <class T> void CPUCore<T>::set_0_xix_c() { R.setC(SET_X_X(0)); }
template <class T> void CPUCore<T>::set_0_xix_d() { R.setD(SET_X_X(0)); }
template <class T> void CPUCore<T>::set_0_xix_e() { R.setE(SET_X_X(0)); }
template <class T> void CPUCore<T>::set_0_xix_h() { R.setH(SET_X_X(0)); }
template <class T> void CPUCore<T>::set_0_xix_l() { R.setL(SET_X_X(0)); }
template <class T> void CPUCore<T>::set_1_xix_a() { R.setA(SET_X_X(1)); }
template <class T> void CPUCore<T>::set_1_xix_b() { R.setB(SET_X_X(1)); }
template <class T> void CPUCore<T>::set_1_xix_c() { R.setC(SET_X_X(1)); }
template <class T> void CPUCore<T>::set_1_xix_d() { R.setD(SET_X_X(1)); }
template <class T> void CPUCore<T>::set_1_xix_e() { R.setE(SET_X_X(1)); }
template <class T> void CPUCore<T>::set_1_xix_h() { R.setH(SET_X_X(1)); }
template <class T> void CPUCore<T>::set_1_xix_l() { R.setL(SET_X_X(1)); }
template <class T> void CPUCore<T>::set_2_xix_a() { R.setA(SET_X_X(2)); }
template <class T> void CPUCore<T>::set_2_xix_b() { R.setB(SET_X_X(2)); }
template <class T> void CPUCore<T>::set_2_xix_c() { R.setC(SET_X_X(2)); }
template <class T> void CPUCore<T>::set_2_xix_d() { R.setD(SET_X_X(2)); }
template <class T> void CPUCore<T>::set_2_xix_e() { R.setE(SET_X_X(2)); }
template <class T> void CPUCore<T>::set_2_xix_h() { R.setH(SET_X_X(2)); }
template <class T> void CPUCore<T>::set_2_xix_l() { R.setL(SET_X_X(2)); }
template <class T> void CPUCore<T>::set_3_xix_a() { R.setA(SET_X_X(3)); }
template <class T> void CPUCore<T>::set_3_xix_b() { R.setB(SET_X_X(3)); }
template <class T> void CPUCore<T>::set_3_xix_c() { R.setC(SET_X_X(3)); }
template <class T> void CPUCore<T>::set_3_xix_d() { R.setD(SET_X_X(3)); }
template <class T> void CPUCore<T>::set_3_xix_e() { R.setE(SET_X_X(3)); }
template <class T> void CPUCore<T>::set_3_xix_h() { R.setH(SET_X_X(3)); }
template <class T> void CPUCore<T>::set_3_xix_l() { R.setL(SET_X_X(3)); }
template <class T> void CPUCore<T>::set_4_xix_a() { R.setA(SET_X_X(4)); }
template <class T> void CPUCore<T>::set_4_xix_b() { R.setB(SET_X_X(4)); }
template <class T> void CPUCore<T>::set_4_xix_c() { R.setC(SET_X_X(4)); }
template <class T> void CPUCore<T>::set_4_xix_d() { R.setD(SET_X_X(4)); }
template <class T> void CPUCore<T>::set_4_xix_e() { R.setE(SET_X_X(4)); }
template <class T> void CPUCore<T>::set_4_xix_h() { R.setH(SET_X_X(4)); }
template <class T> void CPUCore<T>::set_4_xix_l() { R.setL(SET_X_X(4)); }
template <class T> void CPUCore<T>::set_5_xix_a() { R.setA(SET_X_X(5)); }
template <class T> void CPUCore<T>::set_5_xix_b() { R.setB(SET_X_X(5)); }
template <class T> void CPUCore<T>::set_5_xix_c() { R.setC(SET_X_X(5)); }
template <class T> void CPUCore<T>::set_5_xix_d() { R.setD(SET_X_X(5)); }
template <class T> void CPUCore<T>::set_5_xix_e() { R.setE(SET_X_X(5)); }
template <class T> void CPUCore<T>::set_5_xix_h() { R.setH(SET_X_X(5)); }
template <class T> void CPUCore<T>::set_5_xix_l() { R.setL(SET_X_X(5)); }
template <class T> void CPUCore<T>::set_6_xix_a() { R.setA(SET_X_X(6)); }
template <class T> void CPUCore<T>::set_6_xix_b() { R.setB(SET_X_X(6)); }
template <class T> void CPUCore<T>::set_6_xix_c() { R.setC(SET_X_X(6)); }
template <class T> void CPUCore<T>::set_6_xix_d() { R.setD(SET_X_X(6)); }
template <class T> void CPUCore<T>::set_6_xix_e() { R.setE(SET_X_X(6)); }
template <class T> void CPUCore<T>::set_6_xix_h() { R.setH(SET_X_X(6)); }
template <class T> void CPUCore<T>::set_6_xix_l() { R.setL(SET_X_X(6)); }
template <class T> void CPUCore<T>::set_7_xix_a() { R.setA(SET_X_X(7)); }
template <class T> void CPUCore<T>::set_7_xix_b() { R.setB(SET_X_X(7)); }
template <class T> void CPUCore<T>::set_7_xix_c() { R.setC(SET_X_X(7)); }
template <class T> void CPUCore<T>::set_7_xix_d() { R.setD(SET_X_X(7)); }
template <class T> void CPUCore<T>::set_7_xix_e() { R.setE(SET_X_X(7)); }
template <class T> void CPUCore<T>::set_7_xix_h() { R.setH(SET_X_X(7)); }
template <class T> void CPUCore<T>::set_7_xix_l() { R.setL(SET_X_X(7)); }

template <class T> void CPUCore<T>::set_0_xiy_a() { R.setA(SET_X_Y(0)); }
template <class T> void CPUCore<T>::set_0_xiy_b() { R.setB(SET_X_Y(0)); }
template <class T> void CPUCore<T>::set_0_xiy_c() { R.setC(SET_X_Y(0)); }
template <class T> void CPUCore<T>::set_0_xiy_d() { R.setD(SET_X_Y(0)); }
template <class T> void CPUCore<T>::set_0_xiy_e() { R.setE(SET_X_Y(0)); }
template <class T> void CPUCore<T>::set_0_xiy_h() { R.setH(SET_X_Y(0)); }
template <class T> void CPUCore<T>::set_0_xiy_l() { R.setL(SET_X_Y(0)); }
template <class T> void CPUCore<T>::set_1_xiy_a() { R.setA(SET_X_Y(1)); }
template <class T> void CPUCore<T>::set_1_xiy_b() { R.setB(SET_X_Y(1)); }
template <class T> void CPUCore<T>::set_1_xiy_c() { R.setC(SET_X_Y(1)); }
template <class T> void CPUCore<T>::set_1_xiy_d() { R.setD(SET_X_Y(1)); }
template <class T> void CPUCore<T>::set_1_xiy_e() { R.setE(SET_X_Y(1)); }
template <class T> void CPUCore<T>::set_1_xiy_h() { R.setH(SET_X_Y(1)); }
template <class T> void CPUCore<T>::set_1_xiy_l() { R.setL(SET_X_Y(1)); }
template <class T> void CPUCore<T>::set_2_xiy_a() { R.setA(SET_X_Y(2)); }
template <class T> void CPUCore<T>::set_2_xiy_b() { R.setB(SET_X_Y(2)); }
template <class T> void CPUCore<T>::set_2_xiy_c() { R.setC(SET_X_Y(2)); }
template <class T> void CPUCore<T>::set_2_xiy_d() { R.setD(SET_X_Y(2)); }
template <class T> void CPUCore<T>::set_2_xiy_e() { R.setE(SET_X_Y(2)); }
template <class T> void CPUCore<T>::set_2_xiy_h() { R.setH(SET_X_Y(2)); }
template <class T> void CPUCore<T>::set_2_xiy_l() { R.setL(SET_X_Y(2)); }
template <class T> void CPUCore<T>::set_3_xiy_a() { R.setA(SET_X_Y(3)); }
template <class T> void CPUCore<T>::set_3_xiy_b() { R.setB(SET_X_Y(3)); }
template <class T> void CPUCore<T>::set_3_xiy_c() { R.setC(SET_X_Y(3)); }
template <class T> void CPUCore<T>::set_3_xiy_d() { R.setD(SET_X_Y(3)); }
template <class T> void CPUCore<T>::set_3_xiy_e() { R.setE(SET_X_Y(3)); }
template <class T> void CPUCore<T>::set_3_xiy_h() { R.setH(SET_X_Y(3)); }
template <class T> void CPUCore<T>::set_3_xiy_l() { R.setL(SET_X_Y(3)); }
template <class T> void CPUCore<T>::set_4_xiy_a() { R.setA(SET_X_Y(4)); }
template <class T> void CPUCore<T>::set_4_xiy_b() { R.setB(SET_X_Y(4)); }
template <class T> void CPUCore<T>::set_4_xiy_c() { R.setC(SET_X_Y(4)); }
template <class T> void CPUCore<T>::set_4_xiy_d() { R.setD(SET_X_Y(4)); }
template <class T> void CPUCore<T>::set_4_xiy_e() { R.setE(SET_X_Y(4)); }
template <class T> void CPUCore<T>::set_4_xiy_h() { R.setH(SET_X_Y(4)); }
template <class T> void CPUCore<T>::set_4_xiy_l() { R.setL(SET_X_Y(4)); }
template <class T> void CPUCore<T>::set_5_xiy_a() { R.setA(SET_X_Y(5)); }
template <class T> void CPUCore<T>::set_5_xiy_b() { R.setB(SET_X_Y(5)); }
template <class T> void CPUCore<T>::set_5_xiy_c() { R.setC(SET_X_Y(5)); }
template <class T> void CPUCore<T>::set_5_xiy_d() { R.setD(SET_X_Y(5)); }
template <class T> void CPUCore<T>::set_5_xiy_e() { R.setE(SET_X_Y(5)); }
template <class T> void CPUCore<T>::set_5_xiy_h() { R.setH(SET_X_Y(5)); }
template <class T> void CPUCore<T>::set_5_xiy_l() { R.setL(SET_X_Y(5)); }
template <class T> void CPUCore<T>::set_6_xiy_a() { R.setA(SET_X_Y(6)); }
template <class T> void CPUCore<T>::set_6_xiy_b() { R.setB(SET_X_Y(6)); }
template <class T> void CPUCore<T>::set_6_xiy_c() { R.setC(SET_X_Y(6)); }
template <class T> void CPUCore<T>::set_6_xiy_d() { R.setD(SET_X_Y(6)); }
template <class T> void CPUCore<T>::set_6_xiy_e() { R.setE(SET_X_Y(6)); }
template <class T> void CPUCore<T>::set_6_xiy_h() { R.setH(SET_X_Y(6)); }
template <class T> void CPUCore<T>::set_6_xiy_l() { R.setL(SET_X_Y(6)); }
template <class T> void CPUCore<T>::set_7_xiy_a() { R.setA(SET_X_Y(7)); }
template <class T> void CPUCore<T>::set_7_xiy_b() { R.setB(SET_X_Y(7)); }
template <class T> void CPUCore<T>::set_7_xiy_c() { R.setC(SET_X_Y(7)); }
template <class T> void CPUCore<T>::set_7_xiy_d() { R.setD(SET_X_Y(7)); }
template <class T> void CPUCore<T>::set_7_xiy_e() { R.setE(SET_X_Y(7)); }
template <class T> void CPUCore<T>::set_7_xiy_h() { R.setH(SET_X_Y(7)); }
template <class T> void CPUCore<T>::set_7_xiy_l() { R.setL(SET_X_Y(7)); }


// RL r
template <class T> inline byte CPUCore<T>::RL(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | ((R.getF() & C_FLAG) ? 0x01 : 0);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RL_X(word x)
{
	byte res = RL(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RL_X_(word x) { memptr = x; return RL_X(x); }
template <class T> inline byte CPUCore<T>::RL_X_X() { return RL_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::RL_X_Y() { return RL_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::rl_a() { R.setA(RL(R.getA())); }
template <class T> void CPUCore<T>::rl_b() { R.setB(RL(R.getB())); }
template <class T> void CPUCore<T>::rl_c() { R.setC(RL(R.getC())); }
template <class T> void CPUCore<T>::rl_d() { R.setD(RL(R.getD())); }
template <class T> void CPUCore<T>::rl_e() { R.setE(RL(R.getE())); }
template <class T> void CPUCore<T>::rl_h() { R.setH(RL(R.getH())); }
template <class T> void CPUCore<T>::rl_l() { R.setL(RL(R.getL())); }

template <class T> void CPUCore<T>::rl_xhl()   { RL_X(R.HL); }
template <class T> void CPUCore<T>::rl_xix  () { RL_X_X(); }
template <class T> void CPUCore<T>::rl_xiy  () { RL_X_Y(); }

template <class T> void CPUCore<T>::rl_xix_a() { R.setA(RL_X_X()); }
template <class T> void CPUCore<T>::rl_xix_b() { R.setB(RL_X_X()); }
template <class T> void CPUCore<T>::rl_xix_c() { R.setC(RL_X_X()); }
template <class T> void CPUCore<T>::rl_xix_d() { R.setD(RL_X_X()); }
template <class T> void CPUCore<T>::rl_xix_e() { R.setE(RL_X_X()); }
template <class T> void CPUCore<T>::rl_xix_h() { R.setH(RL_X_X()); }
template <class T> void CPUCore<T>::rl_xix_l() { R.setL(RL_X_X()); }

template <class T> void CPUCore<T>::rl_xiy_a() { R.setA(RL_X_Y()); }
template <class T> void CPUCore<T>::rl_xiy_b() { R.setB(RL_X_Y()); }
template <class T> void CPUCore<T>::rl_xiy_c() { R.setC(RL_X_Y()); }
template <class T> void CPUCore<T>::rl_xiy_d() { R.setD(RL_X_Y()); }
template <class T> void CPUCore<T>::rl_xiy_e() { R.setE(RL_X_Y()); }
template <class T> void CPUCore<T>::rl_xiy_h() { R.setH(RL_X_Y()); }
template <class T> void CPUCore<T>::rl_xiy_l() { R.setL(RL_X_Y()); }


// RLC r
template <class T> inline byte CPUCore<T>::RLC(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | c;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RLC_X(word x)
{
	byte res = RLC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RLC_X_(word x) { memptr = x; return RLC_X(x); }
template <class T> inline byte CPUCore<T>::RLC_X_X() { return RLC_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::RLC_X_Y() { return RLC_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::rlc_a() { R.setA(RLC(R.getA())); }
template <class T> void CPUCore<T>::rlc_b() { R.setB(RLC(R.getB())); }
template <class T> void CPUCore<T>::rlc_c() { R.setC(RLC(R.getC())); }
template <class T> void CPUCore<T>::rlc_d() { R.setD(RLC(R.getD())); }
template <class T> void CPUCore<T>::rlc_e() { R.setE(RLC(R.getE())); }
template <class T> void CPUCore<T>::rlc_h() { R.setH(RLC(R.getH())); }
template <class T> void CPUCore<T>::rlc_l() { R.setL(RLC(R.getL())); }

template <class T> void CPUCore<T>::rlc_xhl()   { RLC_X(R.HL); }
template <class T> void CPUCore<T>::rlc_xix  () { RLC_X_X(); }
template <class T> void CPUCore<T>::rlc_xiy  () { RLC_X_Y(); }

template <class T> void CPUCore<T>::rlc_xix_a() { R.setA(RLC_X_X()); }
template <class T> void CPUCore<T>::rlc_xix_b() { R.setB(RLC_X_X()); }
template <class T> void CPUCore<T>::rlc_xix_c() { R.setC(RLC_X_X()); }
template <class T> void CPUCore<T>::rlc_xix_d() { R.setD(RLC_X_X()); }
template <class T> void CPUCore<T>::rlc_xix_e() { R.setE(RLC_X_X()); }
template <class T> void CPUCore<T>::rlc_xix_h() { R.setH(RLC_X_X()); }
template <class T> void CPUCore<T>::rlc_xix_l() { R.setL(RLC_X_X()); }

template <class T> void CPUCore<T>::rlc_xiy_a() { R.setA(RLC_X_Y()); }
template <class T> void CPUCore<T>::rlc_xiy_b() { R.setB(RLC_X_Y()); }
template <class T> void CPUCore<T>::rlc_xiy_c() { R.setC(RLC_X_Y()); }
template <class T> void CPUCore<T>::rlc_xiy_d() { R.setD(RLC_X_Y()); }
template <class T> void CPUCore<T>::rlc_xiy_e() { R.setE(RLC_X_Y()); }
template <class T> void CPUCore<T>::rlc_xiy_h() { R.setH(RLC_X_Y()); }
template <class T> void CPUCore<T>::rlc_xiy_l() { R.setL(RLC_X_Y()); }


// RR r
template <class T> inline byte CPUCore<T>::RR(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | ((R.getF() & C_FLAG) ? 0x80 : 0);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RR_X(word x)
{
	byte res = RR(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RR_X_(word x) { memptr = x; return RR_X(x); }
template <class T> inline byte CPUCore<T>::RR_X_X() { return RR_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::RR_X_Y() { return RR_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::rr_a() { R.setA(RR(R.getA())); }
template <class T> void CPUCore<T>::rr_b() { R.setB(RR(R.getB())); }
template <class T> void CPUCore<T>::rr_c() { R.setC(RR(R.getC())); }
template <class T> void CPUCore<T>::rr_d() { R.setD(RR(R.getD())); }
template <class T> void CPUCore<T>::rr_e() { R.setE(RR(R.getE())); }
template <class T> void CPUCore<T>::rr_h() { R.setH(RR(R.getH())); }
template <class T> void CPUCore<T>::rr_l() { R.setL(RR(R.getL())); }

template <class T> void CPUCore<T>::rr_xhl()   { RR_X(R.HL); }
template <class T> void CPUCore<T>::rr_xix  () { RR_X_X(); }
template <class T> void CPUCore<T>::rr_xiy  () { RR_X_Y(); }

template <class T> void CPUCore<T>::rr_xix_a() { R.setA(RR_X_X()); }
template <class T> void CPUCore<T>::rr_xix_b() { R.setB(RR_X_X()); }
template <class T> void CPUCore<T>::rr_xix_c() { R.setC(RR_X_X()); }
template <class T> void CPUCore<T>::rr_xix_d() { R.setD(RR_X_X()); }
template <class T> void CPUCore<T>::rr_xix_e() { R.setE(RR_X_X()); }
template <class T> void CPUCore<T>::rr_xix_h() { R.setH(RR_X_X()); }
template <class T> void CPUCore<T>::rr_xix_l() { R.setL(RR_X_X()); }

template <class T> void CPUCore<T>::rr_xiy_a() { R.setA(RR_X_Y()); }
template <class T> void CPUCore<T>::rr_xiy_b() { R.setB(RR_X_Y()); }
template <class T> void CPUCore<T>::rr_xiy_c() { R.setC(RR_X_Y()); }
template <class T> void CPUCore<T>::rr_xiy_d() { R.setD(RR_X_Y()); }
template <class T> void CPUCore<T>::rr_xiy_e() { R.setE(RR_X_Y()); }
template <class T> void CPUCore<T>::rr_xiy_h() { R.setH(RR_X_Y()); }
template <class T> void CPUCore<T>::rr_xiy_l() { R.setL(RR_X_Y()); }


// RRC r
template <class T> inline byte CPUCore<T>::RRC(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (c << 7);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RRC_X(word x)
{
	byte res = RRC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RRC_X_(word x) { memptr = x; return RRC_X(x); }
template <class T> inline byte CPUCore<T>::RRC_X_X() { return RRC_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::RRC_X_Y() { return RRC_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::rrc_a() { R.setA(RRC(R.getA())); }
template <class T> void CPUCore<T>::rrc_b() { R.setB(RRC(R.getB())); }
template <class T> void CPUCore<T>::rrc_c() { R.setC(RRC(R.getC())); }
template <class T> void CPUCore<T>::rrc_d() { R.setD(RRC(R.getD())); }
template <class T> void CPUCore<T>::rrc_e() { R.setE(RRC(R.getE())); }
template <class T> void CPUCore<T>::rrc_h() { R.setH(RRC(R.getH())); }
template <class T> void CPUCore<T>::rrc_l() { R.setL(RRC(R.getL())); }

template <class T> void CPUCore<T>::rrc_xhl()   { RRC_X(R.HL); }
template <class T> void CPUCore<T>::rrc_xix  () { RRC_X_X(); }
template <class T> void CPUCore<T>::rrc_xiy  () { RRC_X_Y(); }

template <class T> void CPUCore<T>::rrc_xix_a() { R.setA(RRC_X_X()); }
template <class T> void CPUCore<T>::rrc_xix_b() { R.setB(RRC_X_X()); }
template <class T> void CPUCore<T>::rrc_xix_c() { R.setC(RRC_X_X()); }
template <class T> void CPUCore<T>::rrc_xix_d() { R.setD(RRC_X_X()); }
template <class T> void CPUCore<T>::rrc_xix_e() { R.setE(RRC_X_X()); }
template <class T> void CPUCore<T>::rrc_xix_h() { R.setH(RRC_X_X()); }
template <class T> void CPUCore<T>::rrc_xix_l() { R.setL(RRC_X_X()); }

template <class T> void CPUCore<T>::rrc_xiy_a() { R.setA(RRC_X_Y()); }
template <class T> void CPUCore<T>::rrc_xiy_b() { R.setB(RRC_X_Y()); }
template <class T> void CPUCore<T>::rrc_xiy_c() { R.setC(RRC_X_Y()); }
template <class T> void CPUCore<T>::rrc_xiy_d() { R.setD(RRC_X_Y()); }
template <class T> void CPUCore<T>::rrc_xiy_e() { R.setE(RRC_X_Y()); }
template <class T> void CPUCore<T>::rrc_xiy_h() { R.setH(RRC_X_Y()); }
template <class T> void CPUCore<T>::rrc_xiy_l() { R.setL(RRC_X_Y()); }


// SLA r
template <class T> inline byte CPUCore<T>::SLA(byte reg)
{
	byte c = reg >> 7;
	reg <<= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLA_X(word x)
{
	byte res = SLA(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SLA_X_(word x) { memptr = x; return SLA_X(x); }
template <class T> inline byte CPUCore<T>::SLA_X_X() { return SLA_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::SLA_X_Y() { return SLA_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::sla_a() { R.setA(SLA(R.getA())); }
template <class T> void CPUCore<T>::sla_b() { R.setB(SLA(R.getB())); }
template <class T> void CPUCore<T>::sla_c() { R.setC(SLA(R.getC())); }
template <class T> void CPUCore<T>::sla_d() { R.setD(SLA(R.getD())); }
template <class T> void CPUCore<T>::sla_e() { R.setE(SLA(R.getE())); }
template <class T> void CPUCore<T>::sla_h() { R.setH(SLA(R.getH())); }
template <class T> void CPUCore<T>::sla_l() { R.setL(SLA(R.getL())); }

template <class T> void CPUCore<T>::sla_xhl()   { SLA_X(R.HL); }
template <class T> void CPUCore<T>::sla_xix  () { SLA_X_X(); }
template <class T> void CPUCore<T>::sla_xiy  () { SLA_X_Y(); }

template <class T> void CPUCore<T>::sla_xix_a() { R.setA(SLA_X_X()); }
template <class T> void CPUCore<T>::sla_xix_b() { R.setB(SLA_X_X()); }
template <class T> void CPUCore<T>::sla_xix_c() { R.setC(SLA_X_X()); }
template <class T> void CPUCore<T>::sla_xix_d() { R.setD(SLA_X_X()); }
template <class T> void CPUCore<T>::sla_xix_e() { R.setE(SLA_X_X()); }
template <class T> void CPUCore<T>::sla_xix_h() { R.setH(SLA_X_X()); }
template <class T> void CPUCore<T>::sla_xix_l() { R.setL(SLA_X_X()); }

template <class T> void CPUCore<T>::sla_xiy_a() { R.setA(SLA_X_Y()); }
template <class T> void CPUCore<T>::sla_xiy_b() { R.setB(SLA_X_Y()); }
template <class T> void CPUCore<T>::sla_xiy_c() { R.setC(SLA_X_Y()); }
template <class T> void CPUCore<T>::sla_xiy_d() { R.setD(SLA_X_Y()); }
template <class T> void CPUCore<T>::sla_xiy_e() { R.setE(SLA_X_Y()); }
template <class T> void CPUCore<T>::sla_xiy_h() { R.setH(SLA_X_Y()); }
template <class T> void CPUCore<T>::sla_xiy_l() { R.setL(SLA_X_Y()); }


// SLL r
template <class T> inline byte CPUCore<T>::SLL(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLL_X(word x)
{
	byte res = SLL(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SLL_X_(word x) { memptr = x; return SLL_X(x); }
template <class T> inline byte CPUCore<T>::SLL_X_X() { return SLL_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::SLL_X_Y() { return SLL_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::sll_a() { R.setA(SLL(R.getA())); }
template <class T> void CPUCore<T>::sll_b() { R.setB(SLL(R.getB())); }
template <class T> void CPUCore<T>::sll_c() { R.setC(SLL(R.getC())); }
template <class T> void CPUCore<T>::sll_d() { R.setD(SLL(R.getD())); }
template <class T> void CPUCore<T>::sll_e() { R.setE(SLL(R.getE())); }
template <class T> void CPUCore<T>::sll_h() { R.setH(SLL(R.getH())); }
template <class T> void CPUCore<T>::sll_l() { R.setL(SLL(R.getL())); }

template <class T> void CPUCore<T>::sll_xhl()   { SLL_X(R.HL); }
template <class T> void CPUCore<T>::sll_xix  () { SLL_X_X(); }
template <class T> void CPUCore<T>::sll_xiy  () { SLL_X_Y(); }

template <class T> void CPUCore<T>::sll_xix_a() { R.setA(SLL_X_X()); }
template <class T> void CPUCore<T>::sll_xix_b() { R.setB(SLL_X_X()); }
template <class T> void CPUCore<T>::sll_xix_c() { R.setC(SLL_X_X()); }
template <class T> void CPUCore<T>::sll_xix_d() { R.setD(SLL_X_X()); }
template <class T> void CPUCore<T>::sll_xix_e() { R.setE(SLL_X_X()); }
template <class T> void CPUCore<T>::sll_xix_h() { R.setH(SLL_X_X()); }
template <class T> void CPUCore<T>::sll_xix_l() { R.setL(SLL_X_X()); }

template <class T> void CPUCore<T>::sll_xiy_a() { R.setA(SLL_X_Y()); }
template <class T> void CPUCore<T>::sll_xiy_b() { R.setB(SLL_X_Y()); }
template <class T> void CPUCore<T>::sll_xiy_c() { R.setC(SLL_X_Y()); }
template <class T> void CPUCore<T>::sll_xiy_d() { R.setD(SLL_X_Y()); }
template <class T> void CPUCore<T>::sll_xiy_e() { R.setE(SLL_X_Y()); }
template <class T> void CPUCore<T>::sll_xiy_h() { R.setH(SLL_X_Y()); }
template <class T> void CPUCore<T>::sll_xiy_l() { R.setL(SLL_X_Y()); }


// SRA r
template <class T> inline byte CPUCore<T>::SRA(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (reg & 0x80);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRA_X(word x)
{
	byte res = SRA(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SRA_X_(word x) { memptr = x; return SRA_X(x); }
template <class T> inline byte CPUCore<T>::SRA_X_X() { return SRA_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::SRA_X_Y() { return SRA_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::sra_a() { R.setA(SRA(R.getA())); }
template <class T> void CPUCore<T>::sra_b() { R.setB(SRA(R.getB())); }
template <class T> void CPUCore<T>::sra_c() { R.setC(SRA(R.getC())); }
template <class T> void CPUCore<T>::sra_d() { R.setD(SRA(R.getD())); }
template <class T> void CPUCore<T>::sra_e() { R.setE(SRA(R.getE())); }
template <class T> void CPUCore<T>::sra_h() { R.setH(SRA(R.getH())); }
template <class T> void CPUCore<T>::sra_l() { R.setL(SRA(R.getL())); }

template <class T> void CPUCore<T>::sra_xhl()   { SRA_X(R.HL); }
template <class T> void CPUCore<T>::sra_xix  () { SRA_X_X(); }
template <class T> void CPUCore<T>::sra_xiy  () { SRA_X_Y(); }

template <class T> void CPUCore<T>::sra_xix_a() { R.setA(SRA_X_X()); }
template <class T> void CPUCore<T>::sra_xix_b() { R.setB(SRA_X_X()); }
template <class T> void CPUCore<T>::sra_xix_c() { R.setC(SRA_X_X()); }
template <class T> void CPUCore<T>::sra_xix_d() { R.setD(SRA_X_X()); }
template <class T> void CPUCore<T>::sra_xix_e() { R.setE(SRA_X_X()); }
template <class T> void CPUCore<T>::sra_xix_h() { R.setH(SRA_X_X()); }
template <class T> void CPUCore<T>::sra_xix_l() { R.setL(SRA_X_X()); }

template <class T> void CPUCore<T>::sra_xiy_a() { R.setA(SRA_X_Y()); }
template <class T> void CPUCore<T>::sra_xiy_b() { R.setB(SRA_X_Y()); }
template <class T> void CPUCore<T>::sra_xiy_c() { R.setC(SRA_X_Y()); }
template <class T> void CPUCore<T>::sra_xiy_d() { R.setD(SRA_X_Y()); }
template <class T> void CPUCore<T>::sra_xiy_e() { R.setE(SRA_X_Y()); }
template <class T> void CPUCore<T>::sra_xiy_h() { R.setH(SRA_X_Y()); }
template <class T> void CPUCore<T>::sra_xiy_l() { R.setL(SRA_X_Y()); }


// SRL R
template <class T> inline byte CPUCore<T>::SRL(byte reg)
{
	byte c = reg & 1;
	reg >>= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRL_X(word x)
{
	byte res = SRL(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SRL_X_(word x) { memptr = x; return SRL_X(x); }
template <class T> inline byte CPUCore<T>::SRL_X_X() { return SRL_X_(R.IX + ofst); }
template <class T> inline byte CPUCore<T>::SRL_X_Y() { return SRL_X_(R.IY + ofst); }

template <class T> void CPUCore<T>::srl_a() { R.setA(SRL(R.getA())); }
template <class T> void CPUCore<T>::srl_b() { R.setB(SRL(R.getB())); }
template <class T> void CPUCore<T>::srl_c() { R.setC(SRL(R.getC())); }
template <class T> void CPUCore<T>::srl_d() { R.setD(SRL(R.getD())); }
template <class T> void CPUCore<T>::srl_e() { R.setE(SRL(R.getE())); }
template <class T> void CPUCore<T>::srl_h() { R.setH(SRL(R.getH())); }
template <class T> void CPUCore<T>::srl_l() { R.setL(SRL(R.getL())); }

template <class T> void CPUCore<T>::srl_xhl()   { SRL_X(R.HL); }
template <class T> void CPUCore<T>::srl_xix  () { SRL_X_X(); }
template <class T> void CPUCore<T>::srl_xiy  () { SRL_X_Y(); }

template <class T> void CPUCore<T>::srl_xix_a() { R.setA(SRL_X_X()); }
template <class T> void CPUCore<T>::srl_xix_b() { R.setB(SRL_X_X()); }
template <class T> void CPUCore<T>::srl_xix_c() { R.setC(SRL_X_X()); }
template <class T> void CPUCore<T>::srl_xix_d() { R.setD(SRL_X_X()); }
template <class T> void CPUCore<T>::srl_xix_e() { R.setE(SRL_X_X()); }
template <class T> void CPUCore<T>::srl_xix_h() { R.setH(SRL_X_X()); }
template <class T> void CPUCore<T>::srl_xix_l() { R.setL(SRL_X_X()); }

template <class T> void CPUCore<T>::srl_xiy_a() { R.setA(SRL_X_Y()); }
template <class T> void CPUCore<T>::srl_xiy_b() { R.setB(SRL_X_Y()); }
template <class T> void CPUCore<T>::srl_xiy_c() { R.setC(SRL_X_Y()); }
template <class T> void CPUCore<T>::srl_xiy_d() { R.setD(SRL_X_Y()); }
template <class T> void CPUCore<T>::srl_xiy_e() { R.setE(SRL_X_Y()); }
template <class T> void CPUCore<T>::srl_xiy_h() { R.setH(SRL_X_Y()); }
template <class T> void CPUCore<T>::srl_xiy_l() { R.setL(SRL_X_Y()); }


// RLA RLCA RRA RRCA
template <class T> void CPUCore<T>::rla()
{
	byte c = R.getF() & C_FLAG;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x80) ? C_FLAG : 0));
	R.setA((R.getA() << 1) | (c ? 1 : 0));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
}
template <class T> void CPUCore<T>::rlca()
{
	R.setA((R.getA() << 1) | (R.getA() >> 7));
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() & (Y_FLAG | X_FLAG | C_FLAG)));
}
template <class T> void CPUCore<T>::rra()
{
	byte c = R.getF() & C_FLAG;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x01) ? C_FLAG : 0));
	R.setA((R.getA() >> 1) | (c ? 0x80 : 0));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
}
template <class T> void CPUCore<T>::rrca()
{
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() &  C_FLAG));
	R.setA((R.getA() >> 1) | (R.getA() << 7));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
}


// RLD
template <class T> void CPUCore<T>::rld()
{
	byte val = RDMEM(R.HL);
	memptr = R.HL + 1;
	T::RLD_DELAY();
	WRMEM(R.HL, (val << 4) | (R.getA() & 0x0F));
	R.setA((R.getA() & 0xF0) | (val >> 4));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
}

// RRD
template <class T> void CPUCore<T>::rrd()
{
	byte val = RDMEM(R.HL);
	memptr = R.HL + 1;
	T::RLD_DELAY();
	WRMEM(R.HL, (val >> 4) | (R.getA() << 4));
	R.setA((R.getA() & 0xF0) | (val & 0x0F));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
}


// PUSH ss
template <class T> inline void CPUCore<T>::PUSH(word reg)
{
	T::PUSH_DELAY();
	WRMEM(R.SP - 1, reg >> 8);
	WRMEM(R.SP - 2, reg & 255);
	R.SP -= 2;
}
template <class T> void CPUCore<T>::push_af() { PUSH(R.AF); }
template <class T> void CPUCore<T>::push_bc() { PUSH(R.BC); }
template <class T> void CPUCore<T>::push_de() { PUSH(R.DE); }
template <class T> void CPUCore<T>::push_hl() { PUSH(R.HL); }
template <class T> void CPUCore<T>::push_ix() { PUSH(R.IX); }
template <class T> void CPUCore<T>::push_iy() { PUSH(R.IY); }


// POP ss
template <class T> inline word CPUCore<T>::POP()
{
	word res = RDMEM(R.SP + 0);
	res     += RDMEM(R.SP + 1) << 8;
	R.SP += 2;
	return res;
}
template <class T> void CPUCore<T>::pop_af() { R.AF = POP(); }
template <class T> void CPUCore<T>::pop_bc() { R.BC = POP(); }
template <class T> void CPUCore<T>::pop_de() { R.DE = POP(); }
template <class T> void CPUCore<T>::pop_hl() { R.HL = POP(); }
template <class T> void CPUCore<T>::pop_ix() { R.IX = POP(); }
template <class T> void CPUCore<T>::pop_iy() { R.IY = POP(); }


// CALL nn / CALL cc,nn
template <class T> inline void CPUCore<T>::CALL()
{
	memptr = RD_WORD_PC();
	PUSH(R.PC);
	R.PC = memptr;
}
template <class T> inline void CPUCore<T>::SKIP_JP()
{
	memptr = RD_WORD_PC();
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
	PUSH(R.PC); memptr = x; R.PC = x;
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
	memptr = POP();
	R.PC = memptr;
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
	T::RETN_DELAY();
	R.IFF1 = R.nextIFF1 = R.IFF2;
	slowInstructions = 2;
	RET();
}
template <class T> void CPUCore<T>::retn()
{
	T::RETN_DELAY();
	R.IFF1 = R.nextIFF1 = R.IFF2;
	slowInstructions = 2;
	RET();
}


// JP ss
template <class T> void CPUCore<T>::jp_hl() { R.PC = R.HL; }
template <class T> void CPUCore<T>::jp_ix() { R.PC = R.IX; }
template <class T> void CPUCore<T>::jp_iy() { R.PC = R.IY; }

// JP nn / JP cc,nn
template <class T> inline void CPUCore<T>::JP()
{
	memptr = RD_WORD_PC();
	R.PC = memptr;
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
	offset ofst = RDMEM_OPCODE(R.PC);
	R.PC += ofst + 1;
	memptr = R.PC;
	T::ADD_16_8_DELAY();
}
template <class T> inline void CPUCore<T>::SKIP_JR()
{
	RDMEM_OPCODE(R.PC++); // ignore return value
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
	byte b = R.getB();
	R.setB(--b);
	if (b) JR(); else SKIP_JR();
}

// EX (SP),ss
template <class T> inline void CPUCore<T>::EX_SP(word& reg)
{
	memptr  = RDMEM(R.SP + 0);
	memptr += RDMEM(R.SP + 1) << 8;
	T::SMALL_DELAY();
	WRMEM(R.SP + 1, reg >> 8);
	WRMEM(R.SP + 0, reg & 255);
	reg = memptr;
	T::EX_SP_HL_DELAY();
}
template <class T> void CPUCore<T>::ex_xsp_hl() { EX_SP(R.HL); }
template <class T> void CPUCore<T>::ex_xsp_ix() { EX_SP(R.IX); }
template <class T> void CPUCore<T>::ex_xsp_iy() { EX_SP(R.IY); }


// IN r,(c)
template <class T> inline byte CPUCore<T>::IN()
{
	byte res = READ_PORT(R.BC);
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[res]);
	return res;
}
template <class T> void CPUCore<T>::in_a_c() { R.setA(IN()); }
template <class T> void CPUCore<T>::in_b_c() { R.setB(IN()); }
template <class T> void CPUCore<T>::in_c_c() { R.setC(IN()); }
template <class T> void CPUCore<T>::in_d_c() { R.setD(IN()); }
template <class T> void CPUCore<T>::in_e_c() { R.setE(IN()); }
template <class T> void CPUCore<T>::in_h_c() { R.setH(IN()); }
template <class T> void CPUCore<T>::in_l_c() { R.setL(IN()); }
template <class T> void CPUCore<T>::in_0_c() { IN(); } // discard result

// IN a,(n)
template <class T> void CPUCore<T>::in_a_byte()
{
	word y = RDMEM_OPCODE(R.PC++) + 256 * R.getA();
	R.setA(READ_PORT(y));
}


// OUT (c),r
template <class T> inline void CPUCore<T>::OUT(byte val)
{
	WRITE_PORT(R.BC, val);
}
template <class T> void CPUCore<T>::out_c_a()   { OUT(R.getA()); }
template <class T> void CPUCore<T>::out_c_b()   { OUT(R.getB()); }
template <class T> void CPUCore<T>::out_c_c()   { OUT(R.getC()); }
template <class T> void CPUCore<T>::out_c_d()   { OUT(R.getD()); }
template <class T> void CPUCore<T>::out_c_e()   { OUT(R.getE()); }
template <class T> void CPUCore<T>::out_c_h()   { OUT(R.getH()); }
template <class T> void CPUCore<T>::out_c_l()   { OUT(R.getL()); }
template <class T> void CPUCore<T>::out_c_0()   { OUT(0);        }

// OUT (n),a
template <class T> void CPUCore<T>::out_byte_a()
{
	word y = RDMEM_OPCODE(R.PC++) + 256 * R.getA();
	WRITE_PORT(y, R.getA());
}


// block CP
template <class T> inline void CPUCore<T>::BLOCK_CP(bool increase, bool repeat)
{
	byte val = RDMEM(R.HL);
	T::BLOCK_DELAY();
	byte res = R.getA() - val;
	if (increase) R.HL++; else R.HL--;
	R.BC--;
	byte f = (R.getF() & C_FLAG) |
	         ((R.getA() ^ val ^ res) & H_FLAG) |
	         ZSTable[res] |
	         N_FLAG;
	if (f & H_FLAG) res -= 1;
	if (res & 0x02) f |= Y_FLAG; // bit 1 -> flag 5
	if (res & 0x08) f |= X_FLAG; // bit 3 -> flag 3
	if (R.BC)       f |= V_FLAG;
	R.setF(f);
	if (repeat && R.BC && !(f & Z_FLAG)) { T::BLOCK_DELAY(); R.PC -= 2; }
}
template <class T> void CPUCore<T>::cpd()  { BLOCK_CP(false, false); }
template <class T> void CPUCore<T>::cpi()  { BLOCK_CP(true,  false); }
template <class T> void CPUCore<T>::cpdr() { BLOCK_CP(false, true ); }
template <class T> void CPUCore<T>::cpir() { BLOCK_CP(true,  true ); }


// block LD
template <class T> inline void CPUCore<T>::BLOCK_LD(bool increase, bool repeat)
{
	byte val = RDMEM(R.HL);
	WRMEM(R.DE, val);
	T::LDI_DELAY();
	if (increase) { R.HL++; R.DE++; } else { R.HL--; R.DE--; }
	R.BC--;
	byte f = R.getF() & (S_FLAG | Z_FLAG | C_FLAG);
	if ((R.getA() + val) & 0x02) f |= Y_FLAG;	// bit 1 -> flag 5
	if ((R.getA() + val) & 0x08) f |= X_FLAG;	// bit 3 -> flag 3
	if (R.BC) f |= V_FLAG;
	R.setF(f);
	if (repeat && R.BC) { T::BLOCK_DELAY(); R.PC -= 2; }
}
template <class T> void CPUCore<T>::ldd()  { BLOCK_LD(false, false); }
template <class T> void CPUCore<T>::ldi()  { BLOCK_LD(true,  false); }
template <class T> void CPUCore<T>::lddr() { BLOCK_LD(false, true ); }
template <class T> void CPUCore<T>::ldir() { BLOCK_LD(true,  true ); }


// block IN
template <class T> inline void CPUCore<T>::BLOCK_IN(bool increase, bool repeat)
{
	T::SMALL_DELAY();
	byte val = READ_PORT(R.BC);
	byte b = R.getB() - 1; R.setB(b);
	WRMEM(R.HL, val);
	if (increase) R.HL++; else R.HL--;
	byte f = ZSTable[b];
	if (val & S_FLAG) f |= N_FLAG;
	int k = val + ((R.getC() + (increase ? 1 : -1)) & 0xFF);
	if (k & 0x100) f |= H_FLAG | C_FLAG;
	R.setF(f | ZSPXYTable[(k & 0x07) ^ b] & P_FLAG);
	if (repeat && b) { T::BLOCK_DELAY(); R.PC -= 2; }
}
template <class T> void CPUCore<T>::ind()  { BLOCK_IN(false, false); }
template <class T> void CPUCore<T>::ini()  { BLOCK_IN(true,  false); }
template <class T> void CPUCore<T>::indr() { BLOCK_IN(false, true ); }
template <class T> void CPUCore<T>::inir() { BLOCK_IN(true,  true ); }


// block OUT
template <class T> inline void CPUCore<T>::BLOCK_OUT(bool increase, bool repeat)
{
	T::SMALL_DELAY();
	byte val = RDMEM(R.HL);
	byte b = R.getB() - 1; R.setB(b);
	WRITE_PORT(R.BC, val);
	if (increase) R.HL++; else R.HL--;
	byte f = ZSXYTable[b];
	if (val & S_FLAG) f |= N_FLAG;
	int k = val + R.getL();
	if (k & 0x100) f |= H_FLAG | C_FLAG;
	R.setF(f | ZSPXYTable[(k & 0x07) ^ b] & P_FLAG);
	if (repeat && b) { T::BLOCK_DELAY(); R.PC -= 2; }
}
template <class T> void CPUCore<T>::outd() { BLOCK_OUT(false, false); }
template <class T> void CPUCore<T>::outi() { BLOCK_OUT(true,  false); }
template <class T> void CPUCore<T>::otdr() { BLOCK_OUT(false, true ); }
template <class T> void CPUCore<T>::otir() { BLOCK_OUT(true,  true ); }


// various
template <class T> void CPUCore<T>::nop() { }
template <class T> void CPUCore<T>::ccf()
{
	R.setF(((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG) |
	        ((R.getF() & C_FLAG) ? H_FLAG : 0)) |
	        (R.getA() & (X_FLAG | Y_FLAG))                  ) ^ C_FLAG);
}
template <class T> void CPUCore<T>::cpl()
{
	R.setA(R.getA() ^ 0xFF);
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	       H_FLAG | N_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));

}
template <class T> void CPUCore<T>::daa()
{
	int i = R.getA();
	if (R.getF() & C_FLAG) i |= 0x100;
	if (R.getF() & H_FLAG) i |= 0x200;
	if (R.getF() & N_FLAG) i |= 0x400;
	R.AF = DAATable[i];
}
template <class T> void CPUCore<T>::neg()
{
	 byte i = R.getA();
	 R.setA(0);
	 SUB(i);
}
template <class T> void CPUCore<T>::scf()
{
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       C_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));
}

template <class T> void CPUCore<T>::ex_af_af()
{
	std::swap(R.AF, R.AF2);
}
template <class T> void CPUCore<T>::ex_de_hl()
{
	std::swap(R.DE, R.HL);
}
template <class T> void CPUCore<T>::exx()
{
	std::swap(R.BC, R.BC2);
	std::swap(R.DE, R.DE2);
	std::swap(R.HL, R.HL2);
}

template <class T> void CPUCore<T>::di()
{
	T::DI_DELAY();
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
	R.setA(R.I);
	R.setF((R.getF() & C_FLAG) |
	       ZSXYTable[R.getA()] |
	       (R.IFF2 ? V_FLAG : 0));
}
template <class T> void CPUCore<T>::ld_a_r()
{
	T::SMALL_DELAY();
	R.setA((R.R & 0x7f) | (R.R2 & 0x80));
	R.setF((R.getF() & C_FLAG) |
	       ZSXYTable[R.getA()] |
	       (R.IFF2 ? V_FLAG : 0));
}

// LD I/R,A
template <class T> void CPUCore<T>::ld_i_a()
{
	T::SMALL_DELAY();
	R.I = R.getA();
}
template <class T> void CPUCore<T>::ld_r_a()
{
	T::SMALL_DELAY();
	R.R = R.R2 = R.getA();
}

// MULUB A,r
template <class T> inline void CPUCore<T>::MULUB(byte reg)
{
	// TODO check flags
	T::clock += 12;
	R.HL = (word)R.getA() * reg;
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (R.HL ? 0 : Z_FLAG) |
	       ((R.HL & 0x8000) ? C_FLAG : 0));
}
template <class T> void CPUCore<T>::mulub_a_xhl() { } // TODO
template <class T> void CPUCore<T>::mulub_a_a()   { } // TODO
template <class T> void CPUCore<T>::mulub_a_b()   { MULUB(R.getB()); }
template <class T> void CPUCore<T>::mulub_a_c()   { MULUB(R.getC()); }
template <class T> void CPUCore<T>::mulub_a_d()   { MULUB(R.getD()); }
template <class T> void CPUCore<T>::mulub_a_e()   { MULUB(R.getE()); }
template <class T> void CPUCore<T>::mulub_a_h()   { } // TODO
template <class T> void CPUCore<T>::mulub_a_l()   { } // TODO

// MULUW HL,ss
template <class T> inline void CPUCore<T>::MULUW(word reg)
{
	// TODO check flags
	T::clock += 34;
	unsigned long res = (unsigned long)R.HL * reg;
	R.DE = res >> 16;
	R.HL = res & 0xffff;
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (res ? 0 : Z_FLAG) |
	       ((res & 0x80000000) ? C_FLAG : 0));
}
template <class T> void CPUCore<T>::muluw_hl_bc() { MULUW(R.BC); }
template <class T> void CPUCore<T>::muluw_hl_de() { } // TODO
template <class T> void CPUCore<T>::muluw_hl_hl() { } // TODO
template <class T> void CPUCore<T>::muluw_hl_sp() { MULUW(R.SP); }


// prefixes
template <class T> void CPUCore<T>::dd_cb()
{
	ofst = RDMEM_OPCODE(R.PC++);
	byte opcode = RDMEM_OPCODE(R.PC++);
	T::DD_CB_DELAY();
	(this->*opcode_dd_cb[opcode])();
}

template <class T> void CPUCore<T>::fd_cb()
{
	ofst = RDMEM_OPCODE(R.PC++);
	byte opcode = RDMEM_OPCODE(R.PC++);
	T::DD_CB_DELAY();
	(this->*opcode_fd_cb[opcode])();
}

template <class T> void CPUCore<T>::cb()
{
	byte opcode = RDMEM_OPCODE(R.PC++);
	M1Cycle();
	(this->*opcode_cb[opcode])();
}

template <class T> void CPUCore<T>::ed()
{
	byte opcode = RDMEM_OPCODE(R.PC++);
	M1Cycle();
	(this->*opcode_ed[opcode])();
}

template <class T> void CPUCore<T>::dd()
{
	byte opcode = RDMEM_OPCODE(R.PC++);
	if ((opcode != 0xCB) && (opcode != 0xDD) && (opcode != 0xFD)) {
		M1Cycle();
	} else {
		T::SMALL_DELAY();
	}
	(this->*opcode_dd[opcode])();
}
template <class T> void CPUCore<T>::dd2()
{
	byte opcode = RDMEM_OPCODE(R.PC++);
	T::SMALL_DELAY();
	(this->*opcode_dd[opcode])();
}

template <class T> void CPUCore<T>::fd()
{
	byte opcode = RDMEM_OPCODE(R.PC++);
	if ((opcode != 0xCB) && (opcode != 0xDD) && (opcode != 0xFD)) {
		M1Cycle();
	} else {
		T::SMALL_DELAY();
	}
	(this->*opcode_fd[opcode])();
}
template <class T> void CPUCore<T>::fd2()
{
	byte opcode = RDMEM_OPCODE(R.PC++);
	T::SMALL_DELAY();
	(this->*opcode_fd[opcode])();
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

} // namespace openmsx

