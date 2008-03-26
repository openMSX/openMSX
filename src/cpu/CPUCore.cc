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
#include "Thread.hh"
#include "likely.hh"
#include "inline.hh"
#include "build-info.hh"
#include <iomanip>
#include <iostream>
#include <cassert>
#include <string.h>

using std::string;

namespace openmsx {

typedef signed char offset;

// This function only exists as a workaround for a bug in g++-4.2.x, see
//   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=34336
// This can be removed once g++-4.2.4 is released.
static BooleanSetting* createFreqLockedSetting(
	CommandController& commandController, const string& name)
{
	return new BooleanSetting(commandController,
	        name + "_freq_locked",
	        "real (locked) or custom (unlocked) " + name + " frequency",
	        true);
}

template <class T> CPUCore<T>::CPUCore(
		MSXMotherBoard& motherboard_, const string& name,
		const BooleanSetting& traceSetting_, const EmuTime& time)
	: T(time, motherboard_.getScheduler())
	, motherboard(motherboard_)
	, scheduler(motherboard.getScheduler())
	, interface(NULL)
	, traceSetting(traceSetting_)
	, freqLocked(createFreqLockedSetting(
		motherboard.getCommandController(), name))
	, freqValue(new IntegerSetting(motherboard.getCommandController(),
	        name + "_freq",
	        "custom " + name + " frequency (only valid when unlocked)",
	        T::CLOCK_FREQ, 1000000, 100000000))
	, freq(T::CLOCK_FREQ)
	, NMIStatus(0)
	, IRQStatus(0)
	, nmiEdge(false)
	, exitLoop(false)
{
	if (freqLocked->getValue()) {
		// locked
		T::setFreq(T::CLOCK_FREQ);
	} else {
		// unlocked
		T::setFreq(freqValue->getValue());
	}

	freqLocked->attach(*this);
	freqValue->attach(*this);
	doReset(time);
}

template <class T> CPUCore<T>::~CPUCore()
{
	freqValue->detach(*this);
	freqLocked->detach(*this);
}

template <class T> void CPUCore<T>::setInterface(MSXCPUInterface* interf)
{
	interface = interf;
}

template <class T> void CPUCore<T>::warp(const EmuTime& time)
{
	assert(T::getTimeFast() <= time);
	T::setTime(time);
}

template <class T> const EmuTime& CPUCore<T>::getCurrentTime() const
{
	return T::getTime();
}

template <class T> void CPUCore<T>::invalidateMemCache(unsigned start, unsigned size)
{
	unsigned first = start / CacheLine::SIZE;
	unsigned num = (size + CacheLine::SIZE - 1) / CacheLine::SIZE;
	memset(&readCacheLine  [first], 0, num * sizeof(byte*)); // NULL
	memset(&writeCacheLine [first], 0, num * sizeof(byte*)); //
	memset(&readCacheTried [first], 0, num * sizeof(bool));  // FALSE
	memset(&writeCacheTried[first], 0, num * sizeof(bool));  //
}

template <class T> void CPUCore<T>::doReset(const EmuTime& time)
{
	// AF and SP are 0xFFFF
	// PC, R, IFF1, IFF2, HALT and IM are 0x0
	// all others are random
	R.setAF(0xFFFF);
	R.setBC(0xFFFF);
	R.setDE(0xFFFF);
	R.setHL(0xFFFF);
	R.setIX(0xFFFF);
	R.setIY(0xFFFF);
	R.setPC(0x0000);
	R.setSP(0xFFFF);
	R.setAF2(0xFFFF);
	R.setBC2(0xFFFF);
	R.setDE2(0xFFFF);
	R.setHL2(0xFFFF);
	R.setNextIFF1(false);
	R.setIFF1(false);
	R.setIFF2(false);
	R.setHALT(false);
	R.setIM(0);
	R.setI(0x00);
	R.setR(0x00);
	memptr = 0xFFFF;
	invalidateMemCache(0x0000, 0x10000);

	assert(T::getTimeFast() <= time);
	T::setTime(time);

	assert(NMIStatus == 0); // other devices must reset their NMI source
	assert(IRQStatus == 0); // other devices must reset their IRQ source
}

// I believe the following two methods are thread safe even without any
// locking. The worst that can happen is that we occasionally needlessly
// exit the CPU loop, but that's harmless
//  TODO thread issues are always tricky, can someone confirm this really
//       is thread safe
template <class T> void CPUCore<T>::exitCPULoopAsync()
{
	// can get called from non-main threads
	exitLoop = true;
}
template <class T> void CPUCore<T>::exitCPULoopSync()
{
	assert(Thread::isMainThread());
	exitLoop = true;
	T::enableLimit(false);
}
template <class T> inline bool CPUCore<T>::needExitCPULoop()
{
	// always executed in main thread
	if (unlikely(exitLoop)) {
		exitLoop = false;
		return true;
	}
	return false;
}

template <class T> void CPUCore<T>::setSlowInstructions()
{
	slowInstructions = 2;
	T::enableLimit(false);
}

template <class T> void CPUCore<T>::raiseIRQ()
{
	assert(IRQStatus >= 0);
	if (IRQStatus == 0) {
		setSlowInstructions();
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
	// NMIs are currently disabled, they are anyway not used in MSX and
	// not having to check for them allows to emulate slightly faster
	assert(false);
	assert(NMIStatus >= 0);
	if (NMIStatus == 0) {
		nmiEdge = true;
		setSlowInstructions();
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
	T::advanceTime(time);
}

template <class T> void CPUCore<T>::setNextSyncPoint(const EmuTime& time)
{
	T::setLimit(time);
}

template <class T> void CPUCore<T>::doBreak()
{
	if (!breaked) {
		breaked = true;

		motherboard.block();

		motherboard.getMSXCliComm().update(CliComm::STATUS, "cpu", "suspended");
		motherboard.getEventDistributor().distributeEvent(
			new SimpleEvent<OPENMSX_BREAK_EVENT>());
	}
}

template <class T> void CPUCore<T>::doStep()
{
	if (breaked) {
		step = true;
		doContinue2();
	}
}

template <class T> void CPUCore<T>::doContinue()
{
	if (breaked) {
		continued = true;
		doContinue2();
	}
}

template <class T> void CPUCore<T>::doContinue2()
{
	breaked = false;
	motherboard.getMSXCliComm().update(CliComm::STATUS, "cpu", "running");
	motherboard.unblock();
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
	word address = (tokens.size() < 3) ? R.getPC() : tokens[2]->getInt();
	byte outBuf[4];
	std::string dasmOutput;
	unsigned len = dasm(*interface, address, outBuf, dasmOutput,
	               T::getTimeFast());
	result.addListElement(dasmOutput);
	char tmp[3]; tmp[2] = 0;
	for (unsigned i = 0; i < len; ++i) {
		toHex(outBuf[i], tmp);
		result.addListElement(tmp);
	}
}

template <class T> void CPUCore<T>::update(const Setting& setting)
{
	if (&setting == freqLocked.get()) {
		if (freqLocked->getValue()) {
			// locked
			T::setFreq(freq);
		} else {
			// unlocked
			T::setFreq(freqValue->getValue());
		}
	} else if (&setting == freqValue.get()) {
		if (!freqLocked->getValue()) {
			T::setFreq(freqValue->getValue());
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
		T::setFreq(freq);
	}
}


template <class T> inline byte CPUCore<T>::READ_PORT(unsigned port)
{
	memptr = port + 1; // not 16-bit
	T::PRE_IO(port);
	EmuTime time = T::getTimeFast();
	scheduler.schedule(time);
	byte result = interface->readIO(port, time);
	T::POST_IO(port);
	return result;
}

template <class T> inline void CPUCore<T>::WRITE_PORT(unsigned port, byte value)
{
	memptr = port + 1; // not 16-bit
	T::PRE_IO(port);
	EmuTime time = T::getTimeFast();
	scheduler.schedule(time);
	interface->writeIO(port, value, time);
	T::POST_IO(port);
}

template <class T> byte CPUCore<T>::RDMEM_OPCODEslow(unsigned address)
{
	// not cached
	unsigned high = address >> CacheLine::BITS;
	if (!readCacheTried[high]) {
		// try to cache now
		unsigned addrBase = address & CacheLine::HIGH;
		if (const byte* line = interface->getReadCacheLine(addrBase)) {
			// cached ok
			T::PRE_RDMEM_OPCODE(address);
			T::POST_MEM(address);
			readCacheLine[high] = line - addrBase;
			return readCacheLine[high][address];
		}
	}
	// uncacheable
	readCacheTried[high] = true;
	T::PRE_RDMEM_OPCODE(address);
	EmuTime time = T::getTimeFast();
	scheduler.schedule(time);
	byte result = interface->readMem(address, time);
	T::POST_MEM(address);
	return result;
}
template <class T> ALWAYS_INLINE byte CPUCore<T>::RDMEM_OPCODE()
{
	unsigned address = R.getPC();
	R.setPC(address + 1);
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(line != NULL)) {
		// cached, fast path
		T::PRE_RDMEM_OPCODE(address);
		T::POST_MEM(address);
		return line[address];
	} else {
		return RDMEM_OPCODEslow(address); // not inlined
	}
}

static ALWAYS_INLINE unsigned read16LE(const byte* p)
{
	if (OPENMSX_BIGENDIAN || !OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		return p[0] + 256 * p[1];
	} else {
		return *reinterpret_cast<const word*>(p);
	}
}

static ALWAYS_INLINE void write16LE(byte* p, unsigned value)
{
	if (OPENMSX_BIGENDIAN || !OPENMSX_UNALIGNED_MEMORY_ACCESS) {
		p[0] = value & 0xff;
		p[1] = value >> 8;
	} else {
		*reinterpret_cast<word*>(p) = value;
	}
}

template <class T> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD_PC()
{
	unsigned addr = R.getPC();
	const byte* line = readCacheLine[addr >> CacheLine::BITS];
	if (likely(((addr & CacheLine::LOW) != CacheLine::LOW) &&
		   (line != NULL))) {
		// fast path: cached and two bytes in same cache line
		T::PRE_RDMEM_OPCODE(addr);
		T::PRE_RDMEM_OPCODE(addr); // same addr twice, ok
		T::POST_MEM(addr);
		T::POST_MEM(addr);
		R.setPC(addr + 2);
		return read16LE(&line[addr]);
	} else {
		// slow path, not inline
		return RD_WORD_PC_slow();
	}
}

template <class T> unsigned CPUCore<T>::RD_WORD_PC_slow()
{
	unsigned res = RDMEM_OPCODE();
	res         += RDMEM_OPCODE() << 8;
	return res;
}

template <class T> byte CPUCore<T>::RDMEMslow(unsigned address)
{
	// not cached
	unsigned high = address >> CacheLine::BITS;
	if (!readCacheTried[high]) {
		// try to cache now
		unsigned addrBase = address & CacheLine::HIGH;
		if (const byte* line = interface->getReadCacheLine(addrBase)) {
			// cached ok
			T::PRE_RDMEM(address);
			T::POST_MEM(address);
			readCacheLine[high] = line - addrBase;
			return readCacheLine[high][address];
		}
	}
	// uncacheable
	readCacheTried[high] = true;
	T::PRE_RDMEM(address);
	EmuTime time = T::getTimeFast();
	scheduler.schedule(time);
	byte result = interface->readMem(address, time);
	T::POST_MEM(address);
	return result;
}
template <class T> ALWAYS_INLINE byte CPUCore<T>::RDMEM(unsigned address)
{
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(line != NULL)) {
		// cached, fast path
		T::PRE_RDMEM(address);
		T::POST_MEM(address);
		return line[address];
	} else {
		return RDMEMslow(address); // not inlined
	}
}

template <class T> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD(unsigned address)
{
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) &&
		   (line != NULL))) {
		// fast path: cached and two bytes in same cache line
		T::PRE_RDMEM(address);
		T::PRE_RDMEM(address); // same addr twice, ok
		T::POST_MEM(address);
		T::POST_MEM(address);
		return read16LE(&line[address]);
	} else {
		// slow path, not inline
		return RD_WORD_slow(address);
	}
}

template <class T> unsigned CPUCore<T>::RD_WORD_slow(unsigned address)
{
	unsigned res = RDMEM(address);
	res         += RDMEM((address + 1) & 0xFFFF) << 8;
	return res;
}

template <class T> void CPUCore<T>::WRMEMslow(unsigned address, byte value)
{
	// not cached
	unsigned high = address >> CacheLine::BITS;
	if (!writeCacheTried[high]) {
		// try to cache now
		unsigned addrBase = address & CacheLine::HIGH;
		if (byte* line = interface->getWriteCacheLine(addrBase)) {
			// cached ok
			T::PRE_WRMEM(address);
			T::POST_MEM(address);
			writeCacheLine[high] = line - addrBase;
			writeCacheLine[high][address] = value;
			return;
		}
	}
	// uncacheable
	writeCacheTried[high] = true;
	T::PRE_WRMEM(address);
	EmuTime time = T::getTimeFast();
	scheduler.schedule(time);
	interface->writeMem(address, value, time);
	T::POST_MEM(address);
}
template <class T> ALWAYS_INLINE void CPUCore<T>::WRMEM(unsigned address, byte value)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(line != NULL)) {
		// cached, fast path
		T::PRE_WRMEM(address);
		T::POST_MEM(address);
		line[address] = value;
	} else {
		WRMEMslow(address, value);	// not inlined
	}
}

template <class T> ALWAYS_INLINE void CPUCore<T>::WR_WORD(
	unsigned address, unsigned value)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) &&
		   (line != NULL))) {
		// fast path: cached and two bytes in same cache line
		T::PRE_WRMEM(address);
		T::PRE_WRMEM(address); // same addr twice, ok
		T::POST_MEM(address);
		T::POST_MEM(address);
		write16LE(&line[address], value);
	} else {
		// slow path, not inline
		WR_WORD_slow(address, value);
	}
}

template <class T> void CPUCore<T>::WR_WORD_slow(unsigned address, unsigned value)
{
	WRMEM( address,               value & 255);
	WRMEM((address + 1) & 0xFFFF, value >> 8);
}

template <class T> ALWAYS_INLINE void CPUCore<T>::WR_WORD_rev(
	unsigned address, unsigned value)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) &&
		   (line != NULL))) {
		// fast path: cached and two bytes in same cache line
		T::PRE_WRMEM(address);
		T::PRE_WRMEM(address); // same addr twice, ok
		T::POST_MEM(address);
		T::POST_MEM(address);
		write16LE(&line[address], value);
	} else {
		// slow path, not inline
		WR_WORD_rev_slow(address, value);
	}
}

// same as WR_WORD, but writes high byte first
template <class T> void CPUCore<T>::WR_WORD_rev_slow(
	unsigned address, unsigned value)
{
	WRMEM((address + 1) & 0xFFFF, value >> 8);
	WRMEM( address,               value & 255);
}

template <class T> inline void CPUCore<T>::M1Cycle()
{
	T::M1_DELAY();
	R.incR(1);
}

// NMI interrupt
template <class T> inline void CPUCore<T>::nmi()
{
	PUSH(R.getPC());
	R.setHALT(false);
	R.setIFF1(false);
	R.setNextIFF1(false);
	R.setPC(0x0066);
	T::NMI_DELAY();
	M1Cycle();
}

// IM0 interrupt
template <class T> inline void CPUCore<T>::irq0()
{
	// TODO current implementation only works for 1-byte instructions
	//      ok for MSX
	R.setHALT(false);
	R.di();
	T::IM0_DELAY();
	executeInstruction1(interface->readIRQVector());
}

// IM1 interrupt
template <class T> inline void CPUCore<T>::irq1()
{
	R.setHALT(false);
	R.di();
	T::IM1_DELAY();
	executeInstruction1(0xFF);	// RST 0x38
}

// IM2 interrupt
template <class T> inline void CPUCore<T>::irq2()
{
	R.setHALT(false);
	R.di();
	PUSH(R.getPC());
	unsigned x = interface->readIRQVector() | (R.getI() << 8);
	R.setPC(RD_WORD(x));
	T::IM2_DELAY();
	M1Cycle();
}

template <class T> inline void CPUCore<T>::executeInstruction1(byte opcode)
{
	M1Cycle();
	(*opcode_main[opcode])(*this);
}

template <class T> inline void CPUCore<T>::executeInstruction()
{
	byte opcode = RDMEM_OPCODE();
	executeInstruction1(opcode);
}

template <class T> inline void CPUCore<T>::cpuTracePre()
{
	start_pc = R.getPC();
}
template <class T> inline void CPUCore<T>::cpuTracePost()
{
	if (unlikely(traceSetting.getValue())) {
		cpuTracePost_slow();
	}
}
template <class T> void CPUCore<T>::cpuTracePost_slow()
{
	byte opbuf[4];
	string dasmOutput;
	dasm(*interface, start_pc, opbuf, dasmOutput, T::getTimeFast());
	std::cout << std::setfill('0') << std::hex << std::setw(4) << start_pc
	     << " : " << dasmOutput
	     << " AF=" << std::setw(4) << R.getAF()
	     << " BC=" << std::setw(4) << R.getBC()
	     << " DE=" << std::setw(4) << R.getDE()
	     << " HL=" << std::setw(4) << R.getHL()
	     << " IX=" << std::setw(4) << R.getIX()
	     << " IY=" << std::setw(4) << R.getIY()
	     << " SP=" << std::setw(4) << R.getSP()
	     << std::endl << std::dec;
}

template <class T> inline void CPUCore<T>::executeFast()
{
	T::R800Refresh();
	executeInstruction();
}

template <class T> void CPUCore<T>::executeSlow()
{
	if (unlikely(false && nmiEdge)) {
		// Note: NMIs are disabled, see also raiseNMI()
		nmiEdge = false;
		nmi(); // NMI occured
	} else if (unlikely(R.getIFF1() && IRQStatus)) {
		// normal interrupt
		switch (R.getIM()) {
			case 0: irq0();
				break;
			case 1: irq1();
				break;
			case 2: irq2();
				break;
			default:
				assert(false);
		}
	} else if (unlikely(R.getHALT() || paused)) {
		// in halt mode
		R.incR(T::advanceHalt(T::haltStates(), scheduler.getNext()));
		setSlowInstructions();
	} else {
		R.setIFF1(R.getNextIFF1());
		cpuTracePre();
		executeFast();
		cpuTracePost();
	}
}

template <class T> void CPUCore<T>::execute()
{
	executeInternal();
}

template <class T> void CPUCore<T>::executeInternal()
{
	assert(!breaked);

	// note: Don't use getTimeFast() here, because 'once in a while' we
	//       need to CPUClock::sync() to avoid overflow.
	//       Should be done at least once per second (approx). So only
	//       once in this method is enough.
	scheduler.schedule(T::getTime());
	setSlowInstructions();

	if (continued || step) {
		// at least one instruction
		continued = false;
		executeSlow();
		scheduler.schedule(T::getTimeFast());
		--slowInstructions;
		if (step) {
			step = false;
			doBreak();
			return;
		}
	}

	// Note: we call scheduler _after_ executing the instruction and before
	// deciding between executeFast() and executeSlow() (because a
	// SyncPoint could set an IRQ and then we must choose executeSlow())
	if (!anyBreakPoints() && !traceSetting.getValue()) {
		// fast path, no breakpoints, no tracing
		while (!needExitCPULoop()) {
			if (slowInstructions) {
				--slowInstructions;
				executeSlow();
				scheduler.schedule(T::getTimeFast());
			} else {
				while (slowInstructions == 0) {
					T::enableLimit(true);
					while (likely(!T::limitReached())) {
						// too much overhead for non-debug build
						#ifdef DEBUG
						assert(T::getTimeFast() < scheduler.getNext());
						assert(slowInstructions == 0);
						#endif
						executeFast();
					}
					scheduler.schedule(T::getTimeFast());
					if (needExitCPULoop()) return;
				}
			}
		}
	} else {
		while (!needExitCPULoop()) {
			if (checkBreakPoints(R)) {
				continued = true; // skip bp check on next instr
				break;
			}
			if (slowInstructions == 0) {
				cpuTracePre();
				executeFast();
				cpuTracePost();
				scheduler.schedule(T::getTimeFast());
			} else {
				--slowInstructions;
				executeSlow();
				scheduler.schedule(T::getTimeFast());
			}
		}
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
template <class T> void CPUCore<T>::ld_a_b(S& s)     { s.R.setA(s.R.getB()); }
template <class T> void CPUCore<T>::ld_a_c(S& s)     { s.R.setA(s.R.getC()); }
template <class T> void CPUCore<T>::ld_a_d(S& s)     { s.R.setA(s.R.getD()); }
template <class T> void CPUCore<T>::ld_a_e(S& s)     { s.R.setA(s.R.getE()); }
template <class T> void CPUCore<T>::ld_a_h(S& s)     { s.R.setA(s.R.getH()); }
template <class T> void CPUCore<T>::ld_a_l(S& s)     { s.R.setA(s.R.getL()); }
template <class T> void CPUCore<T>::ld_a_ixh(S& s)   { s.R.setA(s.R.getIXh()); }
template <class T> void CPUCore<T>::ld_a_ixl(S& s)   { s.R.setA(s.R.getIXl()); }
template <class T> void CPUCore<T>::ld_a_iyh(S& s)   { s.R.setA(s.R.getIYh()); }
template <class T> void CPUCore<T>::ld_a_iyl(S& s)   { s.R.setA(s.R.getIYl()); }
template <class T> void CPUCore<T>::ld_b_a(S& s)     { s.R.setB(s.R.getA()); }
template <class T> void CPUCore<T>::ld_b_c(S& s)     { s.R.setB(s.R.getC()); }
template <class T> void CPUCore<T>::ld_b_d(S& s)     { s.R.setB(s.R.getD()); }
template <class T> void CPUCore<T>::ld_b_e(S& s)     { s.R.setB(s.R.getE()); }
template <class T> void CPUCore<T>::ld_b_h(S& s)     { s.R.setB(s.R.getH()); }
template <class T> void CPUCore<T>::ld_b_l(S& s)     { s.R.setB(s.R.getL()); }
template <class T> void CPUCore<T>::ld_b_ixh(S& s)   { s.R.setB(s.R.getIXh()); }
template <class T> void CPUCore<T>::ld_b_ixl(S& s)   { s.R.setB(s.R.getIXl()); }
template <class T> void CPUCore<T>::ld_b_iyh(S& s)   { s.R.setB(s.R.getIYh()); }
template <class T> void CPUCore<T>::ld_b_iyl(S& s)   { s.R.setB(s.R.getIYl()); }
template <class T> void CPUCore<T>::ld_c_a(S& s)     { s.R.setC(s.R.getA()); }
template <class T> void CPUCore<T>::ld_c_b(S& s)     { s.R.setC(s.R.getB()); }
template <class T> void CPUCore<T>::ld_c_d(S& s)     { s.R.setC(s.R.getD()); }
template <class T> void CPUCore<T>::ld_c_e(S& s)     { s.R.setC(s.R.getE()); }
template <class T> void CPUCore<T>::ld_c_h(S& s)     { s.R.setC(s.R.getH()); }
template <class T> void CPUCore<T>::ld_c_l(S& s)     { s.R.setC(s.R.getL()); }
template <class T> void CPUCore<T>::ld_c_ixh(S& s)   { s.R.setC(s.R.getIXh()); }
template <class T> void CPUCore<T>::ld_c_ixl(S& s)   { s.R.setC(s.R.getIXl()); }
template <class T> void CPUCore<T>::ld_c_iyh(S& s)   { s.R.setC(s.R.getIYh()); }
template <class T> void CPUCore<T>::ld_c_iyl(S& s)   { s.R.setC(s.R.getIYl()); }
template <class T> void CPUCore<T>::ld_d_a(S& s)     { s.R.setD(s.R.getA()); }
template <class T> void CPUCore<T>::ld_d_c(S& s)     { s.R.setD(s.R.getC()); }
template <class T> void CPUCore<T>::ld_d_b(S& s)     { s.R.setD(s.R.getB()); }
template <class T> void CPUCore<T>::ld_d_e(S& s)     { s.R.setD(s.R.getE()); }
template <class T> void CPUCore<T>::ld_d_h(S& s)     { s.R.setD(s.R.getH()); }
template <class T> void CPUCore<T>::ld_d_l(S& s)     { s.R.setD(s.R.getL()); }
template <class T> void CPUCore<T>::ld_d_ixh(S& s)   { s.R.setD(s.R.getIXh()); }
template <class T> void CPUCore<T>::ld_d_ixl(S& s)   { s.R.setD(s.R.getIXl()); }
template <class T> void CPUCore<T>::ld_d_iyh(S& s)   { s.R.setD(s.R.getIYh()); }
template <class T> void CPUCore<T>::ld_d_iyl(S& s)   { s.R.setD(s.R.getIYl()); }
template <class T> void CPUCore<T>::ld_e_a(S& s)     { s.R.setE(s.R.getA()); }
template <class T> void CPUCore<T>::ld_e_c(S& s)     { s.R.setE(s.R.getC()); }
template <class T> void CPUCore<T>::ld_e_b(S& s)     { s.R.setE(s.R.getB()); }
template <class T> void CPUCore<T>::ld_e_d(S& s)     { s.R.setE(s.R.getD()); }
template <class T> void CPUCore<T>::ld_e_h(S& s)     { s.R.setE(s.R.getH()); }
template <class T> void CPUCore<T>::ld_e_l(S& s)     { s.R.setE(s.R.getL()); }
template <class T> void CPUCore<T>::ld_e_ixh(S& s)   { s.R.setE(s.R.getIXh()); }
template <class T> void CPUCore<T>::ld_e_ixl(S& s)   { s.R.setE(s.R.getIXl()); }
template <class T> void CPUCore<T>::ld_e_iyh(S& s)   { s.R.setE(s.R.getIYh()); }
template <class T> void CPUCore<T>::ld_e_iyl(S& s)   { s.R.setE(s.R.getIYl()); }
template <class T> void CPUCore<T>::ld_h_a(S& s)     { s.R.setH(s.R.getA()); }
template <class T> void CPUCore<T>::ld_h_c(S& s)     { s.R.setH(s.R.getC()); }
template <class T> void CPUCore<T>::ld_h_b(S& s)     { s.R.setH(s.R.getB()); }
template <class T> void CPUCore<T>::ld_h_e(S& s)     { s.R.setH(s.R.getE()); }
template <class T> void CPUCore<T>::ld_h_d(S& s)     { s.R.setH(s.R.getD()); }
template <class T> void CPUCore<T>::ld_h_l(S& s)     { s.R.setH(s.R.getL()); }
template <class T> void CPUCore<T>::ld_l_a(S& s)     { s.R.setL(s.R.getA()); }
template <class T> void CPUCore<T>::ld_l_c(S& s)     { s.R.setL(s.R.getC()); }
template <class T> void CPUCore<T>::ld_l_b(S& s)     { s.R.setL(s.R.getB()); }
template <class T> void CPUCore<T>::ld_l_e(S& s)     { s.R.setL(s.R.getE()); }
template <class T> void CPUCore<T>::ld_l_d(S& s)     { s.R.setL(s.R.getD()); }
template <class T> void CPUCore<T>::ld_l_h(S& s)     { s.R.setL(s.R.getH()); }
template <class T> void CPUCore<T>::ld_ixh_a(S& s)   { s.R.setIXh(s.R.getA()); }
template <class T> void CPUCore<T>::ld_ixh_b(S& s)   { s.R.setIXh(s.R.getB()); }
template <class T> void CPUCore<T>::ld_ixh_c(S& s)   { s.R.setIXh(s.R.getC()); }
template <class T> void CPUCore<T>::ld_ixh_d(S& s)   { s.R.setIXh(s.R.getD()); }
template <class T> void CPUCore<T>::ld_ixh_e(S& s)   { s.R.setIXh(s.R.getE()); }
template <class T> void CPUCore<T>::ld_ixh_ixl(S& s) { s.R.setIXh(s.R.getIXl()); }
template <class T> void CPUCore<T>::ld_ixl_a(S& s)   { s.R.setIXl(s.R.getA()); }
template <class T> void CPUCore<T>::ld_ixl_b(S& s)   { s.R.setIXl(s.R.getB()); }
template <class T> void CPUCore<T>::ld_ixl_c(S& s)   { s.R.setIXl(s.R.getC()); }
template <class T> void CPUCore<T>::ld_ixl_d(S& s)   { s.R.setIXl(s.R.getD()); }
template <class T> void CPUCore<T>::ld_ixl_e(S& s)   { s.R.setIXl(s.R.getE()); }
template <class T> void CPUCore<T>::ld_ixl_ixh(S& s) { s.R.setIXl(s.R.getIXh()); }
template <class T> void CPUCore<T>::ld_iyh_a(S& s)   { s.R.setIYh(s.R.getA()); }
template <class T> void CPUCore<T>::ld_iyh_b(S& s)   { s.R.setIYh(s.R.getB()); }
template <class T> void CPUCore<T>::ld_iyh_c(S& s)   { s.R.setIYh(s.R.getC()); }
template <class T> void CPUCore<T>::ld_iyh_d(S& s)   { s.R.setIYh(s.R.getD()); }
template <class T> void CPUCore<T>::ld_iyh_e(S& s)   { s.R.setIYh(s.R.getE()); }
template <class T> void CPUCore<T>::ld_iyh_iyl(S& s) { s.R.setIYh(s.R.getIYl()); }
template <class T> void CPUCore<T>::ld_iyl_a(S& s)   { s.R.setIYl(s.R.getA()); }
template <class T> void CPUCore<T>::ld_iyl_b(S& s)   { s.R.setIYl(s.R.getB()); }
template <class T> void CPUCore<T>::ld_iyl_c(S& s)   { s.R.setIYl(s.R.getC()); }
template <class T> void CPUCore<T>::ld_iyl_d(S& s)   { s.R.setIYl(s.R.getD()); }
template <class T> void CPUCore<T>::ld_iyl_e(S& s)   { s.R.setIYl(s.R.getE()); }
template <class T> void CPUCore<T>::ld_iyl_iyh(S& s) { s.R.setIYl(s.R.getIYh()); }

// LD SP,ss
template <class T> void CPUCore<T>::ld_sp_hl(S& s) {
	s.LD_SP_HL_DELAY(); s.R.setSP(s.R.getHL());
}
template <class T> void CPUCore<T>::ld_sp_ix(S& s) {
	s.LD_SP_HL_DELAY(); s.R.setSP(s.R.getIX());
}
template <class T> void CPUCore<T>::ld_sp_iy(S& s) {
	s.LD_SP_HL_DELAY(); s.R.setSP(s.R.getIY());
}

// LD (ss),a
template <class T> inline void CPUCore<T>::WR_X_A(unsigned x)
{
	WRMEM(x, R.getA());
}
template <class T> void CPUCore<T>::ld_xbc_a(S& s) { s.WR_X_A(s.R.getBC()); }
template <class T> void CPUCore<T>::ld_xde_a(S& s) { s.WR_X_A(s.R.getDE()); }
template <class T> void CPUCore<T>::ld_xhl_a(S& s) { s.WR_X_A(s.R.getHL()); }

// LD (HL),r
template <class T> inline void CPUCore<T>::WR_HL_X(byte val)
{
	WRMEM(R.getHL(), val);
}
template <class T> void CPUCore<T>::ld_xhl_b(S& s) { s.WR_HL_X(s.R.getB()); }
template <class T> void CPUCore<T>::ld_xhl_c(S& s) { s.WR_HL_X(s.R.getC()); }
template <class T> void CPUCore<T>::ld_xhl_d(S& s) { s.WR_HL_X(s.R.getD()); }
template <class T> void CPUCore<T>::ld_xhl_e(S& s) { s.WR_HL_X(s.R.getE()); }
template <class T> void CPUCore<T>::ld_xhl_h(S& s) { s.WR_HL_X(s.R.getH()); }
template <class T> void CPUCore<T>::ld_xhl_l(S& s) { s.WR_HL_X(s.R.getL()); }

// LD (HL),n
template <class T> void CPUCore<T>::ld_xhl_byte(S& s)
{
	byte val = s.RDMEM_OPCODE();
	s.WR_HL_X(val);
}

// LD (IX+e),r
template <class T> inline void CPUCore<T>::WR_X_X(byte val)
{
	WRMEM(memptr, val);
}
template <class T> inline void CPUCore<T>::WR_XIX(byte val)
{
	offset ofst = RDMEM_OPCODE();
	memptr = (R.getIX() + ofst) & 0xFFFF;
	T::ADD_16_8_DELAY();
	WR_X_X(val);
}
template <class T> void CPUCore<T>::ld_xix_a(S& s) { s.WR_XIX(s.R.getA()); }
template <class T> void CPUCore<T>::ld_xix_b(S& s) { s.WR_XIX(s.R.getB()); }
template <class T> void CPUCore<T>::ld_xix_c(S& s) { s.WR_XIX(s.R.getC()); }
template <class T> void CPUCore<T>::ld_xix_d(S& s) { s.WR_XIX(s.R.getD()); }
template <class T> void CPUCore<T>::ld_xix_e(S& s) { s.WR_XIX(s.R.getE()); }
template <class T> void CPUCore<T>::ld_xix_h(S& s) { s.WR_XIX(s.R.getH()); }
template <class T> void CPUCore<T>::ld_xix_l(S& s) { s.WR_XIX(s.R.getL()); }

// LD (IY+e),r
template <class T> inline void CPUCore<T>::WR_XIY(byte val)
{
	offset ofst = RDMEM_OPCODE();
	memptr = (R.getIY() + ofst) & 0xFFFF;
	T::ADD_16_8_DELAY();
	WR_X_X(val);
}
template <class T> void CPUCore<T>::ld_xiy_a(S& s) { s.WR_XIY(s.R.getA()); }
template <class T> void CPUCore<T>::ld_xiy_b(S& s) { s.WR_XIY(s.R.getB()); }
template <class T> void CPUCore<T>::ld_xiy_c(S& s) { s.WR_XIY(s.R.getC()); }
template <class T> void CPUCore<T>::ld_xiy_d(S& s) { s.WR_XIY(s.R.getD()); }
template <class T> void CPUCore<T>::ld_xiy_e(S& s) { s.WR_XIY(s.R.getE()); }
template <class T> void CPUCore<T>::ld_xiy_h(S& s) { s.WR_XIY(s.R.getH()); }
template <class T> void CPUCore<T>::ld_xiy_l(S& s) { s.WR_XIY(s.R.getL()); }

// LD (IX+e),n
template <class T> void CPUCore<T>::ld_xix_byte(S& s)
{
	unsigned tmp = s.RD_WORD_PC();
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.PARALLEL_DELAY(); s.WR_X_X(val);
}

// LD (IY+e),n
template <class T> void CPUCore<T>::ld_xiy_byte(S& s)
{
	unsigned tmp = s.RD_WORD_PC();
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.PARALLEL_DELAY(); s.WR_X_X(val);
}

// LD (nn),A
template <class T> void CPUCore<T>::ld_xbyte_a(S& s)
{
	unsigned x = s.RD_WORD_PC();
	s.memptr = s.R.getA() << 8;
	s.WRMEM(x, s.R.getA());
}

// LD (nn),ss
template <class T> inline void CPUCore<T>::WR_NN_Y(unsigned reg)
{
	memptr = RD_WORD_PC();
	WR_WORD(memptr, reg);
}
template <class T> void CPUCore<T>::ld_xword_bc(S& s) { s.WR_NN_Y(s.R.getBC()); }
template <class T> void CPUCore<T>::ld_xword_de(S& s) { s.WR_NN_Y(s.R.getDE()); }
template <class T> void CPUCore<T>::ld_xword_hl(S& s) { s.WR_NN_Y(s.R.getHL()); }
template <class T> void CPUCore<T>::ld_xword_ix(S& s) { s.WR_NN_Y(s.R.getIX()); }
template <class T> void CPUCore<T>::ld_xword_iy(S& s) { s.WR_NN_Y(s.R.getIY()); }
template <class T> void CPUCore<T>::ld_xword_sp(S& s) { s.WR_NN_Y(s.R.getSP()); }

// LD A,(ss)
template <class T> inline void CPUCore<T>::RD_A_X(unsigned x)
{
	R.setA(RDMEM(x));
}
template <class T> void CPUCore<T>::ld_a_xbc(S& s) { s.RD_A_X(s.R.getBC()); }
template <class T> void CPUCore<T>::ld_a_xde(S& s) { s.RD_A_X(s.R.getDE()); }
template <class T> void CPUCore<T>::ld_a_xhl(S& s) { s.RD_A_X(s.R.getHL()); }

// LD A,(IX+e)
template <class T> void CPUCore<T>::ld_a_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.RD_A_X(s.memptr);
}

// LD A,(IY+e)
template <class T> void CPUCore<T>::ld_a_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.RD_A_X(s.memptr);
}

// LD A,(nn)
template <class T> void CPUCore<T>::ld_a_xbyte(S& s)
{
	unsigned addr = s.RD_WORD_PC();
	s.memptr = addr + 1;
	s.RD_A_X(addr);
}

// LD r,n
template <class T> void CPUCore<T>::ld_a_byte(S& s)   { s.R.setA  (s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_b_byte(S& s)   { s.R.setB  (s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_c_byte(S& s)   { s.R.setC  (s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_d_byte(S& s)   { s.R.setD  (s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_e_byte(S& s)   { s.R.setE  (s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_h_byte(S& s)   { s.R.setH  (s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_l_byte(S& s)   { s.R.setL  (s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_ixh_byte(S& s) { s.R.setIXh(s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_ixl_byte(S& s) { s.R.setIXl(s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_iyh_byte(S& s) { s.R.setIYh(s.RDMEM_OPCODE()); }
template <class T> void CPUCore<T>::ld_iyl_byte(S& s) { s.R.setIYl(s.RDMEM_OPCODE()); }

// LD r,(hl)
template <class T> void CPUCore<T>::ld_b_xhl(S& s) { s.R.setB(s.RDMEM(s.R.getHL())); }
template <class T> void CPUCore<T>::ld_c_xhl(S& s) { s.R.setC(s.RDMEM(s.R.getHL())); }
template <class T> void CPUCore<T>::ld_d_xhl(S& s) { s.R.setD(s.RDMEM(s.R.getHL())); }
template <class T> void CPUCore<T>::ld_e_xhl(S& s) { s.R.setE(s.RDMEM(s.R.getHL())); }
template <class T> void CPUCore<T>::ld_h_xhl(S& s) { s.R.setH(s.RDMEM(s.R.getHL())); }
template <class T> void CPUCore<T>::ld_l_xhl(S& s) { s.R.setL(s.RDMEM(s.R.getHL())); }

// LD r,(IX+e)
template <class T> inline byte CPUCore<T>::RD_R_XIX()
{
	offset ofst = RDMEM_OPCODE();
	memptr = (R.getIX() + ofst) & 0xFFFF;
	T::ADD_16_8_DELAY();
	return RDMEM(memptr);
}
template <class T> void CPUCore<T>::ld_b_xix(S& s) { s.R.setB(s.RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_c_xix(S& s) { s.R.setC(s.RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_d_xix(S& s) { s.R.setD(s.RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_e_xix(S& s) { s.R.setE(s.RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_h_xix(S& s) { s.R.setH(s.RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_l_xix(S& s) { s.R.setL(s.RD_R_XIX()); }

// LD r,(IY+e)
template <class T> inline byte CPUCore<T>::RD_R_XIY()
{
	offset ofst = RDMEM_OPCODE();
	memptr = (R.getIY() + ofst) & 0xFFFF;
	T::ADD_16_8_DELAY();
	return RDMEM(memptr);
}
template <class T> void CPUCore<T>::ld_b_xiy(S& s) { s.R.setB(s.RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_c_xiy(S& s) { s.R.setC(s.RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_d_xiy(S& s) { s.R.setD(s.RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_e_xiy(S& s) { s.R.setE(s.RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_h_xiy(S& s) { s.R.setH(s.RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_l_xiy(S& s) { s.R.setL(s.RD_R_XIY()); }

// LD ss,(nn)
template <class T> inline unsigned CPUCore<T>::RD_P_XX()
{
	unsigned addr = RD_WORD_PC();
	memptr = addr + 1; // not 16-bit
	return RD_WORD(addr);
}
template <class T> void CPUCore<T>::ld_bc_xword(S& s) { s.R.setBC(s.RD_P_XX()); }
template <class T> void CPUCore<T>::ld_de_xword(S& s) { s.R.setDE(s.RD_P_XX()); }
template <class T> void CPUCore<T>::ld_hl_xword(S& s) { s.R.setHL(s.RD_P_XX()); }
template <class T> void CPUCore<T>::ld_ix_xword(S& s) { s.R.setIX(s.RD_P_XX()); }
template <class T> void CPUCore<T>::ld_iy_xword(S& s) { s.R.setIY(s.RD_P_XX()); }
template <class T> void CPUCore<T>::ld_sp_xword(S& s) { s.R.setSP(s.RD_P_XX()); }

// LD ss,nn
template <class T> void CPUCore<T>::ld_bc_word(S& s) { s.R.setBC(s.RD_WORD_PC()); }
template <class T> void CPUCore<T>::ld_de_word(S& s) { s.R.setDE(s.RD_WORD_PC()); }
template <class T> void CPUCore<T>::ld_hl_word(S& s) { s.R.setHL(s.RD_WORD_PC()); }
template <class T> void CPUCore<T>::ld_ix_word(S& s) { s.R.setIX(s.RD_WORD_PC()); }
template <class T> void CPUCore<T>::ld_iy_word(S& s) { s.R.setIY(s.RD_WORD_PC()); }
template <class T> void CPUCore<T>::ld_sp_word(S& s) { s.R.setSP(s.RD_WORD_PC()); }


// ADC A,r
template <class T> inline void CPUCore<T>::ADC(byte reg)
{
	unsigned res = R.getA() + reg + ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((R.getA() ^ res) & (reg ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> inline void CPUCore<T>::adc_a_a(S& s)
{
	unsigned res = 2 * s.R.getA() + ((s.R.getF() & C_FLAG) ? 1 : 0);
	s.R.setF(ZSXYTable[res & 0xFF] |
	         ((res & 0x100) ? C_FLAG : 0) |
	         (res & H_FLAG) |
	         (((s.R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	s.R.setA(res);
}
template <class T> void CPUCore<T>::adc_a_b(S& s)   { s.ADC(s.R.getB()); }
template <class T> void CPUCore<T>::adc_a_c(S& s)   { s.ADC(s.R.getC()); }
template <class T> void CPUCore<T>::adc_a_d(S& s)   { s.ADC(s.R.getD()); }
template <class T> void CPUCore<T>::adc_a_e(S& s)   { s.ADC(s.R.getE()); }
template <class T> void CPUCore<T>::adc_a_h(S& s)   { s.ADC(s.R.getH()); }
template <class T> void CPUCore<T>::adc_a_l(S& s)   { s.ADC(s.R.getL()); }
template <class T> void CPUCore<T>::adc_a_ixl(S& s) { s.ADC(s.R.getIXl()); }
template <class T> void CPUCore<T>::adc_a_ixh(S& s) { s.ADC(s.R.getIXh()); }
template <class T> void CPUCore<T>::adc_a_iyl(S& s) { s.ADC(s.R.getIYl()); }
template <class T> void CPUCore<T>::adc_a_iyh(S& s) { s.ADC(s.R.getIYh()); }
template <class T> void CPUCore<T>::adc_a_byte(S& s){ s.ADC(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::adc_a_x(unsigned x) { ADC(RDMEM(x)); }
template <class T> void CPUCore<T>::adc_a_xhl(S& s) { s.adc_a_x(s.R.getHL()); }
template <class T> void CPUCore<T>::adc_a_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.adc_a_x(s.memptr);
}
template <class T> void CPUCore<T>::adc_a_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.adc_a_x(s.memptr);
}

// ADD A,r
template <class T> inline void CPUCore<T>::ADD(byte reg)
{
	unsigned res = R.getA() + reg;
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((R.getA() ^ res) & (reg ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> inline void CPUCore<T>::add_a_a(S& s)
{
	unsigned res = 2 * s.R.getA();
	s.R.setF(ZSXYTable[res & 0xFF] |
	         ((res & 0x100) ? C_FLAG : 0) |
	         (res & H_FLAG) |
	         (((s.R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	s.R.setA(res);
}
template <class T> void CPUCore<T>::add_a_b(S& s)   { s.ADD(s.R.getB()); }
template <class T> void CPUCore<T>::add_a_c(S& s)   { s.ADD(s.R.getC()); }
template <class T> void CPUCore<T>::add_a_d(S& s)   { s.ADD(s.R.getD()); }
template <class T> void CPUCore<T>::add_a_e(S& s)   { s.ADD(s.R.getE()); }
template <class T> void CPUCore<T>::add_a_h(S& s)   { s.ADD(s.R.getH()); }
template <class T> void CPUCore<T>::add_a_l(S& s)   { s.ADD(s.R.getL()); }
template <class T> void CPUCore<T>::add_a_ixl(S& s) { s.ADD(s.R.getIXl()); }
template <class T> void CPUCore<T>::add_a_ixh(S& s) { s.ADD(s.R.getIXh()); }
template <class T> void CPUCore<T>::add_a_iyl(S& s) { s.ADD(s.R.getIYl()); }
template <class T> void CPUCore<T>::add_a_iyh(S& s) { s.ADD(s.R.getIYh()); }
template <class T> void CPUCore<T>::add_a_byte(S& s){ s.ADD(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::add_a_x(unsigned x) { ADD(RDMEM(x)); }
template <class T> void CPUCore<T>::add_a_xhl(S& s) { s.add_a_x(s.R.getHL()); }
template <class T> void CPUCore<T>::add_a_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.add_a_x(s.memptr);
}
template <class T> void CPUCore<T>::add_a_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.add_a_x(s.memptr);
}

// AND r
template <class T> inline void CPUCore<T>::AND(byte reg)
{
	R.setA(R.getA() & reg);
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
}
template <class T> void CPUCore<T>::and_a(S& s)
{
	s.R.setF(ZSPXYTable[s.R.getA()] | H_FLAG);
}
template <class T> void CPUCore<T>::and_b(S& s)   { s.AND(s.R.getB()); }
template <class T> void CPUCore<T>::and_c(S& s)   { s.AND(s.R.getC()); }
template <class T> void CPUCore<T>::and_d(S& s)   { s.AND(s.R.getD()); }
template <class T> void CPUCore<T>::and_e(S& s)   { s.AND(s.R.getE()); }
template <class T> void CPUCore<T>::and_h(S& s)   { s.AND(s.R.getH()); }
template <class T> void CPUCore<T>::and_l(S& s)   { s.AND(s.R.getL()); }
template <class T> void CPUCore<T>::and_ixh(S& s) { s.AND(s.R.getIXh()); }
template <class T> void CPUCore<T>::and_ixl(S& s) { s.AND(s.R.getIXl()); }
template <class T> void CPUCore<T>::and_iyh(S& s) { s.AND(s.R.getIYh()); }
template <class T> void CPUCore<T>::and_iyl(S& s) { s.AND(s.R.getIYl()); }
template <class T> void CPUCore<T>::and_byte(S& s){ s.AND(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::and_x(unsigned x) { AND(RDMEM(x)); }
template <class T> void CPUCore<T>::and_xhl(S& s) { s.and_x(s.R.getHL()); }
template <class T> void CPUCore<T>::and_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.and_x(s.memptr);
}
template <class T> void CPUCore<T>::and_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.and_x(s.memptr);
}

// CP r
template <class T> inline void CPUCore<T>::CP(byte reg)
{
	unsigned q = R.getA() - reg;
	R.setF(ZSTable[q & 0xFF] |
	       (reg & (X_FLAG | Y_FLAG)) |	// XY from operand, not from result
	       ((q & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ q ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ q) & 0x80) >> 5)); // V_FLAG
}
template <class T> void CPUCore<T>::cp_a(S& s)
{
	s.R.setF(ZS0 | N_FLAG |
	         (s.R.getA() & (X_FLAG | Y_FLAG))); // XY from operand, not from result
}
template <class T> void CPUCore<T>::cp_b(S& s)   { s.CP(s.R.getB()); }
template <class T> void CPUCore<T>::cp_c(S& s)   { s.CP(s.R.getC()); }
template <class T> void CPUCore<T>::cp_d(S& s)   { s.CP(s.R.getD()); }
template <class T> void CPUCore<T>::cp_e(S& s)   { s.CP(s.R.getE()); }
template <class T> void CPUCore<T>::cp_h(S& s)   { s.CP(s.R.getH()); }
template <class T> void CPUCore<T>::cp_l(S& s)   { s.CP(s.R.getL()); }
template <class T> void CPUCore<T>::cp_ixh(S& s) { s.CP(s.R.getIXh()); }
template <class T> void CPUCore<T>::cp_ixl(S& s) { s.CP(s.R.getIXl()); }
template <class T> void CPUCore<T>::cp_iyh(S& s) { s.CP(s.R.getIYh()); }
template <class T> void CPUCore<T>::cp_iyl(S& s) { s.CP(s.R.getIYl()); }
template <class T> void CPUCore<T>::cp_byte(S& s){ s.CP(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::cp_x(unsigned x) { CP(RDMEM(x)); }
template <class T> void CPUCore<T>::cp_xhl(S& s) { s.cp_x(s.R.getHL()); }
template <class T> void CPUCore<T>::cp_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.cp_x(s.memptr);
}
template <class T> void CPUCore<T>::cp_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.cp_x(s.memptr);
}

// OR r
template <class T> inline void CPUCore<T>::OR(byte reg)
{
	R.setA(R.getA() | reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> void CPUCore<T>::or_a(S& s)   { s.R.setF(ZSPXYTable[s.R.getA()]); }
template <class T> void CPUCore<T>::or_b(S& s)   { s.OR(s.R.getB()); }
template <class T> void CPUCore<T>::or_c(S& s)   { s.OR(s.R.getC()); }
template <class T> void CPUCore<T>::or_d(S& s)   { s.OR(s.R.getD()); }
template <class T> void CPUCore<T>::or_e(S& s)   { s.OR(s.R.getE()); }
template <class T> void CPUCore<T>::or_h(S& s)   { s.OR(s.R.getH()); }
template <class T> void CPUCore<T>::or_l(S& s)   { s.OR(s.R.getL()); }
template <class T> void CPUCore<T>::or_ixh(S& s) { s.OR(s.R.getIXh()); }
template <class T> void CPUCore<T>::or_ixl(S& s) { s.OR(s.R.getIXl()); }
template <class T> void CPUCore<T>::or_iyh(S& s) { s.OR(s.R.getIYh()); }
template <class T> void CPUCore<T>::or_iyl(S& s) { s.OR(s.R.getIYl()); }
template <class T> void CPUCore<T>::or_byte(S& s){ s.OR(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::or_x(unsigned x) { OR(RDMEM(x)); }
template <class T> void CPUCore<T>::or_xhl(S& s) { s.or_x(s.R.getHL()); }
template <class T> void CPUCore<T>::or_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.or_x(s.memptr);
}
template <class T> void CPUCore<T>::or_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.or_x(s.memptr);
}

// SBC A,r
template <class T> inline void CPUCore<T>::SBC(byte reg)
{
	unsigned res = R.getA() - reg - ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> void CPUCore<T>::sbc_a_a(S& s)
{
	s.R.setAF((s.R.getF() & C_FLAG) ?
	          (255 * 256 | ZSXY255 | C_FLAG | H_FLAG | N_FLAG) :
	          (  0 * 256 | ZSXY0   |                   N_FLAG));
}
template <class T> void CPUCore<T>::sbc_a_b(S& s)   { s.SBC(s.R.getB()); }
template <class T> void CPUCore<T>::sbc_a_c(S& s)   { s.SBC(s.R.getC()); }
template <class T> void CPUCore<T>::sbc_a_d(S& s)   { s.SBC(s.R.getD()); }
template <class T> void CPUCore<T>::sbc_a_e(S& s)   { s.SBC(s.R.getE()); }
template <class T> void CPUCore<T>::sbc_a_h(S& s)   { s.SBC(s.R.getH()); }
template <class T> void CPUCore<T>::sbc_a_l(S& s)   { s.SBC(s.R.getL()); }
template <class T> void CPUCore<T>::sbc_a_ixh(S& s) { s.SBC(s.R.getIXh()); }
template <class T> void CPUCore<T>::sbc_a_ixl(S& s) { s.SBC(s.R.getIXl()); }
template <class T> void CPUCore<T>::sbc_a_iyh(S& s) { s.SBC(s.R.getIYh()); }
template <class T> void CPUCore<T>::sbc_a_iyl(S& s) { s.SBC(s.R.getIYl()); }
template <class T> void CPUCore<T>::sbc_a_byte(S& s){ s.SBC(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::sbc_a_x(unsigned x) { SBC(RDMEM(x)); }
template <class T> void CPUCore<T>::sbc_a_xhl(S& s) { s.sbc_a_x(s.R.getHL()); }
template <class T> void CPUCore<T>::sbc_a_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.sbc_a_x(s.memptr);
}
template <class T> void CPUCore<T>::sbc_a_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.sbc_a_x(s.memptr);
}

// SUB r
template <class T> inline void CPUCore<T>::SUB(byte reg)
{
	unsigned res = R.getA() - reg;
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> void CPUCore<T>::sub_a(S& s)
{
	s.R.setAF(0 * 256 | ZSXY0 | N_FLAG);
}
template <class T> void CPUCore<T>::sub_b(S& s)   { s.SUB(s.R.getB()); }
template <class T> void CPUCore<T>::sub_c(S& s)   { s.SUB(s.R.getC()); }
template <class T> void CPUCore<T>::sub_d(S& s)   { s.SUB(s.R.getD()); }
template <class T> void CPUCore<T>::sub_e(S& s)   { s.SUB(s.R.getE()); }
template <class T> void CPUCore<T>::sub_h(S& s)   { s.SUB(s.R.getH()); }
template <class T> void CPUCore<T>::sub_l(S& s)   { s.SUB(s.R.getL()); }
template <class T> void CPUCore<T>::sub_ixh(S& s) { s.SUB(s.R.getIXh()); }
template <class T> void CPUCore<T>::sub_ixl(S& s) { s.SUB(s.R.getIXl()); }
template <class T> void CPUCore<T>::sub_iyh(S& s) { s.SUB(s.R.getIYh()); }
template <class T> void CPUCore<T>::sub_iyl(S& s) { s.SUB(s.R.getIYl()); }
template <class T> void CPUCore<T>::sub_byte(S& s){ s.SUB(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::sub_x(unsigned x) { SUB(RDMEM(x)); }
template <class T> void CPUCore<T>::sub_xhl(S& s) { s.sub_x(s.R.getHL()); }
template <class T> void CPUCore<T>::sub_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.sub_x(s.memptr);
}
template <class T> void CPUCore<T>::sub_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.sub_x(s.memptr);
}

// XOR r
template <class T> inline void CPUCore<T>::XOR(byte reg)
{
	R.setA(R.getA() ^ reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> void CPUCore<T>::xor_a(S& s)   { s.R.setAF(0 * 256 + ZSPXY0); }
template <class T> void CPUCore<T>::xor_b(S& s)   { s.XOR(s.R.getB()); }
template <class T> void CPUCore<T>::xor_c(S& s)   { s.XOR(s.R.getC()); }
template <class T> void CPUCore<T>::xor_d(S& s)   { s.XOR(s.R.getD()); }
template <class T> void CPUCore<T>::xor_e(S& s)   { s.XOR(s.R.getE()); }
template <class T> void CPUCore<T>::xor_h(S& s)   { s.XOR(s.R.getH()); }
template <class T> void CPUCore<T>::xor_l(S& s)   { s.XOR(s.R.getL()); }
template <class T> void CPUCore<T>::xor_ixh(S& s) { s.XOR(s.R.getIXh()); }
template <class T> void CPUCore<T>::xor_ixl(S& s) { s.XOR(s.R.getIXl()); }
template <class T> void CPUCore<T>::xor_iyh(S& s) { s.XOR(s.R.getIYh()); }
template <class T> void CPUCore<T>::xor_iyl(S& s) { s.XOR(s.R.getIYl()); }
template <class T> void CPUCore<T>::xor_byte(S& s){ s.XOR(s.RDMEM_OPCODE()); }
template <class T> inline void CPUCore<T>::xor_x(unsigned x) { XOR(RDMEM(x)); }
template <class T> void CPUCore<T>::xor_xhl(S& s) { s.xor_x(s.R.getHL()); }
template <class T> void CPUCore<T>::xor_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.xor_x(s.memptr);
}
template <class T> void CPUCore<T>::xor_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.xor_x(s.memptr);
}


// DEC r
template <class T> inline byte CPUCore<T>::DEC(byte reg)
{
	byte res = reg - 1;
	R.setF((R.getF() & C_FLAG) |
	       ((reg & ~res & 0x80) >> 5) | // V_FLAG
	       (((res & 0x0F) + 1) & H_FLAG) |
	       ZSXYTable[res] |
	       N_FLAG);
	return res;
}
template <class T> void CPUCore<T>::dec_a(S& s)   { s.R.setA(  s.DEC(s.R.getA())); }
template <class T> void CPUCore<T>::dec_b(S& s)   { s.R.setB(  s.DEC(s.R.getB())); }
template <class T> void CPUCore<T>::dec_c(S& s)   { s.R.setC(  s.DEC(s.R.getC())); }
template <class T> void CPUCore<T>::dec_d(S& s)   { s.R.setD(  s.DEC(s.R.getD())); }
template <class T> void CPUCore<T>::dec_e(S& s)   { s.R.setE(  s.DEC(s.R.getE())); }
template <class T> void CPUCore<T>::dec_h(S& s)   { s.R.setH(  s.DEC(s.R.getH())); }
template <class T> void CPUCore<T>::dec_l(S& s)   { s.R.setL(  s.DEC(s.R.getL())); }
template <class T> void CPUCore<T>::dec_ixh(S& s) { s.R.setIXh(s.DEC(s.R.getIXh())); }
template <class T> void CPUCore<T>::dec_ixl(S& s) { s.R.setIXl(s.DEC(s.R.getIXl())); }
template <class T> void CPUCore<T>::dec_iyh(S& s) { s.R.setIYh(s.DEC(s.R.getIYh())); }
template <class T> void CPUCore<T>::dec_iyl(S& s) { s.R.setIYl(s.DEC(s.R.getIYl())); }

template <class T> inline void CPUCore<T>::DEC_X(unsigned x)
{
	byte val = DEC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, val);
}
template <class T> void CPUCore<T>::dec_xhl(S& s) { s.DEC_X(s.R.getHL()); }
template <class T> void CPUCore<T>::dec_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.DEC_X(s.memptr);
}
template <class T> void CPUCore<T>::dec_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.DEC_X(s.memptr);
}

// INC r
template <class T> inline byte CPUCore<T>::INC(byte reg)
{
	reg++;
	R.setF((R.getF() & C_FLAG) |
	       ((reg & -reg & 0x80) >> 5) | // V_FLAG
	       (((reg & 0x0F) - 1) & H_FLAG) |
	       ZSXYTable[reg]);
	return reg;
}
template <class T> void CPUCore<T>::inc_a(S& s)   { s.R.setA(  s.INC(s.R.getA())); }
template <class T> void CPUCore<T>::inc_b(S& s)   { s.R.setB(  s.INC(s.R.getB())); }
template <class T> void CPUCore<T>::inc_c(S& s)   { s.R.setC(  s.INC(s.R.getC())); }
template <class T> void CPUCore<T>::inc_d(S& s)   { s.R.setD(  s.INC(s.R.getD())); }
template <class T> void CPUCore<T>::inc_e(S& s)   { s.R.setE(  s.INC(s.R.getE())); }
template <class T> void CPUCore<T>::inc_h(S& s)   { s.R.setH(  s.INC(s.R.getH())); }
template <class T> void CPUCore<T>::inc_l(S& s)   { s.R.setL(  s.INC(s.R.getL())); }
template <class T> void CPUCore<T>::inc_ixh(S& s) { s.R.setIXh(s.INC(s.R.getIXh())); }
template <class T> void CPUCore<T>::inc_ixl(S& s) { s.R.setIXl(s.INC(s.R.getIXl())); }
template <class T> void CPUCore<T>::inc_iyh(S& s) { s.R.setIYh(s.INC(s.R.getIYh())); }
template <class T> void CPUCore<T>::inc_iyl(S& s) { s.R.setIYl(s.INC(s.R.getIYl())); }

template <class T> inline void CPUCore<T>::INC_X(unsigned x)
{
	byte val = INC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, val);
}
template <class T> void CPUCore<T>::inc_xhl(S& s) { s.INC_X(s.R.getHL()); }
template <class T> void CPUCore<T>::inc_xix(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIX() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.INC_X(s.memptr);
}
template <class T> void CPUCore<T>::inc_xiy(S& s)
{
	offset ofst = s.RDMEM_OPCODE();
	s.memptr = (s.R.getIY() + ofst) & 0xFFFF;
	s.ADD_16_8_DELAY();
	s.INC_X(s.memptr);
}


// ADC HL,ss
template <class T> inline void CPUCore<T>::ADCW(unsigned reg)
{
	memptr = R.getHL() + 1; // not 16-bit
	unsigned res = R.getHL() + reg + ((R.getF() & C_FLAG) ? 1 : 0);
	if (res & 0xFFFF) {
		R.setF((((R.getHL() ^ res ^ reg) >> 8) & H_FLAG) |
		       (res >> 16) | // C_FLAG
		       0 | // Z_FLAG
		       (((R.getHL() ^ res) & (reg ^ res) & 0x8000) >> 13) | // V_FLAG
		       ((res >> 8) & (S_FLAG | X_FLAG | Y_FLAG)));
	} else {
		R.setF((((R.getHL() ^ reg) >> 8) & H_FLAG) |
		       (res >> 16) | // C_FLAG
		       Z_FLAG |
		       ((R.getHL() & reg & 0x8000) >> 13) | // V_FLAG
		       0); // S_FLAG X_FLAG Y_FLAG
	}
	R.setHL(res);
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::adc_hl_hl(S& s)
{
	s.memptr = s.R.getHL() + 1; // not 16-bit
	unsigned res = 2 * s.R.getHL() + ((s.R.getF() & C_FLAG) ? 1 : 0);
	if (res & 0xFFFF) {
		s.R.setF((res >> 16) | // C_FLAG
		         0 | // Z_FLAG
		         (((s.R.getHL() ^ res) & 0x8000) >> 13) | // V_FLAG
		         ((res >> 8) & (H_FLAG | S_FLAG | X_FLAG | Y_FLAG)));
	} else {
		s.R.setF((res >> 16) | // C_FLAG
		         Z_FLAG |
		         ((s.R.getHL() & 0x8000) >> 13) | // V_FLAG
		         0); // H_FLAG S_FLAG X_FLAG Y_FLAG
	}
	s.R.setHL(res);
	s.OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::adc_hl_bc(S& s) { s.ADCW(s.R.getBC()); }
template <class T> void CPUCore<T>::adc_hl_de(S& s) { s.ADCW(s.R.getDE()); }
template <class T> void CPUCore<T>::adc_hl_sp(S& s) { s.ADCW(s.R.getSP()); }

// ADD HL/IX/IY,ss
template <class T> inline unsigned CPUCore<T>::ADDW(unsigned reg1, unsigned reg2)
{
	memptr = reg1 + 1; // not 16-bit
	unsigned res = reg1 + reg2;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	       ((res & 0x10000) ? C_FLAG : 0) |
	       ((res >> 8) & (X_FLAG | Y_FLAG)));
	T::OP_16_16_DELAY();
	return res & 0xFFFF;
}
template <class T> inline unsigned CPUCore<T>::ADDW2(unsigned reg)
{
	memptr = reg + 1; // not 16-bit
	unsigned res = 2 * reg;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       ((res & 0x10000) ? C_FLAG : 0) |
	       ((res >> 8) & (H_FLAG | X_FLAG | Y_FLAG)));
	T::OP_16_16_DELAY();
	return res & 0xFFFF;
}
template <class T> void CPUCore<T>::add_hl_bc(S& s) {
	s.R.setHL(s.ADDW(s.R.getHL(), s.R.getBC()));
}
template <class T> void CPUCore<T>::add_hl_de(S& s) {
	s.R.setHL(s.ADDW(s.R.getHL(), s.R.getDE()));
}
template <class T> void CPUCore<T>::add_hl_hl(S& s) {
	s.R.setHL(s.ADDW2(s.R.getHL()));
}
template <class T> void CPUCore<T>::add_hl_sp(S& s) {
	s.R.setHL(s.ADDW(s.R.getHL(), s.R.getSP()));
}
template <class T> void CPUCore<T>::add_ix_bc(S& s) {
	s.R.setIX(s.ADDW(s.R.getIX(), s.R.getBC()));
}
template <class T> void CPUCore<T>::add_ix_de(S& s) {
	s.R.setIX(s.ADDW(s.R.getIX(), s.R.getDE()));
}
template <class T> void CPUCore<T>::add_ix_ix(S& s) {
	s.R.setIX(s.ADDW2(s.R.getIX()));
}
template <class T> void CPUCore<T>::add_ix_sp(S& s) {
	s.R.setIX(s.ADDW(s.R.getIX(), s.R.getSP()));
}
template <class T> void CPUCore<T>::add_iy_bc(S& s) {
	s.R.setIY(s.ADDW(s.R.getIY(), s.R.getBC()));
}
template <class T> void CPUCore<T>::add_iy_de(S& s) {
	s.R.setIY(s.ADDW(s.R.getIY(), s.R.getDE()));
}
template <class T> void CPUCore<T>::add_iy_iy(S& s) {
	s.R.setIY(s.ADDW2(s.R.getIY()));
}
template <class T> void CPUCore<T>::add_iy_sp(S& s) {
	s.R.setIY(s.ADDW(s.R.getIY(), s.R.getSP()));
}

// SBC HL,ss
template <class T> inline void CPUCore<T>::SBCW(unsigned reg)
{
	memptr = R.getHL() + 1; // not 16-bit
	unsigned res = R.getHL() - reg - ((R.getF() & C_FLAG) ? 1 : 0);
	if (res & 0xFFFF) {
		R.setF((((R.getHL() ^ res ^ reg) >> 8) & H_FLAG) |
		       ((res & 0x10000) ? C_FLAG : 0) |
		       0 | // Z_FLAG
		       (((reg ^ R.getHL()) & (R.getHL() ^ res) & 0x8000) >> 13) | // V_FLAG
		       ((res >> 8) & (S_FLAG | X_FLAG | Y_FLAG)) |
		       N_FLAG);
	} else {
		R.setF((((R.getHL() ^ reg) >> 8) & H_FLAG) |
		       ((res & 0x10000) ? C_FLAG : 0) |
		       Z_FLAG |
		       (((reg ^ R.getHL()) & R.getHL() & 0x8000) >> 13) | // V_FLAG
		       0 | // S_FLAG X_FLAG Y_FLAG
		       N_FLAG);
	}
	R.setHL(res);
	T::OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::sbc_hl_hl(S& s)
{
	s.memptr = s.R.getHL() + 1; // not 16-bit
	if (s.R.getF() & C_FLAG) {
		s.R.setF(C_FLAG | H_FLAG | S_FLAG | X_FLAG | Y_FLAG | N_FLAG);
		s.R.setHL(0xFFFF);
	} else {
		s.R.setF(Z_FLAG | N_FLAG);
		s.R.setHL(0);
	}
	s.OP_16_16_DELAY();
}
template <class T> void CPUCore<T>::sbc_hl_bc(S& s) { s.SBCW(s.R.getBC()); }
template <class T> void CPUCore<T>::sbc_hl_de(S& s) { s.SBCW(s.R.getDE()); }
template <class T> void CPUCore<T>::sbc_hl_sp(S& s) { s.SBCW(s.R.getSP()); }


// DEC ss
template <class T> void CPUCore<T>::dec_bc(S& s) {
	s.R.setBC(s.R.getBC() - 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::dec_de(S& s) {
	s.R.setDE(s.R.getDE() - 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::dec_hl(S& s) {
	s.R.setHL(s.R.getHL() - 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::dec_ix(S& s) {
	s.R.setIX(s.R.getIX() - 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::dec_iy(S& s) {
	s.R.setIY(s.R.getIY() - 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::dec_sp(S& s) {
	s.R.setSP(s.R.getSP() - 1); s.INC_16_DELAY();
}

// INC ss
template <class T> void CPUCore<T>::inc_bc(S& s) {
	s.R.setBC(s.R.getBC() + 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::inc_de(S& s) {
	s.R.setDE(s.R.getDE() + 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::inc_hl(S& s) {
	s.R.setHL(s.R.getHL() + 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::inc_ix(S& s) {
	s.R.setIX(s.R.getIX() + 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::inc_iy(S& s) {
	s.R.setIY(s.R.getIY() + 1); s.INC_16_DELAY();
}
template <class T> void CPUCore<T>::inc_sp(S& s) {
	s.R.setSP(s.R.getSP() + 1); s.INC_16_DELAY();
}


// BIT n,r
template <class T> inline void CPUCore<T>::BIT(byte b, byte reg)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[reg & (1 << b)] |
	       (reg & (X_FLAG | Y_FLAG)) |
	       H_FLAG);
}
template <class T> void CPUCore<T>::bit_0_a(S& s) { s.BIT(0, s.R.getA()); }
template <class T> void CPUCore<T>::bit_0_b(S& s) { s.BIT(0, s.R.getB()); }
template <class T> void CPUCore<T>::bit_0_c(S& s) { s.BIT(0, s.R.getC()); }
template <class T> void CPUCore<T>::bit_0_d(S& s) { s.BIT(0, s.R.getD()); }
template <class T> void CPUCore<T>::bit_0_e(S& s) { s.BIT(0, s.R.getE()); }
template <class T> void CPUCore<T>::bit_0_h(S& s) { s.BIT(0, s.R.getH()); }
template <class T> void CPUCore<T>::bit_0_l(S& s) { s.BIT(0, s.R.getL()); }
template <class T> void CPUCore<T>::bit_1_a(S& s) { s.BIT(1, s.R.getA()); }
template <class T> void CPUCore<T>::bit_1_b(S& s) { s.BIT(1, s.R.getB()); }
template <class T> void CPUCore<T>::bit_1_c(S& s) { s.BIT(1, s.R.getC()); }
template <class T> void CPUCore<T>::bit_1_d(S& s) { s.BIT(1, s.R.getD()); }
template <class T> void CPUCore<T>::bit_1_e(S& s) { s.BIT(1, s.R.getE()); }
template <class T> void CPUCore<T>::bit_1_h(S& s) { s.BIT(1, s.R.getH()); }
template <class T> void CPUCore<T>::bit_1_l(S& s) { s.BIT(1, s.R.getL()); }
template <class T> void CPUCore<T>::bit_2_a(S& s) { s.BIT(2, s.R.getA()); }
template <class T> void CPUCore<T>::bit_2_b(S& s) { s.BIT(2, s.R.getB()); }
template <class T> void CPUCore<T>::bit_2_c(S& s) { s.BIT(2, s.R.getC()); }
template <class T> void CPUCore<T>::bit_2_d(S& s) { s.BIT(2, s.R.getD()); }
template <class T> void CPUCore<T>::bit_2_e(S& s) { s.BIT(2, s.R.getE()); }
template <class T> void CPUCore<T>::bit_2_h(S& s) { s.BIT(2, s.R.getH()); }
template <class T> void CPUCore<T>::bit_2_l(S& s) { s.BIT(2, s.R.getL()); }
template <class T> void CPUCore<T>::bit_3_a(S& s) { s.BIT(3, s.R.getA()); }
template <class T> void CPUCore<T>::bit_3_b(S& s) { s.BIT(3, s.R.getB()); }
template <class T> void CPUCore<T>::bit_3_c(S& s) { s.BIT(3, s.R.getC()); }
template <class T> void CPUCore<T>::bit_3_d(S& s) { s.BIT(3, s.R.getD()); }
template <class T> void CPUCore<T>::bit_3_e(S& s) { s.BIT(3, s.R.getE()); }
template <class T> void CPUCore<T>::bit_3_h(S& s) { s.BIT(3, s.R.getH()); }
template <class T> void CPUCore<T>::bit_3_l(S& s) { s.BIT(3, s.R.getL()); }
template <class T> void CPUCore<T>::bit_4_a(S& s) { s.BIT(4, s.R.getA()); }
template <class T> void CPUCore<T>::bit_4_b(S& s) { s.BIT(4, s.R.getB()); }
template <class T> void CPUCore<T>::bit_4_c(S& s) { s.BIT(4, s.R.getC()); }
template <class T> void CPUCore<T>::bit_4_d(S& s) { s.BIT(4, s.R.getD()); }
template <class T> void CPUCore<T>::bit_4_e(S& s) { s.BIT(4, s.R.getE()); }
template <class T> void CPUCore<T>::bit_4_h(S& s) { s.BIT(4, s.R.getH()); }
template <class T> void CPUCore<T>::bit_4_l(S& s) { s.BIT(4, s.R.getL()); }
template <class T> void CPUCore<T>::bit_5_a(S& s) { s.BIT(5, s.R.getA()); }
template <class T> void CPUCore<T>::bit_5_b(S& s) { s.BIT(5, s.R.getB()); }
template <class T> void CPUCore<T>::bit_5_c(S& s) { s.BIT(5, s.R.getC()); }
template <class T> void CPUCore<T>::bit_5_d(S& s) { s.BIT(5, s.R.getD()); }
template <class T> void CPUCore<T>::bit_5_e(S& s) { s.BIT(5, s.R.getE()); }
template <class T> void CPUCore<T>::bit_5_h(S& s) { s.BIT(5, s.R.getH()); }
template <class T> void CPUCore<T>::bit_5_l(S& s) { s.BIT(5, s.R.getL()); }
template <class T> void CPUCore<T>::bit_6_a(S& s) { s.BIT(6, s.R.getA()); }
template <class T> void CPUCore<T>::bit_6_b(S& s) { s.BIT(6, s.R.getB()); }
template <class T> void CPUCore<T>::bit_6_c(S& s) { s.BIT(6, s.R.getC()); }
template <class T> void CPUCore<T>::bit_6_d(S& s) { s.BIT(6, s.R.getD()); }
template <class T> void CPUCore<T>::bit_6_e(S& s) { s.BIT(6, s.R.getE()); }
template <class T> void CPUCore<T>::bit_6_h(S& s) { s.BIT(6, s.R.getH()); }
template <class T> void CPUCore<T>::bit_6_l(S& s) { s.BIT(6, s.R.getL()); }
template <class T> void CPUCore<T>::bit_7_a(S& s) { s.BIT(7, s.R.getA()); }
template <class T> void CPUCore<T>::bit_7_b(S& s) { s.BIT(7, s.R.getB()); }
template <class T> void CPUCore<T>::bit_7_c(S& s) { s.BIT(7, s.R.getC()); }
template <class T> void CPUCore<T>::bit_7_d(S& s) { s.BIT(7, s.R.getD()); }
template <class T> void CPUCore<T>::bit_7_e(S& s) { s.BIT(7, s.R.getE()); }
template <class T> void CPUCore<T>::bit_7_h(S& s) { s.BIT(7, s.R.getH()); }
template <class T> void CPUCore<T>::bit_7_l(S& s) { s.BIT(7, s.R.getL()); }

template <class T> inline void CPUCore<T>::BIT_HL(byte bit)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(R.getHL()) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	T::SMALL_DELAY();
}
template <class T> void CPUCore<T>::bit_0_xhl(S& s) { s.BIT_HL(0); }
template <class T> void CPUCore<T>::bit_1_xhl(S& s) { s.BIT_HL(1); }
template <class T> void CPUCore<T>::bit_2_xhl(S& s) { s.BIT_HL(2); }
template <class T> void CPUCore<T>::bit_3_xhl(S& s) { s.BIT_HL(3); }
template <class T> void CPUCore<T>::bit_4_xhl(S& s) { s.BIT_HL(4); }
template <class T> void CPUCore<T>::bit_5_xhl(S& s) { s.BIT_HL(5); }
template <class T> void CPUCore<T>::bit_6_xhl(S& s) { s.BIT_HL(6); }
template <class T> void CPUCore<T>::bit_7_xhl(S& s) { s.BIT_HL(7); }

template <class T> inline void CPUCore<T>::BIT_IX(byte bit, int ofst)
{
	memptr = (R.getIX() + ofst) & 0xFFFF;
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(memptr) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	T::SMALL_DELAY();
}
template <class T> void CPUCore<T>::bit_0_xix(S& s, int o) { s.BIT_IX(0, o); }
template <class T> void CPUCore<T>::bit_1_xix(S& s, int o) { s.BIT_IX(1, o); }
template <class T> void CPUCore<T>::bit_2_xix(S& s, int o) { s.BIT_IX(2, o); }
template <class T> void CPUCore<T>::bit_3_xix(S& s, int o) { s.BIT_IX(3, o); }
template <class T> void CPUCore<T>::bit_4_xix(S& s, int o) { s.BIT_IX(4, o); }
template <class T> void CPUCore<T>::bit_5_xix(S& s, int o) { s.BIT_IX(5, o); }
template <class T> void CPUCore<T>::bit_6_xix(S& s, int o) { s.BIT_IX(6, o); }
template <class T> void CPUCore<T>::bit_7_xix(S& s, int o) { s.BIT_IX(7, o); }

template <class T> inline void CPUCore<T>::BIT_IY(byte bit, int ofst)
{
	memptr = (R.getIY() + ofst) & 0xFFFF;
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(memptr) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	T::SMALL_DELAY();
}
template <class T> void CPUCore<T>::bit_0_xiy(S& s, int o) { s.BIT_IY(0, o); }
template <class T> void CPUCore<T>::bit_1_xiy(S& s, int o) { s.BIT_IY(1, o); }
template <class T> void CPUCore<T>::bit_2_xiy(S& s, int o) { s.BIT_IY(2, o); }
template <class T> void CPUCore<T>::bit_3_xiy(S& s, int o) { s.BIT_IY(3, o); }
template <class T> void CPUCore<T>::bit_4_xiy(S& s, int o) { s.BIT_IY(4, o); }
template <class T> void CPUCore<T>::bit_5_xiy(S& s, int o) { s.BIT_IY(5, o); }
template <class T> void CPUCore<T>::bit_6_xiy(S& s, int o) { s.BIT_IY(6, o); }
template <class T> void CPUCore<T>::bit_7_xiy(S& s, int o) { s.BIT_IY(7, o); }


// RES n,r
template <class T> inline byte CPUCore<T>::RES(unsigned b, byte reg)
{
	return reg & ~(1 << b);
}
template <class T> void CPUCore<T>::res_0_a(S& s) { s.R.setA(s.RES(0, s.R.getA())); }
template <class T> void CPUCore<T>::res_0_b(S& s) { s.R.setB(s.RES(0, s.R.getB())); }
template <class T> void CPUCore<T>::res_0_c(S& s) { s.R.setC(s.RES(0, s.R.getC())); }
template <class T> void CPUCore<T>::res_0_d(S& s) { s.R.setD(s.RES(0, s.R.getD())); }
template <class T> void CPUCore<T>::res_0_e(S& s) { s.R.setE(s.RES(0, s.R.getE())); }
template <class T> void CPUCore<T>::res_0_h(S& s) { s.R.setH(s.RES(0, s.R.getH())); }
template <class T> void CPUCore<T>::res_0_l(S& s) { s.R.setL(s.RES(0, s.R.getL())); }
template <class T> void CPUCore<T>::res_1_a(S& s) { s.R.setA(s.RES(1, s.R.getA())); }
template <class T> void CPUCore<T>::res_1_b(S& s) { s.R.setB(s.RES(1, s.R.getB())); }
template <class T> void CPUCore<T>::res_1_c(S& s) { s.R.setC(s.RES(1, s.R.getC())); }
template <class T> void CPUCore<T>::res_1_d(S& s) { s.R.setD(s.RES(1, s.R.getD())); }
template <class T> void CPUCore<T>::res_1_e(S& s) { s.R.setE(s.RES(1, s.R.getE())); }
template <class T> void CPUCore<T>::res_1_h(S& s) { s.R.setH(s.RES(1, s.R.getH())); }
template <class T> void CPUCore<T>::res_1_l(S& s) { s.R.setL(s.RES(1, s.R.getL())); }
template <class T> void CPUCore<T>::res_2_a(S& s) { s.R.setA(s.RES(2, s.R.getA())); }
template <class T> void CPUCore<T>::res_2_b(S& s) { s.R.setB(s.RES(2, s.R.getB())); }
template <class T> void CPUCore<T>::res_2_c(S& s) { s.R.setC(s.RES(2, s.R.getC())); }
template <class T> void CPUCore<T>::res_2_d(S& s) { s.R.setD(s.RES(2, s.R.getD())); }
template <class T> void CPUCore<T>::res_2_e(S& s) { s.R.setE(s.RES(2, s.R.getE())); }
template <class T> void CPUCore<T>::res_2_h(S& s) { s.R.setH(s.RES(2, s.R.getH())); }
template <class T> void CPUCore<T>::res_2_l(S& s) { s.R.setL(s.RES(2, s.R.getL())); }
template <class T> void CPUCore<T>::res_3_a(S& s) { s.R.setA(s.RES(3, s.R.getA())); }
template <class T> void CPUCore<T>::res_3_b(S& s) { s.R.setB(s.RES(3, s.R.getB())); }
template <class T> void CPUCore<T>::res_3_c(S& s) { s.R.setC(s.RES(3, s.R.getC())); }
template <class T> void CPUCore<T>::res_3_d(S& s) { s.R.setD(s.RES(3, s.R.getD())); }
template <class T> void CPUCore<T>::res_3_e(S& s) { s.R.setE(s.RES(3, s.R.getE())); }
template <class T> void CPUCore<T>::res_3_h(S& s) { s.R.setH(s.RES(3, s.R.getH())); }
template <class T> void CPUCore<T>::res_3_l(S& s) { s.R.setL(s.RES(3, s.R.getL())); }
template <class T> void CPUCore<T>::res_4_a(S& s) { s.R.setA(s.RES(4, s.R.getA())); }
template <class T> void CPUCore<T>::res_4_b(S& s) { s.R.setB(s.RES(4, s.R.getB())); }
template <class T> void CPUCore<T>::res_4_c(S& s) { s.R.setC(s.RES(4, s.R.getC())); }
template <class T> void CPUCore<T>::res_4_d(S& s) { s.R.setD(s.RES(4, s.R.getD())); }
template <class T> void CPUCore<T>::res_4_e(S& s) { s.R.setE(s.RES(4, s.R.getE())); }
template <class T> void CPUCore<T>::res_4_h(S& s) { s.R.setH(s.RES(4, s.R.getH())); }
template <class T> void CPUCore<T>::res_4_l(S& s) { s.R.setL(s.RES(4, s.R.getL())); }
template <class T> void CPUCore<T>::res_5_a(S& s) { s.R.setA(s.RES(5, s.R.getA())); }
template <class T> void CPUCore<T>::res_5_b(S& s) { s.R.setB(s.RES(5, s.R.getB())); }
template <class T> void CPUCore<T>::res_5_c(S& s) { s.R.setC(s.RES(5, s.R.getC())); }
template <class T> void CPUCore<T>::res_5_d(S& s) { s.R.setD(s.RES(5, s.R.getD())); }
template <class T> void CPUCore<T>::res_5_e(S& s) { s.R.setE(s.RES(5, s.R.getE())); }
template <class T> void CPUCore<T>::res_5_h(S& s) { s.R.setH(s.RES(5, s.R.getH())); }
template <class T> void CPUCore<T>::res_5_l(S& s) { s.R.setL(s.RES(5, s.R.getL())); }
template <class T> void CPUCore<T>::res_6_a(S& s) { s.R.setA(s.RES(6, s.R.getA())); }
template <class T> void CPUCore<T>::res_6_b(S& s) { s.R.setB(s.RES(6, s.R.getB())); }
template <class T> void CPUCore<T>::res_6_c(S& s) { s.R.setC(s.RES(6, s.R.getC())); }
template <class T> void CPUCore<T>::res_6_d(S& s) { s.R.setD(s.RES(6, s.R.getD())); }
template <class T> void CPUCore<T>::res_6_e(S& s) { s.R.setE(s.RES(6, s.R.getE())); }
template <class T> void CPUCore<T>::res_6_h(S& s) { s.R.setH(s.RES(6, s.R.getH())); }
template <class T> void CPUCore<T>::res_6_l(S& s) { s.R.setL(s.RES(6, s.R.getL())); }
template <class T> void CPUCore<T>::res_7_a(S& s) { s.R.setA(s.RES(7, s.R.getA())); }
template <class T> void CPUCore<T>::res_7_b(S& s) { s.R.setB(s.RES(7, s.R.getB())); }
template <class T> void CPUCore<T>::res_7_c(S& s) { s.R.setC(s.RES(7, s.R.getC())); }
template <class T> void CPUCore<T>::res_7_d(S& s) { s.R.setD(s.RES(7, s.R.getD())); }
template <class T> void CPUCore<T>::res_7_e(S& s) { s.R.setE(s.RES(7, s.R.getE())); }
template <class T> void CPUCore<T>::res_7_h(S& s) { s.R.setH(s.RES(7, s.R.getH())); }
template <class T> void CPUCore<T>::res_7_l(S& s) { s.R.setL(s.RES(7, s.R.getL())); }

template <class T> inline byte CPUCore<T>::RES_X(unsigned bit, unsigned x)
{
	byte res = RES(bit, RDMEM(x));
	T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RES_X_(unsigned bit, unsigned x)
{
	memptr = x;
	return RES_X(bit, x);
}
template <class T> inline byte CPUCore<T>::RES_X_X(unsigned bit, int ofst)
{
	return RES_X_(bit, (R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RES_X_Y(unsigned bit, int ofst)
{
	return RES_X_(bit, (R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::res_0_xhl(S& s)   { s.RES_X(0, s.R.getHL()); }
template <class T> void CPUCore<T>::res_1_xhl(S& s)   { s.RES_X(1, s.R.getHL()); }
template <class T> void CPUCore<T>::res_2_xhl(S& s)   { s.RES_X(2, s.R.getHL()); }
template <class T> void CPUCore<T>::res_3_xhl(S& s)   { s.RES_X(3, s.R.getHL()); }
template <class T> void CPUCore<T>::res_4_xhl(S& s)   { s.RES_X(4, s.R.getHL()); }
template <class T> void CPUCore<T>::res_5_xhl(S& s)   { s.RES_X(5, s.R.getHL()); }
template <class T> void CPUCore<T>::res_6_xhl(S& s)   { s.RES_X(6, s.R.getHL()); }
template <class T> void CPUCore<T>::res_7_xhl(S& s)   { s.RES_X(7, s.R.getHL()); }

template <class T> void CPUCore<T>::res_0_xix(S& s, int o)   { s.RES_X_X(0, o); }
template <class T> void CPUCore<T>::res_1_xix(S& s, int o)   { s.RES_X_X(1, o); }
template <class T> void CPUCore<T>::res_2_xix(S& s, int o)   { s.RES_X_X(2, o); }
template <class T> void CPUCore<T>::res_3_xix(S& s, int o)   { s.RES_X_X(3, o); }
template <class T> void CPUCore<T>::res_4_xix(S& s, int o)   { s.RES_X_X(4, o); }
template <class T> void CPUCore<T>::res_5_xix(S& s, int o)   { s.RES_X_X(5, o); }
template <class T> void CPUCore<T>::res_6_xix(S& s, int o)   { s.RES_X_X(6, o); }
template <class T> void CPUCore<T>::res_7_xix(S& s, int o)   { s.RES_X_X(7, o); }

template <class T> void CPUCore<T>::res_0_xiy(S& s, int o)   { s.RES_X_Y(0, o); }
template <class T> void CPUCore<T>::res_1_xiy(S& s, int o)   { s.RES_X_Y(1, o); }
template <class T> void CPUCore<T>::res_2_xiy(S& s, int o)   { s.RES_X_Y(2, o); }
template <class T> void CPUCore<T>::res_3_xiy(S& s, int o)   { s.RES_X_Y(3, o); }
template <class T> void CPUCore<T>::res_4_xiy(S& s, int o)   { s.RES_X_Y(4, o); }
template <class T> void CPUCore<T>::res_5_xiy(S& s, int o)   { s.RES_X_Y(5, o); }
template <class T> void CPUCore<T>::res_6_xiy(S& s, int o)   { s.RES_X_Y(6, o); }
template <class T> void CPUCore<T>::res_7_xiy(S& s, int o)   { s.RES_X_Y(7, o); }

template <class T> void CPUCore<T>::res_0_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_1_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_2_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_3_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_4_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_5_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_6_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_7_xix_a(S& s, int o) { s.R.setA(s.RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_b(S& s, int o) { s.R.setB(s.RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_c(S& s, int o) { s.R.setC(s.RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_d(S& s, int o) { s.R.setD(s.RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_e(S& s, int o) { s.R.setE(s.RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_h(S& s, int o) { s.R.setH(s.RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_l(S& s, int o) { s.R.setL(s.RES_X_X(7, o)); }

template <class T> void CPUCore<T>::res_0_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_1_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_2_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_3_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_4_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_5_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_6_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_7_xiy_a(S& s, int o) { s.R.setA(s.RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_b(S& s, int o) { s.R.setB(s.RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_c(S& s, int o) { s.R.setC(s.RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_d(S& s, int o) { s.R.setD(s.RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_e(S& s, int o) { s.R.setE(s.RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_h(S& s, int o) { s.R.setH(s.RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_l(S& s, int o) { s.R.setL(s.RES_X_Y(7, o)); }


// SET n,r
template <class T> inline byte CPUCore<T>::SET(unsigned b, byte reg)
{
	return reg | (1 << b);
}
template <class T> void CPUCore<T>::set_0_a(S& s) { s.R.setA(s.SET(0, s.R.getA())); }
template <class T> void CPUCore<T>::set_0_b(S& s) { s.R.setB(s.SET(0, s.R.getB())); }
template <class T> void CPUCore<T>::set_0_c(S& s) { s.R.setC(s.SET(0, s.R.getC())); }
template <class T> void CPUCore<T>::set_0_d(S& s) { s.R.setD(s.SET(0, s.R.getD())); }
template <class T> void CPUCore<T>::set_0_e(S& s) { s.R.setE(s.SET(0, s.R.getE())); }
template <class T> void CPUCore<T>::set_0_h(S& s) { s.R.setH(s.SET(0, s.R.getH())); }
template <class T> void CPUCore<T>::set_0_l(S& s) { s.R.setL(s.SET(0, s.R.getL())); }
template <class T> void CPUCore<T>::set_1_a(S& s) { s.R.setA(s.SET(1, s.R.getA())); }
template <class T> void CPUCore<T>::set_1_b(S& s) { s.R.setB(s.SET(1, s.R.getB())); }
template <class T> void CPUCore<T>::set_1_c(S& s) { s.R.setC(s.SET(1, s.R.getC())); }
template <class T> void CPUCore<T>::set_1_d(S& s) { s.R.setD(s.SET(1, s.R.getD())); }
template <class T> void CPUCore<T>::set_1_e(S& s) { s.R.setE(s.SET(1, s.R.getE())); }
template <class T> void CPUCore<T>::set_1_h(S& s) { s.R.setH(s.SET(1, s.R.getH())); }
template <class T> void CPUCore<T>::set_1_l(S& s) { s.R.setL(s.SET(1, s.R.getL())); }
template <class T> void CPUCore<T>::set_2_a(S& s) { s.R.setA(s.SET(2, s.R.getA())); }
template <class T> void CPUCore<T>::set_2_b(S& s) { s.R.setB(s.SET(2, s.R.getB())); }
template <class T> void CPUCore<T>::set_2_c(S& s) { s.R.setC(s.SET(2, s.R.getC())); }
template <class T> void CPUCore<T>::set_2_d(S& s) { s.R.setD(s.SET(2, s.R.getD())); }
template <class T> void CPUCore<T>::set_2_e(S& s) { s.R.setE(s.SET(2, s.R.getE())); }
template <class T> void CPUCore<T>::set_2_h(S& s) { s.R.setH(s.SET(2, s.R.getH())); }
template <class T> void CPUCore<T>::set_2_l(S& s) { s.R.setL(s.SET(2, s.R.getL())); }
template <class T> void CPUCore<T>::set_3_a(S& s) { s.R.setA(s.SET(3, s.R.getA())); }
template <class T> void CPUCore<T>::set_3_b(S& s) { s.R.setB(s.SET(3, s.R.getB())); }
template <class T> void CPUCore<T>::set_3_c(S& s) { s.R.setC(s.SET(3, s.R.getC())); }
template <class T> void CPUCore<T>::set_3_d(S& s) { s.R.setD(s.SET(3, s.R.getD())); }
template <class T> void CPUCore<T>::set_3_e(S& s) { s.R.setE(s.SET(3, s.R.getE())); }
template <class T> void CPUCore<T>::set_3_h(S& s) { s.R.setH(s.SET(3, s.R.getH())); }
template <class T> void CPUCore<T>::set_3_l(S& s) { s.R.setL(s.SET(3, s.R.getL())); }
template <class T> void CPUCore<T>::set_4_a(S& s) { s.R.setA(s.SET(4, s.R.getA())); }
template <class T> void CPUCore<T>::set_4_b(S& s) { s.R.setB(s.SET(4, s.R.getB())); }
template <class T> void CPUCore<T>::set_4_c(S& s) { s.R.setC(s.SET(4, s.R.getC())); }
template <class T> void CPUCore<T>::set_4_d(S& s) { s.R.setD(s.SET(4, s.R.getD())); }
template <class T> void CPUCore<T>::set_4_e(S& s) { s.R.setE(s.SET(4, s.R.getE())); }
template <class T> void CPUCore<T>::set_4_h(S& s) { s.R.setH(s.SET(4, s.R.getH())); }
template <class T> void CPUCore<T>::set_4_l(S& s) { s.R.setL(s.SET(4, s.R.getL())); }
template <class T> void CPUCore<T>::set_5_a(S& s) { s.R.setA(s.SET(5, s.R.getA())); }
template <class T> void CPUCore<T>::set_5_b(S& s) { s.R.setB(s.SET(5, s.R.getB())); }
template <class T> void CPUCore<T>::set_5_c(S& s) { s.R.setC(s.SET(5, s.R.getC())); }
template <class T> void CPUCore<T>::set_5_d(S& s) { s.R.setD(s.SET(5, s.R.getD())); }
template <class T> void CPUCore<T>::set_5_e(S& s) { s.R.setE(s.SET(5, s.R.getE())); }
template <class T> void CPUCore<T>::set_5_h(S& s) { s.R.setH(s.SET(5, s.R.getH())); }
template <class T> void CPUCore<T>::set_5_l(S& s) { s.R.setL(s.SET(5, s.R.getL())); }
template <class T> void CPUCore<T>::set_6_a(S& s) { s.R.setA(s.SET(6, s.R.getA())); }
template <class T> void CPUCore<T>::set_6_b(S& s) { s.R.setB(s.SET(6, s.R.getB())); }
template <class T> void CPUCore<T>::set_6_c(S& s) { s.R.setC(s.SET(6, s.R.getC())); }
template <class T> void CPUCore<T>::set_6_d(S& s) { s.R.setD(s.SET(6, s.R.getD())); }
template <class T> void CPUCore<T>::set_6_e(S& s) { s.R.setE(s.SET(6, s.R.getE())); }
template <class T> void CPUCore<T>::set_6_h(S& s) { s.R.setH(s.SET(6, s.R.getH())); }
template <class T> void CPUCore<T>::set_6_l(S& s) { s.R.setL(s.SET(6, s.R.getL())); }
template <class T> void CPUCore<T>::set_7_a(S& s) { s.R.setA(s.SET(7, s.R.getA())); }
template <class T> void CPUCore<T>::set_7_b(S& s) { s.R.setB(s.SET(7, s.R.getB())); }
template <class T> void CPUCore<T>::set_7_c(S& s) { s.R.setC(s.SET(7, s.R.getC())); }
template <class T> void CPUCore<T>::set_7_d(S& s) { s.R.setD(s.SET(7, s.R.getD())); }
template <class T> void CPUCore<T>::set_7_e(S& s) { s.R.setE(s.SET(7, s.R.getE())); }
template <class T> void CPUCore<T>::set_7_h(S& s) { s.R.setH(s.SET(7, s.R.getH())); }
template <class T> void CPUCore<T>::set_7_l(S& s) { s.R.setL(s.SET(7, s.R.getL())); }

template <class T> inline byte CPUCore<T>::SET_X(unsigned bit, unsigned x)
{
	byte res = SET(bit, RDMEM(x));
	T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SET_X_(unsigned bit, unsigned x)
{
	memptr = x;
	return SET_X(bit, x);
}
template <class T> inline byte CPUCore<T>::SET_X_X(unsigned bit, int ofst)
{
	return SET_X_(bit, (R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SET_X_Y(unsigned bit, int ofst)
{
	return SET_X_(bit, (R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::set_0_xhl(S& s)   { s.SET_X(0, s.R.getHL()); }
template <class T> void CPUCore<T>::set_1_xhl(S& s)   { s.SET_X(1, s.R.getHL()); }
template <class T> void CPUCore<T>::set_2_xhl(S& s)   { s.SET_X(2, s.R.getHL()); }
template <class T> void CPUCore<T>::set_3_xhl(S& s)   { s.SET_X(3, s.R.getHL()); }
template <class T> void CPUCore<T>::set_4_xhl(S& s)   { s.SET_X(4, s.R.getHL()); }
template <class T> void CPUCore<T>::set_5_xhl(S& s)   { s.SET_X(5, s.R.getHL()); }
template <class T> void CPUCore<T>::set_6_xhl(S& s)   { s.SET_X(6, s.R.getHL()); }
template <class T> void CPUCore<T>::set_7_xhl(S& s)   { s.SET_X(7, s.R.getHL()); }

template <class T> void CPUCore<T>::set_0_xix(S& s, int o)   { s.SET_X_X(0, o); }
template <class T> void CPUCore<T>::set_1_xix(S& s, int o)   { s.SET_X_X(1, o); }
template <class T> void CPUCore<T>::set_2_xix(S& s, int o)   { s.SET_X_X(2, o); }
template <class T> void CPUCore<T>::set_3_xix(S& s, int o)   { s.SET_X_X(3, o); }
template <class T> void CPUCore<T>::set_4_xix(S& s, int o)   { s.SET_X_X(4, o); }
template <class T> void CPUCore<T>::set_5_xix(S& s, int o)   { s.SET_X_X(5, o); }
template <class T> void CPUCore<T>::set_6_xix(S& s, int o)   { s.SET_X_X(6, o); }
template <class T> void CPUCore<T>::set_7_xix(S& s, int o)   { s.SET_X_X(7, o); }

template <class T> void CPUCore<T>::set_0_xiy(S& s, int o)   { s.SET_X_Y(0, o); }
template <class T> void CPUCore<T>::set_1_xiy(S& s, int o)   { s.SET_X_Y(1, o); }
template <class T> void CPUCore<T>::set_2_xiy(S& s, int o)   { s.SET_X_Y(2, o); }
template <class T> void CPUCore<T>::set_3_xiy(S& s, int o)   { s.SET_X_Y(3, o); }
template <class T> void CPUCore<T>::set_4_xiy(S& s, int o)   { s.SET_X_Y(4, o); }
template <class T> void CPUCore<T>::set_5_xiy(S& s, int o)   { s.SET_X_Y(5, o); }
template <class T> void CPUCore<T>::set_6_xiy(S& s, int o)   { s.SET_X_Y(6, o); }
template <class T> void CPUCore<T>::set_7_xiy(S& s, int o)   { s.SET_X_Y(7, o); }

template <class T> void CPUCore<T>::set_0_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_1_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_2_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_3_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_4_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_5_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_6_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_7_xix_a(S& s, int o) { s.R.setA(s.SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_b(S& s, int o) { s.R.setB(s.SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_c(S& s, int o) { s.R.setC(s.SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_d(S& s, int o) { s.R.setD(s.SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_e(S& s, int o) { s.R.setE(s.SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_h(S& s, int o) { s.R.setH(s.SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_l(S& s, int o) { s.R.setL(s.SET_X_X(7, o)); }

template <class T> void CPUCore<T>::set_0_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_1_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_2_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_3_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_4_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_5_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_6_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_7_xiy_a(S& s, int o) { s.R.setA(s.SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_b(S& s, int o) { s.R.setB(s.SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_c(S& s, int o) { s.R.setC(s.SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_d(S& s, int o) { s.R.setD(s.SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_e(S& s, int o) { s.R.setE(s.SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_h(S& s, int o) { s.R.setH(s.SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_l(S& s, int o) { s.R.setL(s.SET_X_Y(7, o)); }


// RL r
template <class T> inline byte CPUCore<T>::RL(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | ((R.getF() & C_FLAG) ? 0x01 : 0);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RL_X(unsigned x)
{
	byte res = RL(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RL_X_(unsigned x)
{
	memptr = x; return RL_X(x);
}
template <class T> inline byte CPUCore<T>::RL_X_X(int ofst)
{
	return RL_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RL_X_Y(int ofst)
{
	return RL_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rl_a(S& s) { s.R.setA(s.RL(s.R.getA())); }
template <class T> void CPUCore<T>::rl_b(S& s) { s.R.setB(s.RL(s.R.getB())); }
template <class T> void CPUCore<T>::rl_c(S& s) { s.R.setC(s.RL(s.R.getC())); }
template <class T> void CPUCore<T>::rl_d(S& s) { s.R.setD(s.RL(s.R.getD())); }
template <class T> void CPUCore<T>::rl_e(S& s) { s.R.setE(s.RL(s.R.getE())); }
template <class T> void CPUCore<T>::rl_h(S& s) { s.R.setH(s.RL(s.R.getH())); }
template <class T> void CPUCore<T>::rl_l(S& s) { s.R.setL(s.RL(s.R.getL())); }

template <class T> void CPUCore<T>::rl_xhl(S& s)   { s.RL_X(s.R.getHL()); }
template <class T> void CPUCore<T>::rl_xix(S& s, int o)   { s.RL_X_X(o); }
template <class T> void CPUCore<T>::rl_xiy(S& s, int o)   { s.RL_X_Y(o); }

template <class T> void CPUCore<T>::rl_xix_a(S& s, int o) { s.R.setA(s.RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_b(S& s, int o) { s.R.setB(s.RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_c(S& s, int o) { s.R.setC(s.RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_d(S& s, int o) { s.R.setD(s.RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_e(S& s, int o) { s.R.setE(s.RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_h(S& s, int o) { s.R.setH(s.RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_l(S& s, int o) { s.R.setL(s.RL_X_X(o)); }

template <class T> void CPUCore<T>::rl_xiy_a(S& s, int o) { s.R.setA(s.RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_b(S& s, int o) { s.R.setB(s.RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_c(S& s, int o) { s.R.setC(s.RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_d(S& s, int o) { s.R.setD(s.RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_e(S& s, int o) { s.R.setE(s.RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_h(S& s, int o) { s.R.setH(s.RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_l(S& s, int o) { s.R.setL(s.RL_X_Y(o)); }


// RLC r
template <class T> inline byte CPUCore<T>::RLC(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | c;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RLC_X(unsigned x)
{
	byte res = RLC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RLC_X_(unsigned x)
{
	memptr = x; return RLC_X(x);
}
template <class T> inline byte CPUCore<T>::RLC_X_X(int ofst)
{
	return RLC_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RLC_X_Y(int ofst)
{
	return RLC_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rlc_a(S& s) { s.R.setA(s.RLC(s.R.getA())); }
template <class T> void CPUCore<T>::rlc_b(S& s) { s.R.setB(s.RLC(s.R.getB())); }
template <class T> void CPUCore<T>::rlc_c(S& s) { s.R.setC(s.RLC(s.R.getC())); }
template <class T> void CPUCore<T>::rlc_d(S& s) { s.R.setD(s.RLC(s.R.getD())); }
template <class T> void CPUCore<T>::rlc_e(S& s) { s.R.setE(s.RLC(s.R.getE())); }
template <class T> void CPUCore<T>::rlc_h(S& s) { s.R.setH(s.RLC(s.R.getH())); }
template <class T> void CPUCore<T>::rlc_l(S& s) { s.R.setL(s.RLC(s.R.getL())); }

template <class T> void CPUCore<T>::rlc_xhl(S& s)          { s.RLC_X(s.R.getHL()); }
template <class T> void CPUCore<T>::rlc_xix(S& s, int o)   { s.RLC_X_X(o); }
template <class T> void CPUCore<T>::rlc_xiy(S& s, int o)   { s.RLC_X_Y(o); }

template <class T> void CPUCore<T>::rlc_xix_a(S& s, int o) { s.R.setA(s.RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_b(S& s, int o) { s.R.setB(s.RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_c(S& s, int o) { s.R.setC(s.RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_d(S& s, int o) { s.R.setD(s.RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_e(S& s, int o) { s.R.setE(s.RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_h(S& s, int o) { s.R.setH(s.RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_l(S& s, int o) { s.R.setL(s.RLC_X_X(o)); }

template <class T> void CPUCore<T>::rlc_xiy_a(S& s, int o) { s.R.setA(s.RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_b(S& s, int o) { s.R.setB(s.RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_c(S& s, int o) { s.R.setC(s.RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_d(S& s, int o) { s.R.setD(s.RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_e(S& s, int o) { s.R.setE(s.RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_h(S& s, int o) { s.R.setH(s.RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_l(S& s, int o) { s.R.setL(s.RLC_X_Y(o)); }


// RR r
template <class T> inline byte CPUCore<T>::RR(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | ((R.getF() & C_FLAG) << 7);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RR_X(unsigned x)
{
	byte res = RR(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RR_X_(unsigned x)
{
	memptr = x; return RR_X(x);
}
template <class T> inline byte CPUCore<T>::RR_X_X(int ofst)
{
	return RR_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RR_X_Y(int ofst)
{
	return RR_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rr_a(S& s) { s.R.setA(s.RR(s.R.getA())); }
template <class T> void CPUCore<T>::rr_b(S& s) { s.R.setB(s.RR(s.R.getB())); }
template <class T> void CPUCore<T>::rr_c(S& s) { s.R.setC(s.RR(s.R.getC())); }
template <class T> void CPUCore<T>::rr_d(S& s) { s.R.setD(s.RR(s.R.getD())); }
template <class T> void CPUCore<T>::rr_e(S& s) { s.R.setE(s.RR(s.R.getE())); }
template <class T> void CPUCore<T>::rr_h(S& s) { s.R.setH(s.RR(s.R.getH())); }
template <class T> void CPUCore<T>::rr_l(S& s) { s.R.setL(s.RR(s.R.getL())); }

template <class T> void CPUCore<T>::rr_xhl(S& s) { s.RR_X(s.R.getHL()); }
template <class T> void CPUCore<T>::rr_xix(S& s, int o)   { s.RR_X_X(o); }
template <class T> void CPUCore<T>::rr_xiy(S& s, int o)   { s.RR_X_Y(o); }

template <class T> void CPUCore<T>::rr_xix_a(S& s, int o) { s.R.setA(s.RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_b(S& s, int o) { s.R.setB(s.RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_c(S& s, int o) { s.R.setC(s.RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_d(S& s, int o) { s.R.setD(s.RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_e(S& s, int o) { s.R.setE(s.RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_h(S& s, int o) { s.R.setH(s.RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_l(S& s, int o) { s.R.setL(s.RR_X_X(o)); }

template <class T> void CPUCore<T>::rr_xiy_a(S& s, int o) { s.R.setA(s.RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_b(S& s, int o) { s.R.setB(s.RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_c(S& s, int o) { s.R.setC(s.RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_d(S& s, int o) { s.R.setD(s.RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_e(S& s, int o) { s.R.setE(s.RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_h(S& s, int o) { s.R.setH(s.RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_l(S& s, int o) { s.R.setL(s.RR_X_Y(o)); }


// RRC r
template <class T> inline byte CPUCore<T>::RRC(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (c << 7);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RRC_X(unsigned x)
{
	byte res = RRC(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::RRC_X_(unsigned x)
{
	memptr = x; return RRC_X(x);
}
template <class T> inline byte CPUCore<T>::RRC_X_X(int ofst)
{
	return RRC_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RRC_X_Y(int ofst)
{
	return RRC_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rrc_a(S& s) { s.R.setA(s.RRC(s.R.getA())); }
template <class T> void CPUCore<T>::rrc_b(S& s) { s.R.setB(s.RRC(s.R.getB())); }
template <class T> void CPUCore<T>::rrc_c(S& s) { s.R.setC(s.RRC(s.R.getC())); }
template <class T> void CPUCore<T>::rrc_d(S& s) { s.R.setD(s.RRC(s.R.getD())); }
template <class T> void CPUCore<T>::rrc_e(S& s) { s.R.setE(s.RRC(s.R.getE())); }
template <class T> void CPUCore<T>::rrc_h(S& s) { s.R.setH(s.RRC(s.R.getH())); }
template <class T> void CPUCore<T>::rrc_l(S& s) { s.R.setL(s.RRC(s.R.getL())); }

template <class T> void CPUCore<T>::rrc_xhl(S& s) { s.RRC_X(s.R.getHL()); }
template <class T> void CPUCore<T>::rrc_xix(S& s, int o)   { s.RRC_X_X(o); }
template <class T> void CPUCore<T>::rrc_xiy(S& s, int o)   { s.RRC_X_Y(o); }

template <class T> void CPUCore<T>::rrc_xix_a(S& s, int o) { s.R.setA(s.RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_b(S& s, int o) { s.R.setB(s.RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_c(S& s, int o) { s.R.setC(s.RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_d(S& s, int o) { s.R.setD(s.RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_e(S& s, int o) { s.R.setE(s.RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_h(S& s, int o) { s.R.setH(s.RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_l(S& s, int o) { s.R.setL(s.RRC_X_X(o)); }

template <class T> void CPUCore<T>::rrc_xiy_a(S& s, int o) { s.R.setA(s.RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_b(S& s, int o) { s.R.setB(s.RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_c(S& s, int o) { s.R.setC(s.RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_d(S& s, int o) { s.R.setD(s.RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_e(S& s, int o) { s.R.setE(s.RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_h(S& s, int o) { s.R.setH(s.RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_l(S& s, int o) { s.R.setL(s.RRC_X_Y(o)); }


// SLA r
template <class T> inline byte CPUCore<T>::SLA(byte reg)
{
	byte c = reg >> 7;
	reg <<= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLA_X(unsigned x)
{
	byte res = SLA(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SLA_X_(unsigned x)
{
	memptr = x; return SLA_X(x);
}
template <class T> inline byte CPUCore<T>::SLA_X_X(int ofst)
{
	return SLA_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SLA_X_Y(int ofst)
{
	return SLA_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::sla_a(S& s) { s.R.setA(s.SLA(s.R.getA())); }
template <class T> void CPUCore<T>::sla_b(S& s) { s.R.setB(s.SLA(s.R.getB())); }
template <class T> void CPUCore<T>::sla_c(S& s) { s.R.setC(s.SLA(s.R.getC())); }
template <class T> void CPUCore<T>::sla_d(S& s) { s.R.setD(s.SLA(s.R.getD())); }
template <class T> void CPUCore<T>::sla_e(S& s) { s.R.setE(s.SLA(s.R.getE())); }
template <class T> void CPUCore<T>::sla_h(S& s) { s.R.setH(s.SLA(s.R.getH())); }
template <class T> void CPUCore<T>::sla_l(S& s) { s.R.setL(s.SLA(s.R.getL())); }

template <class T> void CPUCore<T>::sla_xhl(S& s) { s.SLA_X(s.R.getHL()); }
template <class T> void CPUCore<T>::sla_xix(S& s, int o)   { s.SLA_X_X(o); }
template <class T> void CPUCore<T>::sla_xiy(S& s, int o)   { s.SLA_X_Y(o); }

template <class T> void CPUCore<T>::sla_xix_a(S& s, int o) { s.R.setA(s.SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_b(S& s, int o) { s.R.setB(s.SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_c(S& s, int o) { s.R.setC(s.SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_d(S& s, int o) { s.R.setD(s.SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_e(S& s, int o) { s.R.setE(s.SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_h(S& s, int o) { s.R.setH(s.SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_l(S& s, int o) { s.R.setL(s.SLA_X_X(o)); }

template <class T> void CPUCore<T>::sla_xiy_a(S& s, int o) { s.R.setA(s.SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_b(S& s, int o) { s.R.setB(s.SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_c(S& s, int o) { s.R.setC(s.SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_d(S& s, int o) { s.R.setD(s.SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_e(S& s, int o) { s.R.setE(s.SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_h(S& s, int o) { s.R.setH(s.SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_l(S& s, int o) { s.R.setL(s.SLA_X_Y(o)); }


// SLL r
template <class T> inline byte CPUCore<T>::SLL(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLL_X(unsigned x)
{
	byte res = SLL(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SLL_X_(unsigned x)
{
	memptr = x; return SLL_X(x);
}
template <class T> inline byte CPUCore<T>::SLL_X_X(int ofst)
{
	return SLL_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SLL_X_Y(int ofst)
{
	return SLL_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::sll_a(S& s) { s.R.setA(s.SLL(s.R.getA())); }
template <class T> void CPUCore<T>::sll_b(S& s) { s.R.setB(s.SLL(s.R.getB())); }
template <class T> void CPUCore<T>::sll_c(S& s) { s.R.setC(s.SLL(s.R.getC())); }
template <class T> void CPUCore<T>::sll_d(S& s) { s.R.setD(s.SLL(s.R.getD())); }
template <class T> void CPUCore<T>::sll_e(S& s) { s.R.setE(s.SLL(s.R.getE())); }
template <class T> void CPUCore<T>::sll_h(S& s) { s.R.setH(s.SLL(s.R.getH())); }
template <class T> void CPUCore<T>::sll_l(S& s) { s.R.setL(s.SLL(s.R.getL())); }

template <class T> void CPUCore<T>::sll_xhl(S& s) { s.SLL_X(s.R.getHL()); }
template <class T> void CPUCore<T>::sll_xix(S& s, int o)   { s.SLL_X_X(o); }
template <class T> void CPUCore<T>::sll_xiy(S& s, int o)   { s.SLL_X_Y(o); }

template <class T> void CPUCore<T>::sll_xix_a(S& s, int o) { s.R.setA(s.SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_b(S& s, int o) { s.R.setB(s.SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_c(S& s, int o) { s.R.setC(s.SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_d(S& s, int o) { s.R.setD(s.SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_e(S& s, int o) { s.R.setE(s.SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_h(S& s, int o) { s.R.setH(s.SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_l(S& s, int o) { s.R.setL(s.SLL_X_X(o)); }

template <class T> void CPUCore<T>::sll_xiy_a(S& s, int o) { s.R.setA(s.SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_b(S& s, int o) { s.R.setB(s.SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_c(S& s, int o) { s.R.setC(s.SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_d(S& s, int o) { s.R.setD(s.SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_e(S& s, int o) { s.R.setE(s.SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_h(S& s, int o) { s.R.setH(s.SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_l(S& s, int o) { s.R.setL(s.SLL_X_Y(o)); }


// SRA r
template <class T> inline byte CPUCore<T>::SRA(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (reg & 0x80);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRA_X(unsigned x)
{
	byte res = SRA(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SRA_X_(unsigned x)
{
	memptr = x; return SRA_X(x);
}
template <class T> inline byte CPUCore<T>::SRA_X_X(int ofst)
{
	return SRA_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SRA_X_Y(int ofst)
{
	return SRA_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::sra_a(S& s) { s.R.setA(s.SRA(s.R.getA())); }
template <class T> void CPUCore<T>::sra_b(S& s) { s.R.setB(s.SRA(s.R.getB())); }
template <class T> void CPUCore<T>::sra_c(S& s) { s.R.setC(s.SRA(s.R.getC())); }
template <class T> void CPUCore<T>::sra_d(S& s) { s.R.setD(s.SRA(s.R.getD())); }
template <class T> void CPUCore<T>::sra_e(S& s) { s.R.setE(s.SRA(s.R.getE())); }
template <class T> void CPUCore<T>::sra_h(S& s) { s.R.setH(s.SRA(s.R.getH())); }
template <class T> void CPUCore<T>::sra_l(S& s) { s.R.setL(s.SRA(s.R.getL())); }

template <class T> void CPUCore<T>::sra_xhl(S& s) { s.SRA_X(s.R.getHL()); }
template <class T> void CPUCore<T>::sra_xix(S& s, int o)   { s.SRA_X_X(o); }
template <class T> void CPUCore<T>::sra_xiy(S& s, int o)   { s.SRA_X_Y(o); }

template <class T> void CPUCore<T>::sra_xix_a(S& s, int o) { s.R.setA(s.SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_b(S& s, int o) { s.R.setB(s.SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_c(S& s, int o) { s.R.setC(s.SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_d(S& s, int o) { s.R.setD(s.SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_e(S& s, int o) { s.R.setE(s.SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_h(S& s, int o) { s.R.setH(s.SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_l(S& s, int o) { s.R.setL(s.SRA_X_X(o)); }

template <class T> void CPUCore<T>::sra_xiy_a(S& s, int o) { s.R.setA(s.SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_b(S& s, int o) { s.R.setB(s.SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_c(S& s, int o) { s.R.setC(s.SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_d(S& s, int o) { s.R.setD(s.SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_e(S& s, int o) { s.R.setE(s.SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_h(S& s, int o) { s.R.setH(s.SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_l(S& s, int o) { s.R.setL(s.SRA_X_Y(o)); }


// SRL R
template <class T> inline byte CPUCore<T>::SRL(byte reg)
{
	byte c = reg & 1;
	reg >>= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRL_X(unsigned x)
{
	byte res = SRL(RDMEM(x)); T::INC_DELAY();
	WRMEM(x, res);
	return res;
}
template <class T> inline byte CPUCore<T>::SRL_X_(unsigned x)
{
	memptr = x; return SRL_X(x);
}
template <class T> inline byte CPUCore<T>::SRL_X_X(int ofst)
{
	return SRL_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SRL_X_Y(int ofst)
{
	return SRL_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::srl_a(S& s) { s.R.setA(s.SRL(s.R.getA())); }
template <class T> void CPUCore<T>::srl_b(S& s) { s.R.setB(s.SRL(s.R.getB())); }
template <class T> void CPUCore<T>::srl_c(S& s) { s.R.setC(s.SRL(s.R.getC())); }
template <class T> void CPUCore<T>::srl_d(S& s) { s.R.setD(s.SRL(s.R.getD())); }
template <class T> void CPUCore<T>::srl_e(S& s) { s.R.setE(s.SRL(s.R.getE())); }
template <class T> void CPUCore<T>::srl_h(S& s) { s.R.setH(s.SRL(s.R.getH())); }
template <class T> void CPUCore<T>::srl_l(S& s) { s.R.setL(s.SRL(s.R.getL())); }

template <class T> void CPUCore<T>::srl_xhl(S& s) { s.SRL_X(s.R.getHL()); }
template <class T> void CPUCore<T>::srl_xix(S& s, int o)   { s.SRL_X_X(o); }
template <class T> void CPUCore<T>::srl_xiy(S& s, int o)   { s.SRL_X_Y(o); }

template <class T> void CPUCore<T>::srl_xix_a(S& s, int o) { s.R.setA(s.SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_b(S& s, int o) { s.R.setB(s.SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_c(S& s, int o) { s.R.setC(s.SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_d(S& s, int o) { s.R.setD(s.SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_e(S& s, int o) { s.R.setE(s.SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_h(S& s, int o) { s.R.setH(s.SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_l(S& s, int o) { s.R.setL(s.SRL_X_X(o)); }

template <class T> void CPUCore<T>::srl_xiy_a(S& s, int o) { s.R.setA(s.SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_b(S& s, int o) { s.R.setB(s.SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_c(S& s, int o) { s.R.setC(s.SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_d(S& s, int o) { s.R.setD(s.SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_e(S& s, int o) { s.R.setE(s.SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_h(S& s, int o) { s.R.setH(s.SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_l(S& s, int o) { s.R.setL(s.SRL_X_Y(o)); }


// RLA RLCA RRA RRCA
template <class T> void CPUCore<T>::rla(S& s)
{
	byte c = s.R.getF() & C_FLAG;
	s.R.setF((s.R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	         ((s.R.getA() & 0x80) ? C_FLAG : 0));
	s.R.setA((s.R.getA() << 1) | (c ? 1 : 0));
	s.R.setF(s.R.getF() | (s.R.getA() & (X_FLAG | Y_FLAG)));
}
template <class T> void CPUCore<T>::rlca(S& s)
{
	s.R.setA((s.R.getA() << 1) | (s.R.getA() >> 7));
	s.R.setF((s.R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	         (s.R.getA() & (Y_FLAG | X_FLAG | C_FLAG)));
}
template <class T> void CPUCore<T>::rra(S& s)
{
	byte c = (s.R.getF() & C_FLAG) << 7;
	s.R.setF((s.R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	         ((s.R.getA() & 0x01) ? C_FLAG : 0));
	s.R.setA((s.R.getA() >> 1) | c);
	s.R.setF(s.R.getF() | (s.R.getA() & (X_FLAG | Y_FLAG)));
}
template <class T> void CPUCore<T>::rrca(S& s)
{
	s.R.setF((s.R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	         (s.R.getA() &  C_FLAG));
	s.R.setA((s.R.getA() >> 1) | (s.R.getA() << 7));
	s.R.setF(s.R.getF() | (s.R.getA() & (X_FLAG | Y_FLAG)));
}


// RLD
template <class T> void CPUCore<T>::rld(S& s)
{
	byte val = s.RDMEM(s.R.getHL());
	s.memptr = s.R.getHL() + 1; // not 16-bit
	s.RLD_DELAY();
	s.WRMEM(s.R.getHL(), (val << 4) | (s.R.getA() & 0x0F));
	s.R.setA((s.R.getA() & 0xF0) | (val >> 4));
	s.R.setF((s.R.getF() & C_FLAG) | ZSPXYTable[s.R.getA()]);
}

// RRD
template <class T> void CPUCore<T>::rrd(S& s)
{
	byte val = s.RDMEM(s.R.getHL());
	s.memptr = s.R.getHL() + 1; // not 16-bit
	s.RLD_DELAY();
	s.WRMEM(s.R.getHL(), (val >> 4) | (s.R.getA() << 4));
	s.R.setA((s.R.getA() & 0xF0) | (val & 0x0F));
	s.R.setF((s.R.getF() & C_FLAG) | ZSPXYTable[s.R.getA()]);
}


// PUSH ss
template <class T> inline void CPUCore<T>::PUSH(unsigned reg)
{
	T::PUSH_DELAY();
	R.setSP(R.getSP() - 2);
	WR_WORD_rev(R.getSP(), reg);
}
template <class T> void CPUCore<T>::push_af(S& s) { s.PUSH(s.R.getAF()); }
template <class T> void CPUCore<T>::push_bc(S& s) { s.PUSH(s.R.getBC()); }
template <class T> void CPUCore<T>::push_de(S& s) { s.PUSH(s.R.getDE()); }
template <class T> void CPUCore<T>::push_hl(S& s) { s.PUSH(s.R.getHL()); }
template <class T> void CPUCore<T>::push_ix(S& s) { s.PUSH(s.R.getIX()); }
template <class T> void CPUCore<T>::push_iy(S& s) { s.PUSH(s.R.getIY()); }


// POP ss
template <class T> inline unsigned CPUCore<T>::POP()
{
	unsigned addr = R.getSP();
	R.setSP(addr + 2);
	return RD_WORD(addr);
}
template <class T> void CPUCore<T>::pop_af(S& s) { s.R.setAF(s.POP()); }
template <class T> void CPUCore<T>::pop_bc(S& s) { s.R.setBC(s.POP()); }
template <class T> void CPUCore<T>::pop_de(S& s) { s.R.setDE(s.POP()); }
template <class T> void CPUCore<T>::pop_hl(S& s) { s.R.setHL(s.POP()); }
template <class T> void CPUCore<T>::pop_ix(S& s) { s.R.setIX(s.POP()); }
template <class T> void CPUCore<T>::pop_iy(S& s) { s.R.setIY(s.POP()); }


// CALL nn / CALL cc,nn
template <class T> inline void CPUCore<T>::CALL()
{
	memptr = RD_WORD_PC();
	PUSH(R.getPC());
	R.setPC(memptr);
}
template <class T> inline void CPUCore<T>::SKIP_JP()
{
	memptr = RD_WORD_PC();
}
template <class T> void CPUCore<T>::call(S& s)    { s.CALL(); }
template <class T> void CPUCore<T>::call_c(S& s)  { if (s.C())  s.CALL(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::call_m(S& s)  { if (s.M())  s.CALL(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::call_nc(S& s) { if (s.NC()) s.CALL(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::call_nz(S& s) { if (s.NZ()) s.CALL(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::call_p(S& s)  { if (s.P())  s.CALL(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::call_pe(S& s) { if (s.PE()) s.CALL(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::call_po(S& s) { if (s.PO()) s.CALL(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::call_z(S& s)  { if (s.Z())  s.CALL(); else s.SKIP_JP(); }


// RST n
template <class T> inline void CPUCore<T>::RST(unsigned x)
{
	PUSH(R.getPC());
	memptr = x;
	R.setPC(x);
}
template <class T> void CPUCore<T>::rst_00(S& s) { s.RST(0x00); }
template <class T> void CPUCore<T>::rst_08(S& s) { s.RST(0x08); }
template <class T> void CPUCore<T>::rst_10(S& s) { s.RST(0x10); }
template <class T> void CPUCore<T>::rst_18(S& s) { s.RST(0x18); }
template <class T> void CPUCore<T>::rst_20(S& s) { s.RST(0x20); }
template <class T> void CPUCore<T>::rst_28(S& s) { s.RST(0x28); }
template <class T> void CPUCore<T>::rst_30(S& s) { s.RST(0x30); }
template <class T> void CPUCore<T>::rst_38(S& s) { s.RST(0x38); }


// RET
template <class T> inline void CPUCore<T>::RET()
{
	memptr = POP();
	R.setPC(memptr);
}
template <class T> void CPUCore<T>::ret(S& s)    { s.RET(); }
template <class T> void CPUCore<T>::ret_c(S& s)  { s.SMALL_DELAY(); if (s.C())  s.RET(); }
template <class T> void CPUCore<T>::ret_m(S& s)  { s.SMALL_DELAY(); if (s.M())  s.RET(); }
template <class T> void CPUCore<T>::ret_nc(S& s) { s.SMALL_DELAY(); if (s.NC()) s.RET(); }
template <class T> void CPUCore<T>::ret_nz(S& s) { s.SMALL_DELAY(); if (s.NZ()) s.RET(); }
template <class T> void CPUCore<T>::ret_p(S& s)  { s.SMALL_DELAY(); if (s.P())  s.RET(); }
template <class T> void CPUCore<T>::ret_pe(S& s) { s.SMALL_DELAY(); if (s.PE()) s.RET(); }
template <class T> void CPUCore<T>::ret_po(S& s) { s.SMALL_DELAY(); if (s.PO()) s.RET(); }
template <class T> void CPUCore<T>::ret_z(S& s)  { s.SMALL_DELAY(); if (s.Z())  s.RET(); }

template <class T> void CPUCore<T>::reti(S& s)
{
	// same as retn
	s.RETN_DELAY();
	s.R.setIFF1(s.R.getIFF2());
	s.R.setNextIFF1(s.R.getIFF2());
	s.setSlowInstructions();
	s.RET();
}
template <class T> void CPUCore<T>::retn(S& s)
{
	s.RETN_DELAY();
	s.R.setIFF1(s.R.getIFF2());
	s.R.setNextIFF1(s.R.getIFF2());
	s.setSlowInstructions();
	s.RET();
}


// JP ss
template <class T> void CPUCore<T>::jp_hl(S& s) { s.R.setPC(s.R.getHL()); }
template <class T> void CPUCore<T>::jp_ix(S& s) { s.R.setPC(s.R.getIX()); }
template <class T> void CPUCore<T>::jp_iy(S& s) { s.R.setPC(s.R.getIY()); }

// JP nn / JP cc,nn
template <class T> inline void CPUCore<T>::JP()
{
	memptr = RD_WORD_PC();
	R.setPC(memptr);
}
template <class T> void CPUCore<T>::jp(S& s)    { s.JP(); }
template <class T> void CPUCore<T>::jp_c(S& s)  { if (s.C())  s.JP(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::jp_m(S& s)  { if (s.M())  s.JP(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::jp_nc(S& s) { if (s.NC()) s.JP(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::jp_nz(S& s) { if (s.NZ()) s.JP(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::jp_p(S& s)  { if (s.P())  s.JP(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::jp_pe(S& s) { if (s.PE()) s.JP(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::jp_po(S& s) { if (s.PO()) s.JP(); else s.SKIP_JP(); }
template <class T> void CPUCore<T>::jp_z(S& s)  { if (s.Z())  s.JP(); else s.SKIP_JP(); }

// JR e
template <class T> inline void CPUCore<T>::JR()
{
	offset ofst = RDMEM_OPCODE();
	R.setPC((R.getPC() + ofst) & 0xFFFF);
	memptr = R.getPC();
	T::ADD_16_8_DELAY();
}
template <class T> inline void CPUCore<T>::SKIP_JR()
{
	RDMEM_OPCODE(); // ignore return value
}
template <class T> void CPUCore<T>::jr(S& s)    { s.JR(); }
template <class T> void CPUCore<T>::jr_c(S& s)  { if (s.C())  s.JR(); else s.SKIP_JR(); }
template <class T> void CPUCore<T>::jr_nc(S& s) { if (s.NC()) s.JR(); else s.SKIP_JR(); }
template <class T> void CPUCore<T>::jr_nz(S& s) { if (s.NZ()) s.JR(); else s.SKIP_JR(); }
template <class T> void CPUCore<T>::jr_z(S& s)  { if (s.Z())  s.JR(); else s.SKIP_JR(); }

// DJNZ e
template <class T> void CPUCore<T>::djnz(S& s)
{
	s.SMALL_DELAY();
	byte b = s.R.getB();
	s.R.setB(--b);
	if (b) s.JR(); else s.SKIP_JR();
}

// EX (SP),ss
template <class T> inline unsigned CPUCore<T>::EX_SP(unsigned reg)
{
	memptr = RD_WORD(R.getSP());
	T::SMALL_DELAY();
	WR_WORD_rev(R.getSP(), reg);
	T::EX_SP_HL_DELAY();
	return memptr;
}
template <class T> void CPUCore<T>::ex_xsp_hl(S& s) { s.R.setHL(s.EX_SP(s.R.getHL())); }
template <class T> void CPUCore<T>::ex_xsp_ix(S& s) { s.R.setIX(s.EX_SP(s.R.getIX())); }
template <class T> void CPUCore<T>::ex_xsp_iy(S& s) { s.R.setIY(s.EX_SP(s.R.getIY())); }


// IN r,(c)
template <class T> inline byte CPUCore<T>::IN()
{
	byte res = READ_PORT(R.getBC());
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[res]);
	return res;
}
template <class T> void CPUCore<T>::in_a_c(S& s) { s.R.setA(s.IN()); }
template <class T> void CPUCore<T>::in_b_c(S& s) { s.R.setB(s.IN()); }
template <class T> void CPUCore<T>::in_c_c(S& s) { s.R.setC(s.IN()); }
template <class T> void CPUCore<T>::in_d_c(S& s) { s.R.setD(s.IN()); }
template <class T> void CPUCore<T>::in_e_c(S& s) { s.R.setE(s.IN()); }
template <class T> void CPUCore<T>::in_h_c(S& s) { s.R.setH(s.IN()); }
template <class T> void CPUCore<T>::in_l_c(S& s) { s.R.setL(s.IN()); }
template <class T> void CPUCore<T>::in_0_c(S& s) { s.IN(); } // discard result

// IN a,(n)
template <class T> void CPUCore<T>::in_a_byte(S& s)
{
	unsigned y = s.RDMEM_OPCODE() + 256 * s.R.getA();
	s.R.setA(s.READ_PORT(y));
}


// OUT (c),r
template <class T> inline void CPUCore<T>::OUT(byte val)
{
	WRITE_PORT(R.getBC(), val);
}
template <class T> void CPUCore<T>::out_c_a(S& s)   { s.OUT(s.R.getA()); }
template <class T> void CPUCore<T>::out_c_b(S& s)   { s.OUT(s.R.getB()); }
template <class T> void CPUCore<T>::out_c_c(S& s)   { s.OUT(s.R.getC()); }
template <class T> void CPUCore<T>::out_c_d(S& s)   { s.OUT(s.R.getD()); }
template <class T> void CPUCore<T>::out_c_e(S& s)   { s.OUT(s.R.getE()); }
template <class T> void CPUCore<T>::out_c_h(S& s)   { s.OUT(s.R.getH()); }
template <class T> void CPUCore<T>::out_c_l(S& s)   { s.OUT(s.R.getL()); }
template <class T> void CPUCore<T>::out_c_0(S& s)   { s.OUT(0);        }

// OUT (n),a
template <class T> void CPUCore<T>::out_byte_a(S& s)
{
	unsigned y = s.RDMEM_OPCODE() + 256 * s.R.getA();
	s.WRITE_PORT(y, s.R.getA());
}


// block CP
template <class T> inline void CPUCore<T>::BLOCK_CP(int increase, bool repeat)
{
	byte val = RDMEM(R.getHL());
	T::BLOCK_DELAY();
	byte res = R.getA() - val;
	R.setHL(R.getHL() + increase);
	R.setBC(R.getBC() - 1);
	byte f = (R.getF() & C_FLAG) |
	         ((R.getA() ^ val ^ res) & H_FLAG) |
	         ZSTable[res] |
	         N_FLAG;
	res -= ((f & H_FLAG) >> 4);
	if (res & 0x02) f |= Y_FLAG; // bit 1 -> flag 5
	if (res & 0x08) f |= X_FLAG; // bit 3 -> flag 3
	if (R.getBC())  f |= V_FLAG;
	R.setF(f);
	if (repeat && R.getBC() && !(f & Z_FLAG)) {
		T::BLOCK_DELAY();
		R.setPC(R.getPC() - 2);
	}
}
template <class T> void CPUCore<T>::cpd(S& s)  { s.BLOCK_CP(-1, false); }
template <class T> void CPUCore<T>::cpi(S& s)  { s.BLOCK_CP( 1, false); }
template <class T> void CPUCore<T>::cpdr(S& s) { s.BLOCK_CP(-1, true ); }
template <class T> void CPUCore<T>::cpir(S& s) { s.BLOCK_CP( 1, true ); }


// block LD
template <class T> inline void CPUCore<T>::BLOCK_LD(int increase, bool repeat)
{
	byte val = RDMEM(R.getHL());
	WRMEM(R.getDE(), val);
	T::LDI_DELAY();
	R.setHL(R.getHL() + increase);
	R.setDE(R.getDE() + increase);
	R.setBC(R.getBC() - 1);
	byte f = R.getF() & (S_FLAG | Z_FLAG | C_FLAG);
	if ((R.getA() + val) & 0x02) f |= Y_FLAG;	// bit 1 -> flag 5
	if ((R.getA() + val) & 0x08) f |= X_FLAG;	// bit 3 -> flag 3
	if (R.getBC())               f |= V_FLAG;
	R.setF(f);
	if (repeat && R.getBC()) {
		T::BLOCK_DELAY();
		R.setPC(R.getPC() - 2);
	}
}
template <class T> void CPUCore<T>::ldd(S& s)  { s.BLOCK_LD(-1, false); }
template <class T> void CPUCore<T>::ldi(S& s)  { s.BLOCK_LD( 1, false); }
template <class T> void CPUCore<T>::lddr(S& s) { s.BLOCK_LD(-1, true ); }
template <class T> void CPUCore<T>::ldir(S& s) { s.BLOCK_LD( 1, true ); }


// block IN
template <class T> inline void CPUCore<T>::BLOCK_IN(int increase, bool repeat)
{
	T::SMALL_DELAY();
	byte b = R.getB() - 1; R.setB(b); // decr before use
	byte val = READ_PORT(R.getBC());
	WRMEM(R.getHL(), val);
	R.setHL(R.getHL() + increase);
	byte f = ZSTable[b];
	if (val & S_FLAG) f |= N_FLAG;
	unsigned k = val + ((R.getC() + increase) & 0xFF);
	if (k & 0x100)    f |= H_FLAG | C_FLAG;
	R.setF(f | (ZSPXYTable[(k & 0x07) ^ b] & P_FLAG));
	if (repeat && b) {
		T::BLOCK_DELAY();
		R.setPC(R.getPC() - 2);
	}
}
template <class T> void CPUCore<T>::ind(S& s)  { s.BLOCK_IN(-1, false); }
template <class T> void CPUCore<T>::ini(S& s)  { s.BLOCK_IN( 1, false); }
template <class T> void CPUCore<T>::indr(S& s) { s.BLOCK_IN(-1, true ); }
template <class T> void CPUCore<T>::inir(S& s) { s.BLOCK_IN( 1, true ); }


// block OUT
template <class T> inline void CPUCore<T>::BLOCK_OUT(int increase, bool repeat)
{
	T::SMALL_DELAY();
	byte val = RDMEM(R.getHL());
	WRITE_PORT(R.getBC(), val);
	byte b = R.getB() - 1; R.setB(b); // decr after use
	R.setHL(R.getHL() + increase);
	byte f = ZSXYTable[b];
	if (val & S_FLAG) f |= N_FLAG;
	unsigned k = val + R.getL();
	if (k & 0x100)    f |= H_FLAG | C_FLAG;
	R.setF(f | (ZSPXYTable[(k & 0x07) ^ b] & P_FLAG));
	if (repeat && b) {
		T::BLOCK_DELAY();
		R.setPC(R.getPC() - 2);
	}
}
template <class T> void CPUCore<T>::outd(S& s) { s.BLOCK_OUT(-1, false); }
template <class T> void CPUCore<T>::outi(S& s) { s.BLOCK_OUT( 1, false); }
template <class T> void CPUCore<T>::otdr(S& s) { s.BLOCK_OUT(-1, true ); }
template <class T> void CPUCore<T>::otir(S& s) { s.BLOCK_OUT( 1, true ); }


// various
template <class T> void CPUCore<T>::nop(S&) { }
template <class T> void CPUCore<T>::ccf(S& s)
{
	s.R.setF(((s.R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	          ((s.R.getF() & C_FLAG) << 4) | // H_FLAG
	          (s.R.getA() & (X_FLAG | Y_FLAG))                  ) ^ C_FLAG);
}
template <class T> void CPUCore<T>::cpl(S& s)
{
	s.R.setA(s.R.getA() ^ 0xFF);
	s.R.setF((s.R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	         H_FLAG | N_FLAG |
	         (s.R.getA() & (X_FLAG | Y_FLAG)));

}
template <class T> void CPUCore<T>::daa(S& s)
{
	unsigned i = s.R.getA();
	i |= (s.R.getF() & (C_FLAG | N_FLAG)) << 8; // 0x100 0x200
	i |= (s.R.getF() &  H_FLAG)           << 6; // 0x400
	s.R.setAF(DAATable[i]);
}
template <class T> void CPUCore<T>::neg(S& s)
{
	 byte i = s.R.getA();
	 s.R.setA(0);
	 s.SUB(i);
}
template <class T> void CPUCore<T>::scf(S& s)
{
	s.R.setF((s.R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	         C_FLAG |
	         (s.R.getA() & (X_FLAG | Y_FLAG)));
}

template <class T> void CPUCore<T>::ex_af_af(S& s)
{
	unsigned t = s.R.getAF2(); s.R.setAF2(s.R.getAF()); s.R.setAF(t);
}
template <class T> void CPUCore<T>::ex_de_hl(S& s)
{
	unsigned t = s.R.getDE(); s.R.setDE(s.R.getHL()); s.R.setHL(t);
}
template <class T> void CPUCore<T>::exx(S& s)
{
	unsigned t1 = s.R.getBC2(); s.R.setBC2(s.R.getBC()); s.R.setBC(t1);
	unsigned t2 = s.R.getDE2(); s.R.setDE2(s.R.getDE()); s.R.setDE(t2);
	unsigned t3 = s.R.getHL2(); s.R.setHL2(s.R.getHL()); s.R.setHL(t3);
}

template <class T> void CPUCore<T>::di(S& s)
{
	s.DI_DELAY();
	s.R.di();
}
template <class T> void CPUCore<T>::ei(S& s)
{
	s.R.setIFF1(false);	// no ints after this instruction
	s.R.setNextIFF1(true);	// but allow them after next instruction
	s.R.setIFF2(true);
	s.setSlowInstructions();
}
template <class T> void CPUCore<T>::halt(S& s)
{
	s.R.setHALT(true);
	s.setSlowInstructions();

	if (!(s.R.getIFF1() || s.R.getNextIFF1() || s.R.getIFF2())) {
		s.motherboard.getMSXCliComm().printWarning(
			"DI; HALT detected, which means a hang. "
			"You can just as well reset the MSX now...\n");
	}
}
template <class T> void CPUCore<T>::im_0(S& s) { s.SET_IM_DELAY(); s.R.setIM(0); }
template <class T> void CPUCore<T>::im_1(S& s) { s.SET_IM_DELAY(); s.R.setIM(1); }
template <class T> void CPUCore<T>::im_2(S& s) { s.SET_IM_DELAY(); s.R.setIM(2); }

// LD A,I/R
template <class T> void CPUCore<T>::ld_a_i(S& s)
{
	s.SMALL_DELAY();
	s.R.setA(s.R.getI());
	s.R.setF((s.R.getF() & C_FLAG) |
	         ZSXYTable[s.R.getA()] |
	         (s.R.getIFF2() ? V_FLAG : 0));
}
template <class T> void CPUCore<T>::ld_a_r(S& s)
{
	s.SMALL_DELAY();
	s.R.setA(s.R.getR());
	s.R.setF((s.R.getF() & C_FLAG) |
	         ZSXYTable[s.R.getA()] |
	         (s.R.getIFF2() ? V_FLAG : 0));
}

// LD I/R,A
template <class T> void CPUCore<T>::ld_i_a(S& s)
{
	s.SMALL_DELAY();
	s.R.setI(s.R.getA());
}
template <class T> void CPUCore<T>::ld_r_a(S& s)
{
	s.SMALL_DELAY();
	s.R.setR(s.R.getA());
}

// MULUB A,r
template <class T> inline void CPUCore<T>::MULUB(byte reg)
{
	// TODO check flags
	T::MULUB_DELAY();
	R.setHL(unsigned(R.getA()) * reg);
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (R.getHL() ? 0 : Z_FLAG) |
	       ((R.getHL() & 0x8000) ? C_FLAG : 0));
}
//template <class T> void CPUCore<T>::mulub_a_xhl(S&)   { } // TODO
//template <class T> void CPUCore<T>::mulub_a_a(S&)     { } // TODO
template <class T> void CPUCore<T>::mulub_a_b(S& s)   { s.MULUB(s.R.getB()); }
template <class T> void CPUCore<T>::mulub_a_c(S& s)   { s.MULUB(s.R.getC()); }
template <class T> void CPUCore<T>::mulub_a_d(S& s)   { s.MULUB(s.R.getD()); }
template <class T> void CPUCore<T>::mulub_a_e(S& s)   { s.MULUB(s.R.getE()); }
//template <class T> void CPUCore<T>::mulub_a_h(S&)     { } // TODO
//template <class T> void CPUCore<T>::mulub_a_l(S&)     { } // TODO

// MULUW HL,ss
template <class T> inline void CPUCore<T>::MULUW(unsigned reg)
{
	// TODO check flags
	T::MULUW_DELAY();
	unsigned res = unsigned(R.getHL()) * reg;
	R.setDE(res >> 16);
	R.setHL(res & 0xffff);
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (res ? 0 : Z_FLAG) |
	       ((res & 0x80000000) ? C_FLAG : 0));
}
template <class T> void CPUCore<T>::muluw_hl_bc(S& s) { s.MULUW(s.R.getBC()); }
//template <class T> void CPUCore<T>::muluw_hl_de(S&)   { } // TODO
//template <class T> void CPUCore<T>::muluw_hl_hl(S&)   { } // TODO
template <class T> void CPUCore<T>::muluw_hl_sp(S& s) { s.MULUW(s.R.getSP()); }


// prefixes
template <class T> void CPUCore<T>::dd_cb(S& s)
{
	unsigned tmp = s.RD_WORD_PC();
	offset ofst = tmp & 0xFF;
	unsigned opcode = tmp >> 8;
	s.DD_CB_DELAY();
	(*opcode_dd_cb[opcode])(s, ofst);
}

template <class T> void CPUCore<T>::fd_cb(S& s)
{
	unsigned tmp = s.RD_WORD_PC();
	offset ofst = tmp & 0xFF;
	unsigned opcode = tmp >> 8;
	s.DD_CB_DELAY();
	(*opcode_fd_cb[opcode])(s, ofst);
}

template <class T> void CPUCore<T>::cb(S& s)
{
	byte opcode = s.RDMEM_OPCODE();
	s.M1Cycle();
	(*opcode_cb[opcode])(s);
}

template <class T> void CPUCore<T>::ed(S& s)
{
	byte opcode = s.RDMEM_OPCODE();
	s.M1Cycle();
	(*opcode_ed[opcode])(s);
}

template <class T> void CPUCore<T>::dd(S& s)
{
	byte opcode = s.RDMEM_OPCODE();
	s.M1Cycle();
	(*opcode_dd[opcode])(s);
}

template <class T> void CPUCore<T>::fd(S& s)
{
	byte opcode = s.RDMEM_OPCODE();
	s.M1Cycle();
	(*opcode_fd[opcode])(s);
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

} // namespace openmsx

