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
	R.setExtHALT(false);
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

template <class T> void CPUCore<T>::waitCycles(unsigned cycles)
{
	T::add(cycles);
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


template <class T> inline byte CPUCore<T>::READ_PORT(unsigned port, unsigned cc)
{
	memptr = port + 1; // not 16-bit
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	byte result = interface->readIO(port, time);
	// note: no forced page-break after IO
	return result;
}

template <class T> inline void CPUCore<T>::WRITE_PORT(unsigned port, byte value, unsigned cc)
{
	memptr = port + 1; // not 16-bit
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	interface->writeIO(port, value, time);
	// note: no forced page-break after IO
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

template <class T> template <bool PRE_PB, bool POST_PB>
NEVER_INLINE byte CPUCore<T>::RDMEMslow(unsigned address, unsigned cc)
{
	// not cached
	unsigned high = address >> CacheLine::BITS;
	if (!readCacheTried[high]) {
		// try to cache now
		unsigned addrBase = address & CacheLine::HIGH;
		if (const byte* line = interface->getReadCacheLine(addrBase)) {
			// cached ok
			T::template PRE_MEM<PRE_PB, POST_PB>(address);
			T::template POST_MEM<       POST_PB>(address);
			readCacheLine[high] = line - addrBase;
			return readCacheLine[high][address];
		}
	}
	// uncacheable
	readCacheTried[high] = true;
	T::template PRE_MEM<PRE_PB, POST_PB>(address);
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	byte result = interface->readMem(address, time);
	T::template POST_MEM<POST_PB>(address);
	return result;
}
template <class T> template <bool PRE_PB, bool POST_PB>
ALWAYS_INLINE byte CPUCore<T>::RDMEM_impl(unsigned address, unsigned cc)
{
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(line != NULL)) {
		// cached, fast path
		T::template PRE_MEM<PRE_PB, POST_PB>(address);
		T::template POST_MEM<       POST_PB>(address);
		return line[address];
	} else {
		return RDMEMslow<PRE_PB, POST_PB>(address, cc); // not inlined
	}
}
template <class T> ALWAYS_INLINE byte CPUCore<T>::RDMEM_OPCODE(unsigned cc)
{
	unsigned address = R.getPC();
	R.setPC(address + 1);
	return RDMEM_impl<false, false>(address, cc);
}
template <class T> ALWAYS_INLINE byte CPUCore<T>::RDMEM(unsigned address, unsigned cc)
{
	return RDMEM_impl<true, true>(address, cc);
}

template <class T> template <bool PRE_PB, bool POST_PB>
NEVER_INLINE unsigned CPUCore<T>::RD_WORD_slow(unsigned address, unsigned cc)
{
	unsigned res = RDMEM_impl<PRE_PB,  false>(address, cc);
	res         += RDMEM_impl<false, POST_PB>((address + 1) & 0xFFFF, cc + T::CC_RDMEM) << 8;
	return res;
}
template <class T> template <bool PRE_PB, bool POST_PB>
ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD_impl(unsigned address, unsigned cc)
{
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) &&
		   (line != NULL))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<PRE_PB, POST_PB>(address);
		T::template POST_WORD<       POST_PB>(address);
		return read16LE(&line[address]);
	} else {
		// slow path, not inline
		return RD_WORD_slow<PRE_PB, POST_PB>(address, cc);
	}
}
template <class T> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD_PC(unsigned cc)
{
	unsigned addr = R.getPC();
	R.setPC(addr + 2);
	return RD_WORD_impl<false, false>(addr, cc);
}
template <class T> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD(
	unsigned address, unsigned cc)
{
	return RD_WORD_impl<true, true>(address, cc);
}

template <class T> template <bool PRE_PB, bool POST_PB>
NEVER_INLINE void CPUCore<T>::WRMEMslow(unsigned address, byte value, unsigned cc)
{
	// not cached
	unsigned high = address >> CacheLine::BITS;
	if (!writeCacheTried[high]) {
		// try to cache now
		unsigned addrBase = address & CacheLine::HIGH;
		if (byte* line = interface->getWriteCacheLine(addrBase)) {
			// cached ok
			T::template PRE_MEM<PRE_PB, POST_PB>(address);
			T::template POST_MEM<       POST_PB>(address);
			writeCacheLine[high] = line - addrBase;
			writeCacheLine[high][address] = value;
			return;
		}
	}
	// uncacheable
	writeCacheTried[high] = true;
	T::template PRE_MEM<PRE_PB, POST_PB>(address);
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	interface->writeMem(address, value, time);
	T::template POST_MEM<POST_PB>(address);
}
template <class T> template <bool PRE_PB, bool POST_PB>
ALWAYS_INLINE void CPUCore<T>::WRMEM_impl(
	unsigned address, byte value, unsigned cc)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(line != NULL)) {
		// cached, fast path
		T::template PRE_MEM<PRE_PB, POST_PB>(address);
		T::template POST_MEM<       POST_PB>(address);
		line[address] = value;
	} else {
		WRMEMslow<PRE_PB, POST_PB>(address, value, cc);	// not inlined
	}
}
template <class T> ALWAYS_INLINE void CPUCore<T>::WRMEM(
	unsigned address, byte value, unsigned cc)
{
	WRMEM_impl<true, true>(address, value, cc);
}

template <class T> NEVER_INLINE void CPUCore<T>::WR_WORD_slow(
	unsigned address, unsigned value, unsigned cc)
{
	WRMEM_impl<true, false>( address,               value & 255, cc);
	WRMEM_impl<false, true>((address + 1) & 0xFFFF, value >> 8,  cc + T::CC_WRMEM);
}
template <class T> ALWAYS_INLINE void CPUCore<T>::WR_WORD(
	unsigned address, unsigned value, unsigned cc)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) &&
		   (line != NULL))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<true, true>(address);
		T::template POST_WORD<     true>(address);
		write16LE(&line[address], value);
	} else {
		// slow path, not inline
		WR_WORD_slow(address, value, cc);
	}
}

// same as WR_WORD, but writes high byte first
template <class T> template <bool PRE_PB, bool POST_PB>
NEVER_INLINE void CPUCore<T>::WR_WORD_rev_slow(
	unsigned address, unsigned value, unsigned cc)
{
	WRMEM_impl<PRE_PB,  false>((address + 1) & 0xFFFF, value >> 8,  cc);
	WRMEM_impl<false, POST_PB>( address,               value & 255, cc + T::CC_WRMEM);
}
template <class T> template <bool PRE_PB, bool POST_PB>
ALWAYS_INLINE void CPUCore<T>::WR_WORD_rev(
	unsigned address, unsigned value, unsigned cc)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) &&
		   (line != NULL))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<PRE_PB, POST_PB>(address);
		T::template POST_WORD<       POST_PB>(address);
		write16LE(&line[address], value);
	} else {
		// slow path, not inline
		WR_WORD_rev_slow<PRE_PB, POST_PB>(address, value, cc);
	}
}


template <class T> inline void CPUCore<T>::M1Cycle()
{
	R.incR(1);
}

// NMI interrupt
template <class T> inline void CPUCore<T>::nmi()
{
	M1Cycle();
	R.setHALT(false);
	R.setIFF1(false);
	R.setNextIFF1(false);
	PUSH(R.getPC(), T::EE_NMI_1);
	R.setPC(0x0066);
	T::add(T::CC_NMI);
}

// IM0 interrupt
template <class T> inline void CPUCore<T>::irq0()
{
	// TODO current implementation only works for 1-byte instructions
	//      ok for MSX
	assert(interface->readIRQVector() == 0xFF);
	// Note: On real MSX HW there is no extra wait cycle (as introduced
	//       by the M1Cycle() call). To compensate for this IM0_DELAY()
	//       waits one cycle less.
	M1Cycle();
	R.setHALT(false);
	R.di();
	PUSH(R.getPC(), T::EE_IRQ0_1);
	R.setPC(0x0038);
	T::add(T::CC_IRQ0);
}

// IM1 interrupt
template <class T> inline void CPUCore<T>::irq1()
{
	M1Cycle(); // see note in irq0()
	R.setHALT(false);
	R.di();
	PUSH(R.getPC(), T::EE_IRQ1_1);
	R.setPC(0x0038);
	T::add(T::CC_IRQ1);
}

// IM2 interrupt
template <class T> inline void CPUCore<T>::irq2()
{
	M1Cycle(); // see note in irq0()
	R.setHALT(false);
	R.di();
	PUSH(R.getPC(), T::EE_IRQ2_1);
	unsigned x = interface->readIRQVector() | (R.getI() << 8);
	R.setPC(RD_WORD(x, T::CC_IRQ2_2));
	T::add(T::CC_IRQ2);
}

template <class T> NEVER_INLINE int CPUCore<T>::executeInstruction1_slow(byte opcode)
{
	return executeInstruction1(opcode);
}

template <class T> ALWAYS_INLINE int CPUCore<T>::executeInstruction1(byte opcode)
{
	M1Cycle();
	switch (opcode) {
		case 0x00: // nop
		case 0x40: // ld b,b
		case 0x49: // ld c,c
		case 0x52: // ld d,d
		case 0x5b: // ld e,e
		case 0x64: // ld h,h
		case 0x6d: // ld l,l
		case 0x7f: // ld a,a
			return nop();
		case 0x07: return rlca();
		case 0x0f: return rrca();
		case 0x17: return rla();
		case 0x1f: return rra();
		case 0x08: return ex_af_af();
		case 0x27: return daa();
		case 0x2f: return cpl();
		case 0x37: return scf();
		case 0x3f: return ccf();
		case 0x10: return djnz();
		case 0x18: return jr();
		case 0x20: return jr_nz();
		case 0x28: return jr_z();
		case 0x30: return jr_nc();
		case 0x38: return jr_c();
		case 0x32: return ld_xbyte_a();
		case 0x3a: return ld_a_xbyte();
		case 0x22: return ld_xword_SS<HL>();
		case 0x2a: return ld_SS_xword<HL>();
		case 0x02: return ld_SS_a<BC>();
		case 0x12: return ld_SS_a<DE>();
		case 0x1a: return ld_a_SS<DE>();
		case 0x0a: return ld_a_SS<BC>();
		case 0x03: return inc_SS<BC>();
		case 0x13: return inc_SS<DE>();
		case 0x23: return inc_SS<HL>();
		case 0x33: return inc_SS<SP>();
		case 0x0b: return dec_SS<BC>();
		case 0x1b: return dec_SS<DE>();
		case 0x2b: return dec_SS<HL>();
		case 0x3b: return dec_SS<SP>();
		case 0x09: return add_SS_TT<HL,BC>();
		case 0x19: return add_SS_TT<HL,DE>();
		case 0x29: return add_SS_SS<HL>();
		case 0x39: return add_SS_TT<HL,SP>();
		case 0x01: return ld_SS_word<BC>();
		case 0x11: return ld_SS_word<DE>();
		case 0x21: return ld_SS_word<HL>();
		case 0x31: return ld_SS_word<SP>();
		case 0x04: return inc_R<B>();
		case 0x0c: return inc_R<C>();
		case 0x14: return inc_R<D>();
		case 0x1c: return inc_R<E>();
		case 0x24: return inc_R<H>();
		case 0x2c: return inc_R<L>();
		case 0x3c: return inc_R<A>();
		case 0x34: return inc_xhl();
		case 0x05: return dec_R<B>();
		case 0x0d: return dec_R<C>();
		case 0x15: return dec_R<D>();
		case 0x1d: return dec_R<E>();
		case 0x25: return dec_R<H>();
		case 0x2d: return dec_R<L>();
		case 0x3d: return dec_R<A>();
		case 0x35: return dec_xhl();
		case 0x06: return ld_R_byte<B>();
		case 0x0e: return ld_R_byte<C>();
		case 0x16: return ld_R_byte<D>();
		case 0x1e: return ld_R_byte<E>();
		case 0x26: return ld_R_byte<H>();
		case 0x2e: return ld_R_byte<L>();
		case 0x3e: return ld_R_byte<A>();
		case 0x36: return ld_xhl_byte();

		case 0x41: return ld_R_R<B,C>();
		case 0x42: return ld_R_R<B,D>();
		case 0x43: return ld_R_R<B,E>();
		case 0x44: return ld_R_R<B,H>();
		case 0x45: return ld_R_R<B,L>();
		case 0x47: return ld_R_R<B,A>();
		case 0x48: return ld_R_R<C,B>();
		case 0x4a: return ld_R_R<C,D>();
		case 0x4b: return ld_R_R<C,E>();
		case 0x4c: return ld_R_R<C,H>();
		case 0x4d: return ld_R_R<C,L>();
		case 0x4f: return ld_R_R<C,A>();
		case 0x50: return ld_R_R<D,B>();
		case 0x51: return ld_R_R<D,C>();
		case 0x53: return ld_R_R<D,E>();
		case 0x54: return ld_R_R<D,H>();
		case 0x55: return ld_R_R<D,L>();
		case 0x57: return ld_R_R<D,A>();
		case 0x58: return ld_R_R<E,B>();
		case 0x59: return ld_R_R<E,C>();
		case 0x5a: return ld_R_R<E,D>();
		case 0x5c: return ld_R_R<E,H>();
		case 0x5d: return ld_R_R<E,L>();
		case 0x5f: return ld_R_R<E,A>();
		case 0x60: return ld_R_R<H,B>();
		case 0x61: return ld_R_R<H,C>();
		case 0x62: return ld_R_R<H,D>();
		case 0x63: return ld_R_R<H,E>();
		case 0x65: return ld_R_R<H,L>();
		case 0x67: return ld_R_R<H,A>();
		case 0x68: return ld_R_R<L,B>();
		case 0x69: return ld_R_R<L,C>();
		case 0x6a: return ld_R_R<L,D>();
		case 0x6b: return ld_R_R<L,E>();
		case 0x6c: return ld_R_R<L,H>();
		case 0x6f: return ld_R_R<L,A>();
		case 0x78: return ld_R_R<A,B>();
		case 0x79: return ld_R_R<A,C>();
		case 0x7a: return ld_R_R<A,D>();
		case 0x7b: return ld_R_R<A,E>();
		case 0x7c: return ld_R_R<A,H>();
		case 0x7d: return ld_R_R<A,L>();
		case 0x70: return ld_xhl_R<B>();
		case 0x71: return ld_xhl_R<C>();
		case 0x72: return ld_xhl_R<D>();
		case 0x73: return ld_xhl_R<E>();
		case 0x74: return ld_xhl_R<H>();
		case 0x75: return ld_xhl_R<L>();
		case 0x77: return ld_xhl_R<A>();
		case 0x46: return ld_R_xhl<B>();
		case 0x4e: return ld_R_xhl<C>();
		case 0x56: return ld_R_xhl<D>();
		case 0x5e: return ld_R_xhl<E>();
		case 0x66: return ld_R_xhl<H>();
		case 0x6e: return ld_R_xhl<L>();
		case 0x7e: return ld_R_xhl<A>();
		case 0x76: return halt();

		case 0x80: return add_a_R<B>();
		case 0x81: return add_a_R<C>();
		case 0x82: return add_a_R<D>();
		case 0x83: return add_a_R<E>();
		case 0x84: return add_a_R<H>();
		case 0x85: return add_a_R<L>();
		case 0x86: return add_a_xhl();
		case 0x87: return add_a_a();
		case 0x88: return adc_a_R<B>();
		case 0x89: return adc_a_R<C>();
		case 0x8a: return adc_a_R<D>();
		case 0x8b: return adc_a_R<E>();
		case 0x8c: return adc_a_R<H>();
		case 0x8d: return adc_a_R<L>();
		case 0x8e: return adc_a_xhl();
		case 0x8f: return adc_a_a();
		case 0x90: return sub_R<B>();
		case 0x91: return sub_R<C>();
		case 0x92: return sub_R<D>();
		case 0x93: return sub_R<E>();
		case 0x94: return sub_R<H>();
		case 0x95: return sub_R<L>();
		case 0x96: return sub_xhl();
		case 0x97: return sub_a();
		case 0x98: return sbc_a_R<B>();
		case 0x99: return sbc_a_R<C>();
		case 0x9a: return sbc_a_R<D>();
		case 0x9b: return sbc_a_R<E>();
		case 0x9c: return sbc_a_R<H>();
		case 0x9d: return sbc_a_R<L>();
		case 0x9e: return sbc_a_xhl();
		case 0x9f: return sbc_a_a();
		case 0xa0: return and_R<B>();
		case 0xa1: return and_R<C>();
		case 0xa2: return and_R<D>();
		case 0xa3: return and_R<E>();
		case 0xa4: return and_R<H>();
		case 0xa5: return and_R<L>();
		case 0xa6: return and_xhl();
		case 0xa7: return and_a();
		case 0xa8: return xor_R<B>();
		case 0xa9: return xor_R<C>();
		case 0xaa: return xor_R<D>();
		case 0xab: return xor_R<E>();
		case 0xac: return xor_R<H>();
		case 0xad: return xor_R<L>();
		case 0xae: return xor_xhl();
		case 0xaf: return xor_a();
		case 0xb0: return or_R<B>();
		case 0xb1: return or_R<C>();
		case 0xb2: return or_R<D>();
		case 0xb3: return or_R<E>();
		case 0xb4: return or_R<H>();
		case 0xb5: return or_R<L>();
		case 0xb6: return or_xhl();
		case 0xb7: return or_a();
		case 0xb8: return cp_R<B>();
		case 0xb9: return cp_R<C>();
		case 0xba: return cp_R<D>();
		case 0xbb: return cp_R<E>();
		case 0xbc: return cp_R<H>();
		case 0xbd: return cp_R<L>();
		case 0xbe: return cp_xhl();
		case 0xbf: return cp_a();

		case 0xd3: return out_byte_a();
		case 0xdb: return in_a_byte();
		case 0xd9: return exx();
		case 0xe3: return ex_xsp_SS<HL>();
		case 0xeb: return ex_de_hl();
		case 0xe9: return jp_SS<HL>();
		case 0xf9: return ld_sp_SS<HL>();
		case 0xf3: return di();
		case 0xfb: return ei();
		case 0xcb: return cb();
		case 0xed: return ed();
		case 0xdd: return xy<IX, IXH, IXL>();
		case 0xfd: return xy<IY, IYH, IYL>();
		case 0xc6: return add_a_byte();
		case 0xce: return adc_a_byte();
		case 0xd6: return sub_byte();
		case 0xde: return sbc_a_byte();
		case 0xe6: return and_byte();
		case 0xee: return xor_byte();
		case 0xf6: return or_byte();
		case 0xfe: return cp_byte();
		case 0xc0: return ret_nz();
		case 0xc8: return ret_z();
		case 0xd0: return ret_nc();
		case 0xd8: return ret_c();
		case 0xe0: return ret_po();
		case 0xe8: return ret_pe();
		case 0xf0: return ret_p();
		case 0xf8: return ret_m();
		case 0xc9: return ret();
		case 0xc2: return jp_nz();
		case 0xca: return jp_z();
		case 0xd2: return jp_nc();
		case 0xda: return jp_c();
		case 0xe2: return jp_po();
		case 0xea: return jp_pe();
		case 0xf2: return jp_p();
		case 0xfa: return jp_m();
		case 0xc3: return jp();
		case 0xc4: return call_nz();
		case 0xcc: return call_z();
		case 0xd4: return call_nc();
		case 0xdc: return call_c();
		case 0xe4: return call_po();
		case 0xec: return call_pe();
		case 0xf4: return call_p();
		case 0xfc: return call_m();
		case 0xcd: return call();
		case 0xc1: return pop_SS<BC>();
		case 0xd1: return pop_SS<DE>();
		case 0xe1: return pop_SS<HL>();
		case 0xf1: return pop_SS<AF>();
		case 0xc5: return push_SS<BC>();
		case 0xd5: return push_SS<DE>();
		case 0xe5: return push_SS<HL>();
		case 0xf5: return push_SS<AF>();
		case 0xc7: return rst<0x00>();
		case 0xcf: return rst<0x08>();
		case 0xd7: return rst<0x10>();
		case 0xdf: return rst<0x18>();
		case 0xe7: return rst<0x20>();
		case 0xef: return rst<0x28>();
		case 0xf7: return rst<0x30>();
		case 0xff: return rst<0x38>();
	}
	assert(false); return 0;
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

template <class T> void CPUCore<T>::executeFast()
{
	T::R800Refresh();
	byte opcode = RDMEM_OPCODE(T::CC_MAIN);
	int cycles = executeInstruction1_slow(opcode);
	T::add(cycles);
}

template <class T> ALWAYS_INLINE void CPUCore<T>::executeFastInline()
{
	T::R800Refresh();
	byte opcode = RDMEM_OPCODE(T::CC_MAIN);
	int cycles = executeInstruction1(opcode);
	T::add(cycles);
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
	} else if (unlikely(R.getHALT())) {
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
						executeFastInline();
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
template <class T> inline bool CPUCore<T>::cond_C()  { return R.getF() & C_FLAG; }
template <class T> inline bool CPUCore<T>::cond_NC() { return !cond_C(); }
template <class T> inline bool CPUCore<T>::cond_Z()  { return R.getF() & Z_FLAG; }
template <class T> inline bool CPUCore<T>::cond_NZ() { return !cond_Z(); }
template <class T> inline bool CPUCore<T>::cond_M()  { return R.getF() & S_FLAG; }
template <class T> inline bool CPUCore<T>::cond_P()  { return !cond_M(); }
template <class T> inline bool CPUCore<T>::cond_PE() { return R.getF() & V_FLAG; }
template <class T> inline bool CPUCore<T>::cond_PO() { return !cond_PE(); }


// LD r,r
template <class T> template<CPU::Reg8 DST, CPU::Reg8 SRC> int CPUCore<T>::ld_R_R() {
	R.set<DST>(R.get<SRC>()); return T::CC_LD_R_R;
}

// LD SP,ss
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_sp_SS() {
	R.setSP(R.get<REG>()); return T::CC_LD_SP_HL;
}

// LD (ss),a
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_SS_a() {
	WRMEM(R.get<REG>(), R.getA(), T::CC_LD_SS_A_1);
	return T::CC_LD_SS_A;
}

// LD (HL),r
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::ld_xhl_R() {
	WRMEM(R.getHL(), R.get<SRC>(), T::CC_LD_HL_R_1);
	return T::CC_LD_HL_R;
}

// LD (IXY+e),r
template <class T> template<CPU::Reg16 IXY, CPU::Reg8 SRC> int CPUCore<T>::ld_xix_R() {
	offset ofst = RDMEM_OPCODE(T::CC_LD_XIX_R_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	WRMEM(memptr, R.get<SRC>(), T::CC_LD_XIX_R_2);
	return T::CC_LD_XIX_R;
}

// LD (HL),n
template <class T> int CPUCore<T>::ld_xhl_byte() {
	byte val = RDMEM_OPCODE(T::CC_LD_HL_N_1);
	WRMEM(R.getHL(), val, T::CC_LD_HL_N_2);
	return T::CC_LD_HL_N;
}

// LD (IXY+e),n
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::ld_xix_byte() {
	unsigned tmp = RD_WORD_PC(T::CC_LD_XIX_N_1);
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_N_2);
	return T::CC_LD_XIX_N;
}

// LD (nn),A
template <class T> int CPUCore<T>::ld_xbyte_a() {
	unsigned x = RD_WORD_PC(T::CC_LD_NN_A_1);
	memptr = R.getA() << 8;
	WRMEM(x, R.getA(), T::CC_LD_NN_A_2);
	return T::CC_LD_NN_A;
}

// LD (nn),ss
template <class T> inline int CPUCore<T>::WR_NN_Y(unsigned reg, int ee) {
	memptr = RD_WORD_PC(T::CC_LD_XX_HL_1 + ee);
	WR_WORD(memptr, reg, T::CC_LD_XX_HL_2 + ee);
	return T::CC_LD_XX_HL + ee;
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_xword_SS() {
	return WR_NN_Y(R.get<REG>(), 0);
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_xword_SS_ED() {
	return WR_NN_Y(R.get<REG>(), T::EE_ED);
}

// LD A,(ss)
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_a_SS() {
	R.setA(RDMEM(R.get<REG>(), T::CC_LD_A_SS_1)); return T::CC_LD_A_SS;
}

// LD A,(nn)
template <class T> int CPUCore<T>::ld_a_xbyte() {
	unsigned addr = RD_WORD_PC(T::CC_LD_A_NN_1);
	memptr = addr + 1;
	R.setA(RDMEM(addr, T::CC_LD_A_NN_2));
	return T::CC_LD_A_NN;
}

// LD r,n
template <class T> template<CPU::Reg8 DST> int CPUCore<T>::ld_R_byte() {
	R.set<DST>(RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N;
}

// LD r,(hl)
template <class T> template<CPU::Reg8 DST> int CPUCore<T>::ld_R_xhl() {
	R.set<DST>(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL;
}

// LD r,(IXY+e)
template <class T> template<CPU::Reg8 DST, CPU::Reg16 IXY> int CPUCore<T>::ld_R_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_LD_R_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	R.set<DST>(RDMEM(memptr, T::CC_LD_R_XIX_2));
	return T::CC_LD_R_XIX;
}

// LD ss,(nn)
template <class T> inline unsigned CPUCore<T>::RD_P_XX(int ee) {
	unsigned addr = RD_WORD_PC(T::CC_LD_HL_XX_1 + ee);
	memptr = addr + 1; // not 16-bit
	unsigned result = RD_WORD(addr, T::CC_LD_HL_XX_2 + ee);
	return result;
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_SS_xword() {
	R.set<REG>(RD_P_XX(0));        return T::CC_LD_HL_XX;
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_SS_xword_ED() {
	R.set<REG>(RD_P_XX(T::EE_ED)); return T::CC_LD_HL_XX + T::EE_ED;
}

// LD ss,nn
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_SS_word() {
	R.set<REG>(RD_WORD_PC(T::CC_LD_SS_NN_1)); return T::CC_LD_SS_NN;
}


// ADC A,r
template <class T> inline void CPUCore<T>::ADC(byte reg) {
	unsigned res = R.getA() + reg + ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((R.getA() ^ res) & (reg ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> inline int CPUCore<T>::adc_a_a() {
	unsigned res = 2 * R.getA() + ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       (res & H_FLAG) |
	       (((R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::adc_a_R() {
	ADC(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::adc_a_byte() {
	ADC(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::adc_a_xhl() {
	ADC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::adc_a_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	ADC(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// ADD A,r
template <class T> inline void CPUCore<T>::ADD(byte reg) {
	unsigned res = R.getA() + reg;
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((R.getA() ^ res) & (reg ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> inline int CPUCore<T>::add_a_a() {
	unsigned res = 2 * R.getA();
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       (res & H_FLAG) |
	       (((R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::add_a_R() {
	ADD(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::add_a_byte() {
	ADD(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::add_a_xhl() {
	ADD(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::add_a_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	ADD(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// AND r
template <class T> inline void CPUCore<T>::AND(byte reg) {
	R.setA(R.getA() & reg);
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
}
template <class T> int CPUCore<T>::and_a() {
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::and_R() {
	AND(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::and_byte() {
	AND(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::and_xhl() {
	AND(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::and_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	AND(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// CP r
template <class T> inline void CPUCore<T>::CP(byte reg) {
	unsigned q = R.getA() - reg;
	R.setF(ZSTable[q & 0xFF] |
	       (reg & (X_FLAG | Y_FLAG)) |	// XY from operand, not from result
	       ((q & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ q ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ q) & 0x80) >> 5)); // V_FLAG
}
template <class T> int CPUCore<T>::cp_a() {
	R.setF(ZS0 | N_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG))); // XY from operand, not from result
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::cp_R() {
	CP(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::cp_byte() {
	CP(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::cp_xhl() {
	CP(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::cp_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	CP(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// OR r
template <class T> inline void CPUCore<T>::OR(byte reg) {
	R.setA(R.getA() | reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> int CPUCore<T>::or_a() {
	R.setF(ZSPXYTable[R.getA()]); return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::or_R() {
	OR(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::or_byte() {
	OR(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::or_xhl() {
	OR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::or_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	OR(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// SBC A,r
template <class T> inline void CPUCore<T>::SBC(byte reg) {
	unsigned res = R.getA() - reg - ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> int CPUCore<T>::sbc_a_a() {
	R.setAF((R.getF() & C_FLAG) ?
	        (255 * 256 | ZSXY255 | C_FLAG | H_FLAG | N_FLAG) :
	        (  0 * 256 | ZSXY0   |                   N_FLAG));
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::sbc_a_R() {
	SBC(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::sbc_a_byte() {
	SBC(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::sbc_a_xhl() {
	SBC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::sbc_a_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	SBC(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// SUB r
template <class T> inline void CPUCore<T>::SUB(byte reg) {
	unsigned res = R.getA() - reg;
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       N_FLAG |
	       ((R.getA() ^ res ^ reg) & H_FLAG) |
	       (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
}
template <class T> int CPUCore<T>::sub_a() {
	R.setAF(0 * 256 | ZSXY0 | N_FLAG);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::sub_R() {
	SUB(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::sub_byte() {
	SUB(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::sub_xhl() {
	SUB(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::sub_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	SUB(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// XOR r
template <class T> inline void CPUCore<T>::XOR(byte reg) {
	R.setA(R.getA() ^ reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> int CPUCore<T>::xor_a() {
	R.setAF(0 * 256 + ZSPXY0); return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::xor_R() {
	XOR(R.get<SRC>()); return T::CC_CP_R;
}
template <class T> int CPUCore<T>::xor_byte() {
	XOR(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::xor_xhl() {
	XOR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::xor_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	XOR(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}


// DEC r
template <class T> inline byte CPUCore<T>::DEC(byte reg) {
	byte res = reg - 1;
	R.setF((R.getF() & C_FLAG) |
	       ((reg & ~res & 0x80) >> 5) | // V_FLAG
	       (((res & 0x0F) + 1) & H_FLAG) |
	       ZSXYTable[res] |
	       N_FLAG);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::dec_R() {
	R.set<REG>(DEC(R.get<REG>())); return T::CC_INC_R;
}
template <class T> inline int CPUCore<T>::DEC_X(unsigned x, int ee) {
	byte val = DEC(RDMEM(x, T::CC_INC_XHL_1 + ee));
	WRMEM(x, val, T::CC_INC_XHL_2 + ee);
	return T::CC_INC_XHL + ee;
}
template <class T> int CPUCore<T>::dec_xhl() {
	return DEC_X(R.getHL(), 0);
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::dec_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	return DEC_X(memptr, T::EE_INC_XIX);
}

// INC r
template <class T> inline byte CPUCore<T>::INC(byte reg) {
	reg++;
	R.setF((R.getF() & C_FLAG) |
	       ((reg & -reg & 0x80) >> 5) | // V_FLAG
	       (((reg & 0x0F) - 1) & H_FLAG) |
	       ZSXYTable[reg]);
	return reg;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::inc_R() {
	R.set<REG>(INC(R.get<REG>())); return T::CC_INC_R;
}
template <class T> inline int CPUCore<T>::INC_X(unsigned x, int ee) {
	byte val = INC(RDMEM(x, T::CC_INC_XHL_1 + ee));
	WRMEM(x, val, T::CC_INC_XHL_2 + ee);
	return T::CC_INC_XHL + ee;
}
template <class T> int CPUCore<T>::inc_xhl() {
	return INC_X(R.getHL(), 0);
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::inc_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.get<IXY>() + ofst) & 0xFFFF;
	return INC_X(memptr, T::EE_INC_XIX);
}


// ADC HL,ss
template <class T> template<CPU::Reg16 REG> inline int CPUCore<T>::adc_hl_SS() {
	unsigned reg = R.get<REG>();
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
	return T::CC_ADC_HL_SS;
}
template <class T> int CPUCore<T>::adc_hl_hl() {
	memptr = R.getHL() + 1; // not 16-bit
	unsigned res = 2 * R.getHL() + ((R.getF() & C_FLAG) ? 1 : 0);
	if (res & 0xFFFF) {
		R.setF((res >> 16) | // C_FLAG
		       0 | // Z_FLAG
		       (((R.getHL() ^ res) & 0x8000) >> 13) | // V_FLAG
		       ((res >> 8) & (H_FLAG | S_FLAG | X_FLAG | Y_FLAG)));
	} else {
		R.setF((res >> 16) | // C_FLAG
		       Z_FLAG |
		       ((R.getHL() & 0x8000) >> 13) | // V_FLAG
		       0); // H_FLAG S_FLAG X_FLAG Y_FLAG
	}
	R.setHL(res);
	return T::CC_ADC_HL_SS;
}

// ADD HL/IX/IY,ss
template <class T> template<CPU::Reg16 REG1, CPU::Reg16 REG2> int CPUCore<T>::add_SS_TT() {
	unsigned reg1 = R.get<REG1>();
	unsigned reg2 = R.get<REG2>();
	memptr = reg1 + 1; // not 16-bit
	unsigned res = reg1 + reg2;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	       (res >> 16) | // C_FLAG
	       ((res >> 8) & (X_FLAG | Y_FLAG)));
	R.set<REG1>(res & 0xFFFF);
	return T::CC_ADD_HL_SS;
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::add_SS_SS() {
	unsigned reg = R.get<REG>();
	memptr = reg + 1; // not 16-bit
	unsigned res = 2 * reg;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (res >> 16) | // C_FLAG
	       ((res >> 8) & (H_FLAG | X_FLAG | Y_FLAG)));
	R.set<REG>(res & 0xFFFF);
	return T::CC_ADD_HL_SS;
}

// SBC HL,ss
template <class T> template<CPU::Reg16 REG> inline int CPUCore<T>::sbc_hl_SS() {
	unsigned reg = R.get<REG>();
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
	return T::CC_ADC_HL_SS;
}
template <class T> int CPUCore<T>::sbc_hl_hl() {
	memptr = R.getHL() + 1; // not 16-bit
	if (R.getF() & C_FLAG) {
		R.setF(C_FLAG | H_FLAG | S_FLAG | X_FLAG | Y_FLAG | N_FLAG);
		R.setHL(0xFFFF);
	} else {
		R.setF(Z_FLAG | N_FLAG);
		R.setHL(0);
	}
	return T::CC_ADC_HL_SS;
}

// DEC ss
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::dec_SS() {
	R.set<REG>(R.get<REG>() - 1); return T::CC_INC_SS;
}

// INC ss
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::inc_SS() {
	R.set<REG>(R.get<REG>() + 1); return T::CC_INC_SS;
}


// BIT n,r
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::bit_N_R() {
	byte reg = R.get<REG>();
	R.setF((R.getF() & C_FLAG) |
	       ZSPHTable[reg & (1 << N)] |
	       (reg & (X_FLAG | Y_FLAG)));
	return T::CC_BIT_R;
}
template <class T> template<unsigned N> inline int CPUCore<T>::bit_N_xhl() {
	R.setF((R.getF() & C_FLAG) |
	       ZSPHTable[RDMEM(R.getHL(), T::CC_BIT_XHL_1) & (1 << N)] |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	return T::CC_BIT_XHL;
}
template <class T> template<unsigned N> inline int CPUCore<T>::bit_N_xix(unsigned addr) {
	memptr = addr;
	R.setF((R.getF() & C_FLAG) |
	       ZSPHTable[RDMEM(memptr, T::CC_BIT_XIX_1) & (1 << N)] |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	return T::CC_BIT_XIX;
}

// RES n,r
static inline byte RES(unsigned b, byte reg) {
	return reg & ~(1 << b);
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::res_N_R() {
	R.set<REG>(RES(N, R.get<REG>())); return T::CC_SET_R;
}
template <class T> inline byte CPUCore<T>::RES_X(unsigned bit, unsigned addr, int ee) {
	byte res = RES(bit, RDMEM(addr, T::CC_SET_XHL_1 + ee));
	WRMEM(addr, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<unsigned N> int CPUCore<T>::res_N_xhl() {
	RES_X(N, R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::res_N_xix_R(unsigned a) {
	memptr = a; R.set<REG>(RES_X(N, a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// SET n,r
static inline byte SET(unsigned b, byte reg) {
	return reg | (1 << b);
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::set_N_R() {
	R.set<REG>(SET(N, R.get<REG>())); return T::CC_SET_R;
}
template <class T> inline byte CPUCore<T>::SET_X(unsigned bit, unsigned addr, int ee) {
	byte res = SET(bit, RDMEM(addr, T::CC_SET_XHL_1 + ee));
	WRMEM(addr, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<unsigned N> int CPUCore<T>::set_N_xhl() {
	SET_X(N, R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::set_N_xix_R(unsigned a) {
	memptr = a; R.set<REG>(SET_X(N, a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// RL r
template <class T> inline byte CPUCore<T>::RL(byte reg) {
	byte c = reg >> 7;
	reg = (reg << 1) | ((R.getF() & C_FLAG) ? 0x01 : 0);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RL_X(unsigned x, int ee) {
	byte res = RL(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rl_R() {
	R.set<REG>(RL(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rl_xhl() {
	RL_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rl_xix_R(unsigned a) {
	memptr = a; R.set<REG>(RL_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// RLC r
template <class T> inline byte CPUCore<T>::RLC(byte reg) {
	byte c = reg >> 7;
	reg = (reg << 1) | c;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RLC_X(unsigned x, int ee) {
	byte res = RLC(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rlc_R() {
	R.set<REG>(RLC(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rlc_xhl() {
	RLC_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rlc_xix_R(unsigned a) {
	memptr = a; R.set<REG>(RLC_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// RR r
template <class T> inline byte CPUCore<T>::RR(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | ((R.getF() & C_FLAG) << 7);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RR_X(unsigned x, int ee) {
	byte res = RR(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rr_R() {
	R.set<REG>(RR(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rr_xhl() {
	RR_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rr_xix_R(unsigned a) {
	memptr = a; R.set<REG>(RR_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// RRC r
template <class T> inline byte CPUCore<T>::RRC(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | (c << 7);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RRC_X(unsigned x, int ee) {
	byte res = RRC(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rrc_R() {
	R.set<REG>(RRC(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rrc_xhl() {
	RRC_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rrc_xix_R(unsigned a) {
	memptr = a; R.set<REG>(RRC_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// SLA r
template <class T> inline byte CPUCore<T>::SLA(byte reg) {
	byte c = reg >> 7;
	reg <<= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLA_X(unsigned x, int ee) {
	byte res = SLA(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sla_R() {
	R.set<REG>(SLA(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::sla_xhl() {
	SLA_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sla_xix_R(unsigned a) {
	memptr = a; R.set<REG>(SLA_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// SLL r
template <class T> inline byte CPUCore<T>::SLL(byte reg) {
	byte c = reg >> 7;
	reg = (reg << 1) | 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLL_X(unsigned x, int ee) {
	byte res = SLL(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sll_R() {
	R.set<REG>(SLL(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::sll_xhl() {
	SLL_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sll_xix_R(unsigned a) {
	memptr = a; R.set<REG>(SLL_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// SRA r
template <class T> inline byte CPUCore<T>::SRA(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | (reg & 0x80);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRA_X(unsigned x, int ee) {
	byte res = SRA(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sra_R() {
	R.set<REG>(SRA(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::sra_xhl() {
	SRA_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sra_xix_R(unsigned a) {
	memptr = a; R.set<REG>(SRA_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// SRL R
template <class T> inline byte CPUCore<T>::SRL(byte reg) {
	byte c = reg & 1;
	reg >>= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRL_X(unsigned x, int ee) {
	byte res = SRL(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::srl_R() {
	R.set<REG>(SRL(R.get<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::srl_xhl() {
	SRL_X(R.getHL(), 0); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::srl_xix_R(unsigned a) {
	memptr = a; R.set<REG>(SRL_X(a, T::EE_SET_XIX)); return T::CC_SET_XIX;
}

// RLA RLCA RRA RRCA
template <class T> int CPUCore<T>::rla() {
	byte c = R.getF() & C_FLAG;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x80) ? C_FLAG : 0));
	R.setA((R.getA() << 1) | (c ? 1 : 0));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rlca() {
	R.setA((R.getA() << 1) | (R.getA() >> 7));
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() & (Y_FLAG | X_FLAG | C_FLAG)));
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rra() {
	byte c = (R.getF() & C_FLAG) << 7;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x01) ? C_FLAG : 0));
	R.setA((R.getA() >> 1) | c);
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rrca() {
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() &  C_FLAG));
	R.setA((R.getA() >> 1) | (R.getA() << 7));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_RLA;
}


// RLD
template <class T> int CPUCore<T>::rld() {
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	memptr = R.getHL() + 1; // not 16-bit
	WRMEM(R.getHL(), (val << 4) | (R.getA() & 0x0F), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val >> 4));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
	return T::CC_RLD;
}

// RRD
template <class T> int CPUCore<T>::rrd() {
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	memptr = R.getHL() + 1; // not 16-bit
	WRMEM(R.getHL(), (val >> 4) | (R.getA() << 4), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val & 0x0F));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
	return T::CC_RLD;
}


// PUSH ss
template <class T> inline void CPUCore<T>::PUSH(unsigned reg, int ee) {
	R.setSP(R.getSP() - 2);
	WR_WORD_rev<true, true>(R.getSP(), reg, T::CC_PUSH_1 + ee);
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::push_SS() {
	PUSH(R.get<REG>(), 0); return T::CC_PUSH;
}

// POP ss
template <class T> inline unsigned CPUCore<T>::POP(int ee) {
	unsigned addr = R.getSP();
	R.setSP(addr + 2);
	return RD_WORD(addr, T::CC_POP_1 + ee);
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::pop_SS() {
	R.set<REG>(POP(0)); return T::CC_POP;
}


// CALL nn / CALL cc,nn
template <class T> inline int CPUCore<T>::CALL() {
	memptr = RD_WORD_PC(T::CC_CALL_1);
	PUSH(R.getPC(), T::EE_CALL);
	R.setPC(memptr);
	return T::CC_CALL_A;
}
template <class T> inline int CPUCore<T>::SKIP_CALL() {
	memptr = RD_WORD_PC(T::CC_CALL_1);
	return T::CC_CALL_B;
}
template <class T> int CPUCore<T>::call()    { return             CALL();               }
template <class T> int CPUCore<T>::call_c()  { return cond_C()  ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_m()  { return cond_M()  ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_nc() { return cond_NC() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_nz() { return cond_NZ() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_p()  { return cond_P()  ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_pe() { return cond_PE() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_po() { return cond_PO() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_z()  { return cond_Z()  ? CALL() : SKIP_CALL(); }


// RST n
template <class T> template<unsigned ADDR> int CPUCore<T>::rst() {
	PUSH(R.getPC(), 0);
	memptr = ADDR;
	R.setPC(ADDR);
	return T::CC_RST;
}


// RET
template <class T> inline int CPUCore<T>::RET(bool cond, int ee) {
	if (cond) {
		memptr = POP(ee);
		R.setPC(memptr);
		return T::CC_RET_A + ee;
	} else {
		return T::CC_RET_B + ee;
	}
}
template <class T> int CPUCore<T>::ret()    { return RET(true,      0);           }
template <class T> int CPUCore<T>::ret_c()  { return RET(cond_C(),  T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_m()  { return RET(cond_M(),  T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_nc() { return RET(cond_NC(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_nz() { return RET(cond_NZ(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_p()  { return RET(cond_P(),  T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_pe() { return RET(cond_PE(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_po() { return RET(cond_PO(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_z()  { return RET(cond_Z(),  T::EE_RET_C); }

template <class T> int CPUCore<T>::retn() { // also reti
	R.setIFF1(R.getIFF2());
	R.setNextIFF1(R.getIFF2());
	setSlowInstructions();
	return RET(true, T::EE_RETN);
}


// JP ss
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::jp_SS() {
	R.setPC(R.get<REG>()); T::R800ForcePageBreak(); return T::CC_JP_HL;
}

// JP nn / JP cc,nn
template <class T> inline int CPUCore<T>::JP() {
	memptr = RD_WORD_PC(T::CC_JP_1);
	R.setPC(memptr);
	T::R800ForcePageBreak();
	return T::CC_JP_A;
}
template <class T> inline int CPUCore<T>::SKIP_JP() {
	memptr = RD_WORD_PC(T::CC_JP_1);
	return T::CC_JP_B;
}
template <class T> int CPUCore<T>::jp()    { return             JP();             }
template <class T> int CPUCore<T>::jp_c()  { return cond_C()  ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_m()  { return cond_M()  ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_nc() { return cond_NC() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_nz() { return cond_NZ() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_p()  { return cond_P()  ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_pe() { return cond_PE() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_po() { return cond_PO() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_z()  { return cond_Z()  ? JP() : SKIP_JP(); }

// JR e
template <class T> inline int CPUCore<T>::JR(int ee) {
	offset ofst = RDMEM_OPCODE(T::CC_JR_1 + ee);
	R.setPC((R.getPC() + ofst) & 0xFFFF);
	memptr = R.getPC();
	return T::CC_JR_A + ee;
}
template <class T> inline int CPUCore<T>::SKIP_JR(int ee) {
	RDMEM_OPCODE(T::CC_JR_1 + ee); // ignore return value
	return T::CC_JR_B + ee;
}
template <class T> int CPUCore<T>::jr()    { return             JR(0);              }
template <class T> int CPUCore<T>::jr_c()  { return cond_C()  ? JR(0) : SKIP_JR(0); }
template <class T> int CPUCore<T>::jr_nc() { return cond_NC() ? JR(0) : SKIP_JR(0); }
template <class T> int CPUCore<T>::jr_nz() { return cond_NZ() ? JR(0) : SKIP_JR(0); }
template <class T> int CPUCore<T>::jr_z()  { return cond_Z()  ? JR(0) : SKIP_JR(0); }

// DJNZ e
template <class T> int CPUCore<T>::djnz() {
	byte b = R.getB();
	R.setB(--b);
	return b ? JR(T::EE_DJNZ) : SKIP_JR(T::EE_DJNZ);
}

// EX (SP),ss
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ex_xsp_SS() {
	memptr = RD_WORD_impl<true, false>(R.getSP(), T::CC_EX_SP_HL_1);
	WR_WORD_rev<false, true>(R.getSP(), R.get<REG>(), T::CC_EX_SP_HL_2);
	R.set<REG>(memptr);
	return T::CC_EX_SP_HL;
}

// IN r,(c)
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::in_R_c() {
	byte res = READ_PORT(R.getBC(), T::CC_IN_R_C_1);
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[res]);
	R.set<REG>(res);
	return T::CC_IN_R_C;
}

// IN a,(n)
template <class T> int CPUCore<T>::in_a_byte() {
	unsigned y = RDMEM_OPCODE(T::CC_IN_A_N_1) + 256 * R.getA();
	R.setA(READ_PORT(y, T::CC_IN_A_N_2));
	return T::CC_IN_A_N;
}

// OUT (c),r
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::out_c_R() {
	WRITE_PORT(R.getBC(), R.get<REG>(), T::CC_OUT_C_R_1);
	return T::CC_OUT_C_R;
}

// OUT (n),a
template <class T> int CPUCore<T>::out_byte_a() {
	unsigned y = RDMEM_OPCODE(T::CC_OUT_N_A_1) + 256 * R.getA();
	WRITE_PORT(y, R.getA(), T::CC_OUT_N_A_2);
	return T::CC_OUT_N_A;
}


// block CP
template <class T> inline int CPUCore<T>::BLOCK_CP(int increase, bool repeat) {
	byte val = RDMEM(R.getHL(), T::CC_CPI_1);
	byte res = R.getA() - val;
	R.setHL(R.getHL() + increase);
	R.setBC(R.getBC() - 1);
	byte f = (R.getF() & C_FLAG) |
	         ((R.getA() ^ val ^ res) & H_FLAG) |
	         ZSTable[res] |
	         N_FLAG;
	unsigned k = res - ((f & H_FLAG) >> 4);
	R.setF(f |
	       ((k << 4) & Y_FLAG) | // bit 1 -> flag 5
	       (k & X_FLAG) |        // bit 3 -> flag 3
	       (R.getBC() ? V_FLAG : 0));
	if (repeat && R.getBC() && res) {
		R.setPC(R.getPC() - 2);
		return T::CC_CPIR;
	} else {
		return T::CC_CPI;
	}
}
template <class T> int CPUCore<T>::cpd()  { return BLOCK_CP(-1, false); }
template <class T> int CPUCore<T>::cpi()  { return BLOCK_CP( 1, false); }
template <class T> int CPUCore<T>::cpdr() { return BLOCK_CP(-1, true ); }
template <class T> int CPUCore<T>::cpir() { return BLOCK_CP( 1, true ); }


// block LD
template <class T> inline int CPUCore<T>::BLOCK_LD(int increase, bool repeat) {
	byte val = RDMEM(R.getHL(), T::CC_LDI_1);
	WRMEM(R.getDE(), val, T::CC_LDI_2);
	R.setHL(R.getHL() + increase);
	R.setDE(R.getDE() + increase);
	R.setBC(R.getBC() - 1);
	R.setF((R.getF() & (S_FLAG | Z_FLAG | C_FLAG)) |
	       (((R.getA() + val) << 4) & Y_FLAG) | // bit 1 -> flag 5
	       ((R.getA() + val) & X_FLAG) |        // bit 3 -> flag 3
	       (R.getBC() ? V_FLAG : 0));
	if (repeat && R.getBC()) {
		R.setPC(R.getPC() - 2);
		return T::CC_LDIR;
	} else {
		return T::CC_LDI;
	}
}
template <class T> int CPUCore<T>::ldd()  { return BLOCK_LD(-1, false); }
template <class T> int CPUCore<T>::ldi()  { return BLOCK_LD( 1, false); }
template <class T> int CPUCore<T>::lddr() { return BLOCK_LD(-1, true ); }
template <class T> int CPUCore<T>::ldir() { return BLOCK_LD( 1, true ); }


// block IN
template <class T> inline int CPUCore<T>::BLOCK_IN(int increase, bool repeat) {
	R.setBC(R.getBC() - 0x100); // decr before use
	byte val = READ_PORT(R.getBC(), T::CC_INI_1);
	WRMEM(R.getHL(), val, T::CC_INI_2);
	R.setHL(R.getHL() + increase);
	unsigned k = val + ((R.getC() + increase) & 0xFF);
	byte b = R.getB();
	R.setF(((val & S_FLAG) >> 6) | // N_FLAG
	       ((k & 0x100) ? (H_FLAG | C_FLAG) : 0) |
	       ZSXYTable[b] |
	       (ZSPXYTable[(k & 0x07) ^ b] & P_FLAG));
	if (repeat && b) {
		R.setPC(R.getPC() - 2);
		return T::CC_INIR;
	} else {
		return T::CC_INI;
	}
}
template <class T> int CPUCore<T>::ind()  { return BLOCK_IN(-1, false); }
template <class T> int CPUCore<T>::ini()  { return BLOCK_IN( 1, false); }
template <class T> int CPUCore<T>::indr() { return BLOCK_IN(-1, true ); }
template <class T> int CPUCore<T>::inir() { return BLOCK_IN( 1, true ); }


// block OUT
template <class T> inline int CPUCore<T>::BLOCK_OUT(int increase, bool repeat) {
	byte val = RDMEM(R.getHL(), T::CC_OUTI_1);
	R.setHL(R.getHL() + increase);
	WRITE_PORT(R.getBC(), val, T::CC_OUTI_2);
	R.setB(R.getB() - 1); // decr after use
	unsigned k = val + R.getL();
	byte b = R.getB();
	R.setF(((val & S_FLAG) >> 6) | // N_FLAG
	       ((k & 0x100) ? (H_FLAG | C_FLAG) : 0) |
	       ZSXYTable[b] |
	       (ZSPXYTable[(k & 0x07) ^ b] & P_FLAG));
	if (repeat && b) {
		R.setPC(R.getPC() - 2);
		return T::CC_OTIR;
	} else {
		return T::CC_OUTI;
	}
}
template <class T> int CPUCore<T>::outd() { return BLOCK_OUT(-1, false); }
template <class T> int CPUCore<T>::outi() { return BLOCK_OUT( 1, false); }
template <class T> int CPUCore<T>::otdr() { return BLOCK_OUT(-1, true ); }
template <class T> int CPUCore<T>::otir() { return BLOCK_OUT( 1, true ); }


// various
template <class T> int CPUCore<T>::nop() { return T::CC_NOP; }
template <class T> int CPUCore<T>::ccf() {
	R.setF(((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	       ((R.getF() & C_FLAG) << 4) | // H_FLAG
	       (R.getA() & (X_FLAG | Y_FLAG))                  ) ^ C_FLAG);
	return T::CC_CCF;
}
template <class T> int CPUCore<T>::cpl() {
	R.setA(R.getA() ^ 0xFF);
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	       H_FLAG | N_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_CPL;

}
template <class T> int CPUCore<T>::daa() {
	unsigned i = R.getA();
	i |= (R.getF() & (C_FLAG | N_FLAG)) << 8; // 0x100 0x200
	i |= (R.getF() &  H_FLAG)           << 6; // 0x400
	R.setAF(DAATable[i]);
	return T::CC_DAA;
}
template <class T> int CPUCore<T>::neg() {
	 byte i = R.getA();
	 R.setA(0);
	 SUB(i);
	return T::CC_NEG;
}
template <class T> int CPUCore<T>::scf() {
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       C_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_SCF;
}

template <class T> int CPUCore<T>::ex_af_af() {
	unsigned t = R.getAF2(); R.setAF2(R.getAF()); R.setAF(t);
	return T::CC_EX;
}
template <class T> int CPUCore<T>::ex_de_hl() {
	unsigned t = R.getDE(); R.setDE(R.getHL()); R.setHL(t);
	return T::CC_EX;
}
template <class T> int CPUCore<T>::exx() {
	unsigned t1 = R.getBC2(); R.setBC2(R.getBC()); R.setBC(t1);
	unsigned t2 = R.getDE2(); R.setDE2(R.getDE()); R.setDE(t2);
	unsigned t3 = R.getHL2(); R.setHL2(R.getHL()); R.setHL(t3);
	return T::CC_EX;
}

template <class T> int CPUCore<T>::di() {
	R.di();
	return T::CC_DI;
}
template <class T> int CPUCore<T>::ei() {
	R.setIFF1(false);	// no ints after this instruction
	R.setNextIFF1(true);	// but allow them after next instruction
	R.setIFF2(true);
	setSlowInstructions();
	return T::CC_EI;
}
template <class T> int CPUCore<T>::halt() {
	R.setHALT(true);
	setSlowInstructions();

	if (!(R.getIFF1() || R.getNextIFF1() || R.getIFF2())) {
		motherboard.getMSXCliComm().printWarning(
			"DI; HALT detected, which means a hang. "
			"You can just as well reset the MSX now...\n");
	}
	return T::CC_HALT;
}
template <class T> template<unsigned N> int CPUCore<T>::im_N() {
	R.setIM(N); return T::CC_IM;
}

// LD A,I/R
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::ld_a_IR() {
	R.setA(R.get<REG>());
	R.setF((R.getF() & C_FLAG) |
	       ZSXYTable[R.getA()] |
	       (R.getIFF2() ? V_FLAG : 0));
	return T::CC_LD_A_I;
}

// LD I/R,A
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::ld_IR_a() {
	R.set<REG>(R.getA());
	return T::CC_LD_A_I;
}

// MULUB A,r
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::mulub_a_R() {
	// TODO check flags
	R.setHL(unsigned(R.getA()) * R.get<REG>());
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (R.getHL() ? 0 : Z_FLAG) |
	       ((R.getHL() & 0x8000) ? C_FLAG : 0));
	return T::CC_MULUB;
}

// MULUW HL,ss
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::muluw_hl_SS() {
	// TODO check flags
	unsigned res = unsigned(R.getHL()) * R.get<REG>();
	R.setDE(res >> 16);
	R.setHL(res & 0xffff);
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (res ? 0 : Z_FLAG) |
	       ((res & 0x80000000) ? C_FLAG : 0));
	return T::CC_MULUW;
}


// prefixes
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::xy_cb() {
	unsigned tmp = RD_WORD_PC(T::CC_DD_CB);
	offset ofst = tmp & 0xFF;
	unsigned addr = (R.get<IXY>() + ofst) & 0xFFFF;
	unsigned opcode = tmp >> 8;
	switch (opcode) {
		case 0x00: return rlc_xix_R<B>(addr);
		case 0x01: return rlc_xix_R<C>(addr);
		case 0x02: return rlc_xix_R<D>(addr);
		case 0x03: return rlc_xix_R<E>(addr);
		case 0x04: return rlc_xix_R<H>(addr);
		case 0x05: return rlc_xix_R<L>(addr);
		case 0x06: return rlc_xix_R<DUMMY>(addr);
		case 0x07: return rlc_xix_R<A>(addr);
		case 0x08: return rrc_xix_R<B>(addr);
		case 0x09: return rrc_xix_R<C>(addr);
		case 0x0a: return rrc_xix_R<D>(addr);
		case 0x0b: return rrc_xix_R<E>(addr);
		case 0x0c: return rrc_xix_R<H>(addr);
		case 0x0d: return rrc_xix_R<L>(addr);
		case 0x0e: return rrc_xix_R<DUMMY>(addr);
		case 0x0f: return rrc_xix_R<A>(addr);
		case 0x10: return rl_xix_R<B>(addr);
		case 0x11: return rl_xix_R<C>(addr);
		case 0x12: return rl_xix_R<D>(addr);
		case 0x13: return rl_xix_R<E>(addr);
		case 0x14: return rl_xix_R<H>(addr);
		case 0x15: return rl_xix_R<L>(addr);
		case 0x16: return rl_xix_R<DUMMY>(addr);
		case 0x17: return rl_xix_R<A>(addr);
		case 0x18: return rr_xix_R<B>(addr);
		case 0x19: return rr_xix_R<C>(addr);
		case 0x1a: return rr_xix_R<D>(addr);
		case 0x1b: return rr_xix_R<E>(addr);
		case 0x1c: return rr_xix_R<H>(addr);
		case 0x1d: return rr_xix_R<L>(addr);
		case 0x1e: return rr_xix_R<DUMMY>(addr);
		case 0x1f: return rr_xix_R<A>(addr);
		case 0x20: return sla_xix_R<B>(addr);
		case 0x21: return sla_xix_R<C>(addr);
		case 0x22: return sla_xix_R<D>(addr);
		case 0x23: return sla_xix_R<E>(addr);
		case 0x24: return sla_xix_R<H>(addr);
		case 0x25: return sla_xix_R<L>(addr);
		case 0x26: return sla_xix_R<DUMMY>(addr);
		case 0x27: return sla_xix_R<A>(addr);
		case 0x28: return sra_xix_R<B>(addr);
		case 0x29: return sra_xix_R<C>(addr);
		case 0x2a: return sra_xix_R<D>(addr);
		case 0x2b: return sra_xix_R<E>(addr);
		case 0x2c: return sra_xix_R<H>(addr);
		case 0x2d: return sra_xix_R<L>(addr);
		case 0x2e: return sra_xix_R<DUMMY>(addr);
		case 0x2f: return sra_xix_R<A>(addr);
		case 0x30: return sll_xix_R<B>(addr);
		case 0x31: return sll_xix_R<C>(addr);
		case 0x32: return sll_xix_R<D>(addr);
		case 0x33: return sll_xix_R<E>(addr);
		case 0x34: return sll_xix_R<H>(addr);
		case 0x35: return sll_xix_R<L>(addr);
		case 0x36: return sll_xix_R<DUMMY>(addr);
		case 0x37: return sll_xix_R<A>(addr);
		case 0x38: return srl_xix_R<B>(addr);
		case 0x39: return srl_xix_R<C>(addr);
		case 0x3a: return srl_xix_R<D>(addr);
		case 0x3b: return srl_xix_R<E>(addr);
		case 0x3c: return srl_xix_R<H>(addr);
		case 0x3d: return srl_xix_R<L>(addr);
		case 0x3e: return srl_xix_R<DUMMY>(addr);
		case 0x3f: return srl_xix_R<A>(addr);

		case 0x40: case 0x41: case 0x42: case 0x43:
		case 0x44: case 0x45: case 0x46: case 0x47:
			return bit_N_xix<0>(addr);
		case 0x48: case 0x49: case 0x4a: case 0x4b:
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
			return bit_N_xix<1>(addr);
		case 0x50: case 0x51: case 0x52: case 0x53:
		case 0x54: case 0x55: case 0x56: case 0x57:
			return bit_N_xix<2>(addr);
		case 0x58: case 0x59: case 0x5a: case 0x5b:
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:
			return bit_N_xix<3>(addr);
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x66: case 0x67:
			return bit_N_xix<4>(addr);
		case 0x68: case 0x69: case 0x6a: case 0x6b:
		case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			return bit_N_xix<5>(addr);
		case 0x70: case 0x71: case 0x72: case 0x73:
		case 0x74: case 0x75: case 0x76: case 0x77:
			return bit_N_xix<6>(addr);
		case 0x78: case 0x79: case 0x7a: case 0x7b:
		case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			return bit_N_xix<7>(addr);

		case 0x80: return res_N_xix_R<0,B>(addr);
		case 0x81: return res_N_xix_R<0,C>(addr);
		case 0x82: return res_N_xix_R<0,D>(addr);
		case 0x83: return res_N_xix_R<0,E>(addr);
		case 0x84: return res_N_xix_R<0,H>(addr);
		case 0x85: return res_N_xix_R<0,L>(addr);
		case 0x87: return res_N_xix_R<0,A>(addr);
		case 0x88: return res_N_xix_R<1,B>(addr);
		case 0x89: return res_N_xix_R<1,C>(addr);
		case 0x8a: return res_N_xix_R<1,D>(addr);
		case 0x8b: return res_N_xix_R<1,E>(addr);
		case 0x8c: return res_N_xix_R<1,H>(addr);
		case 0x8d: return res_N_xix_R<1,L>(addr);
		case 0x8f: return res_N_xix_R<1,A>(addr);
		case 0x90: return res_N_xix_R<2,B>(addr);
		case 0x91: return res_N_xix_R<2,C>(addr);
		case 0x92: return res_N_xix_R<2,D>(addr);
		case 0x93: return res_N_xix_R<2,E>(addr);
		case 0x94: return res_N_xix_R<2,H>(addr);
		case 0x95: return res_N_xix_R<2,L>(addr);
		case 0x97: return res_N_xix_R<2,A>(addr);
		case 0x98: return res_N_xix_R<3,B>(addr);
		case 0x99: return res_N_xix_R<3,C>(addr);
		case 0x9a: return res_N_xix_R<3,D>(addr);
		case 0x9b: return res_N_xix_R<3,E>(addr);
		case 0x9c: return res_N_xix_R<3,H>(addr);
		case 0x9d: return res_N_xix_R<3,L>(addr);
		case 0x9f: return res_N_xix_R<3,A>(addr);
		case 0xa0: return res_N_xix_R<4,B>(addr);
		case 0xa1: return res_N_xix_R<4,C>(addr);
		case 0xa2: return res_N_xix_R<4,D>(addr);
		case 0xa3: return res_N_xix_R<4,E>(addr);
		case 0xa4: return res_N_xix_R<4,H>(addr);
		case 0xa5: return res_N_xix_R<4,L>(addr);
		case 0xa7: return res_N_xix_R<4,A>(addr);
		case 0xa8: return res_N_xix_R<5,B>(addr);
		case 0xa9: return res_N_xix_R<5,C>(addr);
		case 0xaa: return res_N_xix_R<5,D>(addr);
		case 0xab: return res_N_xix_R<5,E>(addr);
		case 0xac: return res_N_xix_R<5,H>(addr);
		case 0xad: return res_N_xix_R<5,L>(addr);
		case 0xaf: return res_N_xix_R<5,A>(addr);
		case 0xb0: return res_N_xix_R<6,B>(addr);
		case 0xb1: return res_N_xix_R<6,C>(addr);
		case 0xb2: return res_N_xix_R<6,D>(addr);
		case 0xb3: return res_N_xix_R<6,E>(addr);
		case 0xb4: return res_N_xix_R<6,H>(addr);
		case 0xb5: return res_N_xix_R<6,L>(addr);
		case 0xb7: return res_N_xix_R<6,A>(addr);
		case 0xb8: return res_N_xix_R<7,B>(addr);
		case 0xb9: return res_N_xix_R<7,C>(addr);
		case 0xba: return res_N_xix_R<7,D>(addr);
		case 0xbb: return res_N_xix_R<7,E>(addr);
		case 0xbc: return res_N_xix_R<7,H>(addr);
		case 0xbd: return res_N_xix_R<7,L>(addr);
		case 0xbf: return res_N_xix_R<7,A>(addr);
		case 0x86: return res_N_xix_R<0,DUMMY>(addr);
		case 0x8e: return res_N_xix_R<1,DUMMY>(addr);
		case 0x96: return res_N_xix_R<2,DUMMY>(addr);
		case 0x9e: return res_N_xix_R<3,DUMMY>(addr);
		case 0xa6: return res_N_xix_R<4,DUMMY>(addr);
		case 0xae: return res_N_xix_R<5,DUMMY>(addr);
		case 0xb6: return res_N_xix_R<6,DUMMY>(addr);
		case 0xbe: return res_N_xix_R<7,DUMMY>(addr);

		case 0xc0: return set_N_xix_R<0,B>(addr);
		case 0xc1: return set_N_xix_R<0,C>(addr);
		case 0xc2: return set_N_xix_R<0,D>(addr);
		case 0xc3: return set_N_xix_R<0,E>(addr);
		case 0xc4: return set_N_xix_R<0,H>(addr);
		case 0xc5: return set_N_xix_R<0,L>(addr);
		case 0xc7: return set_N_xix_R<0,A>(addr);
		case 0xc8: return set_N_xix_R<1,B>(addr);
		case 0xc9: return set_N_xix_R<1,C>(addr);
		case 0xca: return set_N_xix_R<1,D>(addr);
		case 0xcb: return set_N_xix_R<1,E>(addr);
		case 0xcc: return set_N_xix_R<1,H>(addr);
		case 0xcd: return set_N_xix_R<1,L>(addr);
		case 0xcf: return set_N_xix_R<1,A>(addr);
		case 0xd0: return set_N_xix_R<2,B>(addr);
		case 0xd1: return set_N_xix_R<2,C>(addr);
		case 0xd2: return set_N_xix_R<2,D>(addr);
		case 0xd3: return set_N_xix_R<2,E>(addr);
		case 0xd4: return set_N_xix_R<2,H>(addr);
		case 0xd5: return set_N_xix_R<2,L>(addr);
		case 0xd7: return set_N_xix_R<2,A>(addr);
		case 0xd8: return set_N_xix_R<3,B>(addr);
		case 0xd9: return set_N_xix_R<3,C>(addr);
		case 0xda: return set_N_xix_R<3,D>(addr);
		case 0xdb: return set_N_xix_R<3,E>(addr);
		case 0xdc: return set_N_xix_R<3,H>(addr);
		case 0xdd: return set_N_xix_R<3,L>(addr);
		case 0xdf: return set_N_xix_R<3,A>(addr);
		case 0xe0: return set_N_xix_R<4,B>(addr);
		case 0xe1: return set_N_xix_R<4,C>(addr);
		case 0xe2: return set_N_xix_R<4,D>(addr);
		case 0xe3: return set_N_xix_R<4,E>(addr);
		case 0xe4: return set_N_xix_R<4,H>(addr);
		case 0xe5: return set_N_xix_R<4,L>(addr);
		case 0xe7: return set_N_xix_R<4,A>(addr);
		case 0xe8: return set_N_xix_R<5,B>(addr);
		case 0xe9: return set_N_xix_R<5,C>(addr);
		case 0xea: return set_N_xix_R<5,D>(addr);
		case 0xeb: return set_N_xix_R<5,E>(addr);
		case 0xec: return set_N_xix_R<5,H>(addr);
		case 0xed: return set_N_xix_R<5,L>(addr);
		case 0xef: return set_N_xix_R<5,A>(addr);
		case 0xf0: return set_N_xix_R<6,B>(addr);
		case 0xf1: return set_N_xix_R<6,C>(addr);
		case 0xf2: return set_N_xix_R<6,D>(addr);
		case 0xf3: return set_N_xix_R<6,E>(addr);
		case 0xf4: return set_N_xix_R<6,H>(addr);
		case 0xf5: return set_N_xix_R<6,L>(addr);
		case 0xf7: return set_N_xix_R<6,A>(addr);
		case 0xf8: return set_N_xix_R<7,B>(addr);
		case 0xf9: return set_N_xix_R<7,C>(addr);
		case 0xfa: return set_N_xix_R<7,D>(addr);
		case 0xfb: return set_N_xix_R<7,E>(addr);
		case 0xfc: return set_N_xix_R<7,H>(addr);
		case 0xfd: return set_N_xix_R<7,L>(addr);
		case 0xff: return set_N_xix_R<7,A>(addr);
		case 0xc6: return set_N_xix_R<0,DUMMY>(addr);
		case 0xce: return set_N_xix_R<1,DUMMY>(addr);
		case 0xd6: return set_N_xix_R<2,DUMMY>(addr);
		case 0xde: return set_N_xix_R<3,DUMMY>(addr);
		case 0xe6: return set_N_xix_R<4,DUMMY>(addr);
		case 0xee: return set_N_xix_R<5,DUMMY>(addr);
		case 0xf6: return set_N_xix_R<6,DUMMY>(addr);
		case 0xfe: return set_N_xix_R<7,DUMMY>(addr);
	}
	assert(false); return 0;
}

template <class T> int CPUCore<T>::cb() {
	byte opcode = RDMEM_OPCODE(T::CC_PREFIX);
	M1Cycle();
	switch (opcode) {
		case 0x00: return rlc_R<B>();
		case 0x01: return rlc_R<C>();
		case 0x02: return rlc_R<D>();
		case 0x03: return rlc_R<E>();
		case 0x04: return rlc_R<H>();
		case 0x05: return rlc_R<L>();
		case 0x07: return rlc_R<A>();
		case 0x06: return rlc_xhl();
		case 0x08: return rrc_R<B>();
		case 0x09: return rrc_R<C>();
		case 0x0a: return rrc_R<D>();
		case 0x0b: return rrc_R<E>();
		case 0x0c: return rrc_R<H>();
		case 0x0d: return rrc_R<L>();
		case 0x0f: return rrc_R<A>();
		case 0x0e: return rrc_xhl();
		case 0x10: return rl_R<B>();
		case 0x11: return rl_R<C>();
		case 0x12: return rl_R<D>();
		case 0x13: return rl_R<E>();
		case 0x14: return rl_R<H>();
		case 0x15: return rl_R<L>();
		case 0x17: return rl_R<A>();
		case 0x16: return rl_xhl();
		case 0x18: return rr_R<B>();
		case 0x19: return rr_R<C>();
		case 0x1a: return rr_R<D>();
		case 0x1b: return rr_R<E>();
		case 0x1c: return rr_R<H>();
		case 0x1d: return rr_R<L>();
		case 0x1f: return rr_R<A>();
		case 0x1e: return rr_xhl();
		case 0x20: return sla_R<B>();
		case 0x21: return sla_R<C>();
		case 0x22: return sla_R<D>();
		case 0x23: return sla_R<E>();
		case 0x24: return sla_R<H>();
		case 0x25: return sla_R<L>();
		case 0x27: return sla_R<A>();
		case 0x26: return sla_xhl();
		case 0x28: return sra_R<B>();
		case 0x29: return sra_R<C>();
		case 0x2a: return sra_R<D>();
		case 0x2b: return sra_R<E>();
		case 0x2c: return sra_R<H>();
		case 0x2d: return sra_R<L>();
		case 0x2f: return sra_R<A>();
		case 0x2e: return sra_xhl();
		case 0x30: return sll_R<B>();
		case 0x31: return sll_R<C>();
		case 0x32: return sll_R<D>();
		case 0x33: return sll_R<E>();
		case 0x34: return sll_R<H>();
		case 0x35: return sll_R<L>();
		case 0x37: return sll_R<A>();
		case 0x36: return sll_xhl();
		case 0x38: return srl_R<B>();
		case 0x39: return srl_R<C>();
		case 0x3a: return srl_R<D>();
		case 0x3b: return srl_R<E>();
		case 0x3c: return srl_R<H>();
		case 0x3d: return srl_R<L>();
		case 0x3f: return srl_R<A>();
		case 0x3e: return srl_xhl();

		case 0x40: return bit_N_R<0,B>();
		case 0x41: return bit_N_R<0,C>();
		case 0x42: return bit_N_R<0,D>();
		case 0x43: return bit_N_R<0,E>();
		case 0x44: return bit_N_R<0,H>();
		case 0x45: return bit_N_R<0,L>();
		case 0x47: return bit_N_R<0,A>();
		case 0x48: return bit_N_R<1,B>();
		case 0x49: return bit_N_R<1,C>();
		case 0x4a: return bit_N_R<1,D>();
		case 0x4b: return bit_N_R<1,E>();
		case 0x4c: return bit_N_R<1,H>();
		case 0x4d: return bit_N_R<1,L>();
		case 0x4f: return bit_N_R<1,A>();
		case 0x50: return bit_N_R<2,B>();
		case 0x51: return bit_N_R<2,C>();
		case 0x52: return bit_N_R<2,D>();
		case 0x53: return bit_N_R<2,E>();
		case 0x54: return bit_N_R<2,H>();
		case 0x55: return bit_N_R<2,L>();
		case 0x57: return bit_N_R<2,A>();
		case 0x58: return bit_N_R<3,B>();
		case 0x59: return bit_N_R<3,C>();
		case 0x5a: return bit_N_R<3,D>();
		case 0x5b: return bit_N_R<3,E>();
		case 0x5c: return bit_N_R<3,H>();
		case 0x5d: return bit_N_R<3,L>();
		case 0x5f: return bit_N_R<3,A>();
		case 0x60: return bit_N_R<4,B>();
		case 0x61: return bit_N_R<4,C>();
		case 0x62: return bit_N_R<4,D>();
		case 0x63: return bit_N_R<4,E>();
		case 0x64: return bit_N_R<4,H>();
		case 0x65: return bit_N_R<4,L>();
		case 0x67: return bit_N_R<4,A>();
		case 0x68: return bit_N_R<5,B>();
		case 0x69: return bit_N_R<5,C>();
		case 0x6a: return bit_N_R<5,D>();
		case 0x6b: return bit_N_R<5,E>();
		case 0x6c: return bit_N_R<5,H>();
		case 0x6d: return bit_N_R<5,L>();
		case 0x6f: return bit_N_R<5,A>();
		case 0x70: return bit_N_R<6,B>();
		case 0x71: return bit_N_R<6,C>();
		case 0x72: return bit_N_R<6,D>();
		case 0x73: return bit_N_R<6,E>();
		case 0x74: return bit_N_R<6,H>();
		case 0x75: return bit_N_R<6,L>();
		case 0x77: return bit_N_R<6,A>();
		case 0x78: return bit_N_R<7,B>();
		case 0x79: return bit_N_R<7,C>();
		case 0x7a: return bit_N_R<7,D>();
		case 0x7b: return bit_N_R<7,E>();
		case 0x7c: return bit_N_R<7,H>();
		case 0x7d: return bit_N_R<7,L>();
		case 0x7f: return bit_N_R<7,A>();
		case 0x46: return bit_N_xhl<0>();
		case 0x4e: return bit_N_xhl<1>();
		case 0x56: return bit_N_xhl<2>();
		case 0x5e: return bit_N_xhl<3>();
		case 0x66: return bit_N_xhl<4>();
		case 0x6e: return bit_N_xhl<5>();
		case 0x76: return bit_N_xhl<6>();
		case 0x7e: return bit_N_xhl<7>();

		case 0x80: return res_N_R<0,B>();
		case 0x81: return res_N_R<0,C>();
		case 0x82: return res_N_R<0,D>();
		case 0x83: return res_N_R<0,E>();
		case 0x84: return res_N_R<0,H>();
		case 0x85: return res_N_R<0,L>();
		case 0x87: return res_N_R<0,A>();
		case 0x88: return res_N_R<1,B>();
		case 0x89: return res_N_R<1,C>();
		case 0x8a: return res_N_R<1,D>();
		case 0x8b: return res_N_R<1,E>();
		case 0x8c: return res_N_R<1,H>();
		case 0x8d: return res_N_R<1,L>();
		case 0x8f: return res_N_R<1,A>();
		case 0x90: return res_N_R<2,B>();
		case 0x91: return res_N_R<2,C>();
		case 0x92: return res_N_R<2,D>();
		case 0x93: return res_N_R<2,E>();
		case 0x94: return res_N_R<2,H>();
		case 0x95: return res_N_R<2,L>();
		case 0x97: return res_N_R<2,A>();
		case 0x98: return res_N_R<3,B>();
		case 0x99: return res_N_R<3,C>();
		case 0x9a: return res_N_R<3,D>();
		case 0x9b: return res_N_R<3,E>();
		case 0x9c: return res_N_R<3,H>();
		case 0x9d: return res_N_R<3,L>();
		case 0x9f: return res_N_R<3,A>();
		case 0xa0: return res_N_R<4,B>();
		case 0xa1: return res_N_R<4,C>();
		case 0xa2: return res_N_R<4,D>();
		case 0xa3: return res_N_R<4,E>();
		case 0xa4: return res_N_R<4,H>();
		case 0xa5: return res_N_R<4,L>();
		case 0xa7: return res_N_R<4,A>();
		case 0xa8: return res_N_R<5,B>();
		case 0xa9: return res_N_R<5,C>();
		case 0xaa: return res_N_R<5,D>();
		case 0xab: return res_N_R<5,E>();
		case 0xac: return res_N_R<5,H>();
		case 0xad: return res_N_R<5,L>();
		case 0xaf: return res_N_R<5,A>();
		case 0xb0: return res_N_R<6,B>();
		case 0xb1: return res_N_R<6,C>();
		case 0xb2: return res_N_R<6,D>();
		case 0xb3: return res_N_R<6,E>();
		case 0xb4: return res_N_R<6,H>();
		case 0xb5: return res_N_R<6,L>();
		case 0xb7: return res_N_R<6,A>();
		case 0xb8: return res_N_R<7,B>();
		case 0xb9: return res_N_R<7,C>();
		case 0xba: return res_N_R<7,D>();
		case 0xbb: return res_N_R<7,E>();
		case 0xbc: return res_N_R<7,H>();
		case 0xbd: return res_N_R<7,L>();
		case 0xbf: return res_N_R<7,A>();
		case 0x86: return res_N_xhl<0>();
		case 0x8e: return res_N_xhl<1>();
		case 0x96: return res_N_xhl<2>();
		case 0x9e: return res_N_xhl<3>();
		case 0xa6: return res_N_xhl<4>();
		case 0xae: return res_N_xhl<5>();
		case 0xb6: return res_N_xhl<6>();
		case 0xbe: return res_N_xhl<7>();

		case 0xc0: return set_N_R<0,B>();
		case 0xc1: return set_N_R<0,C>();
		case 0xc2: return set_N_R<0,D>();
		case 0xc3: return set_N_R<0,E>();
		case 0xc4: return set_N_R<0,H>();
		case 0xc5: return set_N_R<0,L>();
		case 0xc7: return set_N_R<0,A>();
		case 0xc8: return set_N_R<1,B>();
		case 0xc9: return set_N_R<1,C>();
		case 0xca: return set_N_R<1,D>();
		case 0xcb: return set_N_R<1,E>();
		case 0xcc: return set_N_R<1,H>();
		case 0xcd: return set_N_R<1,L>();
		case 0xcf: return set_N_R<1,A>();
		case 0xd0: return set_N_R<2,B>();
		case 0xd1: return set_N_R<2,C>();
		case 0xd2: return set_N_R<2,D>();
		case 0xd3: return set_N_R<2,E>();
		case 0xd4: return set_N_R<2,H>();
		case 0xd5: return set_N_R<2,L>();
		case 0xd7: return set_N_R<2,A>();
		case 0xd8: return set_N_R<3,B>();
		case 0xd9: return set_N_R<3,C>();
		case 0xda: return set_N_R<3,D>();
		case 0xdb: return set_N_R<3,E>();
		case 0xdc: return set_N_R<3,H>();
		case 0xdd: return set_N_R<3,L>();
		case 0xdf: return set_N_R<3,A>();
		case 0xe0: return set_N_R<4,B>();
		case 0xe1: return set_N_R<4,C>();
		case 0xe2: return set_N_R<4,D>();
		case 0xe3: return set_N_R<4,E>();
		case 0xe4: return set_N_R<4,H>();
		case 0xe5: return set_N_R<4,L>();
		case 0xe7: return set_N_R<4,A>();
		case 0xe8: return set_N_R<5,B>();
		case 0xe9: return set_N_R<5,C>();
		case 0xea: return set_N_R<5,D>();
		case 0xeb: return set_N_R<5,E>();
		case 0xec: return set_N_R<5,H>();
		case 0xed: return set_N_R<5,L>();
		case 0xef: return set_N_R<5,A>();
		case 0xf0: return set_N_R<6,B>();
		case 0xf1: return set_N_R<6,C>();
		case 0xf2: return set_N_R<6,D>();
		case 0xf3: return set_N_R<6,E>();
		case 0xf4: return set_N_R<6,H>();
		case 0xf5: return set_N_R<6,L>();
		case 0xf7: return set_N_R<6,A>();
		case 0xf8: return set_N_R<7,B>();
		case 0xf9: return set_N_R<7,C>();
		case 0xfa: return set_N_R<7,D>();
		case 0xfb: return set_N_R<7,E>();
		case 0xfc: return set_N_R<7,H>();
		case 0xfd: return set_N_R<7,L>();
		case 0xff: return set_N_R<7,A>();
		case 0xc6: return set_N_xhl<0>();
		case 0xce: return set_N_xhl<1>();
		case 0xd6: return set_N_xhl<2>();
		case 0xde: return set_N_xhl<3>();
		case 0xe6: return set_N_xhl<4>();
		case 0xee: return set_N_xhl<5>();
		case 0xf6: return set_N_xhl<6>();
		case 0xfe: return set_N_xhl<7>();
	}
	assert(false); return 0;
}

template <class T> int CPUCore<T>::ed() {
	byte opcode = RDMEM_OPCODE(T::CC_PREFIX);
	M1Cycle();
	switch (opcode) {
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c: case 0x0d: case 0x0e: case 0x0f:
		case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b:
		case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x24: case 0x25: case 0x26: case 0x27:
		case 0x28: case 0x29: case 0x2a: case 0x2b:
		case 0x2c: case 0x2d: case 0x2e: case 0x2f:
		case 0x30: case 0x31: case 0x32: case 0x33:
		case 0x34: case 0x35: case 0x36: case 0x37:
		case 0x38: case 0x39: case 0x3a: case 0x3b:
		case 0x3c: case 0x3d: case 0x3e: case 0x3f:

		case 0x77: case 0x7f:

		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x90: case 0x91: case 0x92: case 0x93:
		case 0x94: case 0x95: case 0x96: case 0x97:
		case 0x98: case 0x99: case 0x9a: case 0x9b:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f:
		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xac: case 0xad: case 0xae: case 0xaf:
		case 0xb4: case 0xb5: case 0xb6: case 0xb7:
		case 0xbc: case 0xbd: case 0xbe: case 0xbf:

		case 0xc0:            case 0xc2:
		case 0xc4: case 0xc5: case 0xc6: case 0xc7:
		case 0xc8: case 0xca: case 0xcb:
		case 0xcc: case 0xcd: case 0xce: case 0xcf:
		case 0xd0:            case 0xd2: case 0xd3:
		case 0xd4: case 0xd5: case 0xd6: case 0xd7:
		case 0xd8:            case 0xda: case 0xdb:
		case 0xdc: case 0xdd: case 0xde: case 0xdf:
		case 0xe0: case 0xe1: case 0xe2: case 0xe3:
		case 0xe4: case 0xe5: case 0xe6: case 0xe7:
		case 0xe8: case 0xe9: case 0xea: case 0xeb:
		case 0xec: case 0xed: case 0xee: case 0xef:
		case 0xf0: case 0xf1: case 0xf2:
		case 0xf4: case 0xf5: case 0xf6: case 0xf7:
		case 0xf8: case 0xf9: case 0xfa: case 0xfb:
		case 0xfc: case 0xfd: case 0xfe: case 0xff:
			return nop();

		case 0x40: return in_R_c<B>();
		case 0x48: return in_R_c<C>();
		case 0x50: return in_R_c<D>();
		case 0x58: return in_R_c<E>();
		case 0x60: return in_R_c<H>();
		case 0x68: return in_R_c<L>();
		case 0x70: return in_R_c<DUMMY>();
		case 0x78: return in_R_c<A>();

		case 0x41: return out_c_R<B>();
		case 0x49: return out_c_R<C>();
		case 0x51: return out_c_R<D>();
		case 0x59: return out_c_R<E>();
		case 0x61: return out_c_R<H>();
		case 0x69: return out_c_R<L>();
		case 0x71: return out_c_R<DUMMY>();
		case 0x79: return out_c_R<A>();

		case 0x42: return sbc_hl_SS<BC>();
		case 0x52: return sbc_hl_SS<DE>();
		case 0x62: return sbc_hl_hl();
		case 0x72: return sbc_hl_SS<SP>();

		case 0x4a: return adc_hl_SS<BC>();
		case 0x5a: return adc_hl_SS<DE>();
		case 0x6a: return adc_hl_hl();
		case 0x7a: return adc_hl_SS<SP>();

		case 0x43: return ld_xword_SS_ED<BC>();
		case 0x53: return ld_xword_SS_ED<DE>();
		case 0x63: return ld_xword_SS_ED<HL>();
		case 0x73: return ld_xword_SS_ED<SP>();

		case 0x4b: return ld_SS_xword_ED<BC>();
		case 0x5b: return ld_SS_xword_ED<DE>();
		case 0x6b: return ld_SS_xword_ED<HL>();
		case 0x7b: return ld_SS_xword_ED<SP>();

		case 0x47: return ld_IR_a<REG_I>();
		case 0x4f: return ld_IR_a<REG_R>();
		case 0x57: return ld_a_IR<REG_I>();
		case 0x5f: return ld_a_IR<REG_R>();

		case 0x67: return rrd();
		case 0x6f: return rld();

		case 0x45: case 0x4d: case 0x55: case 0x5d:
		case 0x65: case 0x6d: case 0x75: case 0x7d:
			return retn();
		case 0x46: case 0x4e: case 0x66: case 0x6e:
			return im_N<0>();
		case 0x56: case 0x76:
			return im_N<1>();
		case 0x5e: case 0x7e:
			return im_N<2>();
		case 0x44: case 0x4c: case 0x54: case 0x5c:
		case 0x64: case 0x6c: case 0x74: case 0x7c:
			return neg();

		case 0xa0: return ldi();
		case 0xa1: return cpi();
		case 0xa2: return ini();
		case 0xa3: return outi();
		case 0xa8: return ldd();
		case 0xa9: return cpd();
		case 0xaa: return ind();
		case 0xab: return outd();
		case 0xb0: return ldir();
		case 0xb1: return cpir();
		case 0xb2: return inir();
		case 0xb3: return otir();
		case 0xb8: return lddr();
		case 0xb9: return cpdr();
		case 0xba: return indr();
		case 0xbb: return otdr();

		case 0xc1: return T::hasMul() ? mulub_a_R<B>() : nop();
		case 0xc9: return T::hasMul() ? mulub_a_R<C>() : nop();
		case 0xd1: return T::hasMul() ? mulub_a_R<D>() : nop();
		case 0xd9: return T::hasMul() ? mulub_a_R<E>() : nop();
		case 0xc3: return T::hasMul() ? muluw_hl_SS<BC>() : nop();
		case 0xf3: return T::hasMul() ? muluw_hl_SS<SP>() : nop();
	}
	assert(false); return 0;
}

template <class T> template<CPU::Reg16 IXY, CPU::Reg8 IXYH, CPU::Reg8 IXYL>
int CPUCore<T>::xy() {
	T::add(T::CC_DD);
	byte opcode = RDMEM_OPCODE(T::CC_MAIN);
	M1Cycle();
	switch (opcode) {
		case 0x00: // nop();
		case 0x01: // ld_bc_word();
		case 0x02: // ld_xbc_a();
		case 0x03: // inc_bc();
		case 0x04: // inc_b();
		case 0x05: // dec_b();
		case 0x06: // ld_b_byte();
		case 0x07: // rlca();
		case 0x08: // ex_af_af();
		case 0x0a: // ld_a_xbc();
		case 0x0b: // dec_bc();
		case 0x0c: // inc_c();
		case 0x0d: // dec_c();
		case 0x0e: // ld_c_byte();
		case 0x0f: // rrca();
		case 0x10: // djnz();
		case 0x11: // ld_de_word();
		case 0x12: // ld_xde_a();
		case 0x13: // inc_de();
		case 0x14: // inc_d();
		case 0x15: // dec_d();
		case 0x16: // ld_d_byte();
		case 0x17: // rla();
		case 0x18: // jr();
		case 0x1a: // ld_a_xde();
		case 0x1b: // dec_de();
		case 0x1c: // inc_e();
		case 0x1d: // dec_e();
		case 0x1e: // ld_e_byte();
		case 0x1f: // rra();
		case 0x20: // jr_nz();
		case 0x27: // daa();
		case 0x28: // jr_z();
		case 0x2f: // cpl();
		case 0x30: // jr_nc();
		case 0x31: // ld_sp_word();
		case 0x32: // ld_xbyte_a();
		case 0x33: // inc_sp();
		case 0x37: // scf();
		case 0x38: // jr_c();
		case 0x3a: // ld_a_xbyte();
		case 0x3b: // dec_sp();
		case 0x3c: // inc_a();
		case 0x3d: // dec_a();
		case 0x3e: // ld_a_byte();
		case 0x3f: // ccf();

		case 0x40: // ld_b_b();
		case 0x41: // ld_b_c();
		case 0x42: // ld_b_d();
		case 0x43: // ld_b_e();
		case 0x47: // ld_b_a();
		case 0x48: // ld_c_b();
		case 0x49: // ld_c_c();
		case 0x4a: // ld_c_d();
		case 0x4b: // ld_c_e();
		case 0x4f: // ld_c_a();
		case 0x50: // ld_d_b();
		case 0x51: // ld_d_c();
		case 0x52: // ld_d_d();
		case 0x53: // ld_d_e();
		case 0x57: // ld_d_a();
		case 0x58: // ld_e_b();
		case 0x59: // ld_e_c();
		case 0x5a: // ld_e_d();
		case 0x5b: // ld_e_e();
		case 0x5f: // ld_e_a();
		case 0x64: // ld_ixh_ixh(); == nop
		case 0x6d: // ld_ixl_ixl(); == nop
		case 0x76: // halt();
		case 0x78: // ld_a_b();
		case 0x79: // ld_a_c();
		case 0x7a: // ld_a_d();
		case 0x7b: // ld_a_e();
		case 0x7f: // ld_a_a();

		case 0x80: // add_a_b();
		case 0x81: // add_a_c();
		case 0x82: // add_a_d();
		case 0x83: // add_a_e();
		case 0x87: // add_a_a();
		case 0x88: // adc_a_b();
		case 0x89: // adc_a_c();
		case 0x8a: // adc_a_d();
		case 0x8b: // adc_a_e();
		case 0x8f: // adc_a_a();
		case 0x90: // sub_b();
		case 0x91: // sub_c();
		case 0x92: // sub_d();
		case 0x93: // sub_e();
		case 0x97: // sub_a();
		case 0x98: // sbc_a_b();
		case 0x99: // sbc_a_c();
		case 0x9a: // sbc_a_d();
		case 0x9b: // sbc_a_e();
		case 0x9f: // sbc_a_a();
		case 0xa0: // and_b();
		case 0xa1: // and_c();
		case 0xa2: // and_d();
		case 0xa3: // and_e();
		case 0xa7: // and_a();
		case 0xa8: // xor_b();
		case 0xa9: // xor_c();
		case 0xaa: // xor_d();
		case 0xab: // xor_e();
		case 0xaf: // xor_a();
		case 0xb0: // or_b();
		case 0xb1: // or_c();
		case 0xb2: // or_d();
		case 0xb3: // or_e();
		case 0xb7: // or_a();
		case 0xb8: // cp_b();
		case 0xb9: // cp_c();
		case 0xba: // cp_d();
		case 0xbb: // cp_e();
		case 0xbf: // cp_a();

		case 0xc0: // ret_nz();
		case 0xc1: // pop_bc();
		case 0xc2: // jp_nz();
		case 0xc3: // jp();
		case 0xc4: // call_nz();
		case 0xc5: // push_bc();
		case 0xc6: // add_a_byte();
		case 0xc7: // rst_00();
		case 0xc8: // ret_z();
		case 0xc9: // ret();
		case 0xca: // jp_z();
		case 0xcc: // call_z();
		case 0xcd: // call();
		case 0xce: // adc_a_byte();
		case 0xcf: // rst_08();
		case 0xd0: // ret_nc();
		case 0xd1: // pop_de();
		case 0xd2: // jp_nc();
		case 0xd3: // out_byte_a();
		case 0xd4: // call_nc();
		case 0xd5: // push_de();
		case 0xd6: // sub_byte();
		case 0xd7: // rst_10();
		case 0xd8: // ret_c();
		case 0xd9: // exx();
		case 0xda: // jp_c();
		case 0xdb: // in_a_byte();
		case 0xdc: // call_c();
		case 0xde: // sbc_a_byte();
		case 0xdf: // rst_18();
		case 0xe0: // ret_po();
		case 0xe2: // jp_po();
		case 0xe4: // call_po();
		case 0xe6: // and_byte();
		case 0xe7: // rst_20();
		case 0xe8: // ret_pe();
		case 0xea: // jp_pe();
		case 0xeb: // ex_de_hl();
		case 0xec: // call_pe();
		case 0xed: // ed();
		case 0xee: // xor_byte();
		case 0xef: // rst_28();
		case 0xf0: // ret_p();
		case 0xf1: // pop_af();
		case 0xf2: // jp_p();
		case 0xf3: // di();
		case 0xf4: // call_p();
		case 0xf5: // push_af();
		case 0xf6: // or_byte();
		case 0xf7: // rst_30();
		case 0xf8: // ret_m();
		case 0xfa: // jp_m();
		case 0xfb: // ei();
		case 0xfc: // call_m();
		case 0xfe: // cp_byte();
		case 0xff: // rst_38();
			return executeInstruction1_slow(opcode);

		case 0x09: return add_SS_TT<IXY,BC>();
		case 0x19: return add_SS_TT<IXY,DE>();
		case 0x21: return ld_SS_word<IXY>();
		case 0x22: return ld_xword_SS<IXY>();
		case 0x23: return inc_SS<IXY>();
		case 0x24: return inc_R<IXYH>();
		case 0x25: return dec_R<IXYH>();
		case 0x26: return ld_R_byte<IXYH>();
		case 0x29: return add_SS_SS<IXY>();
		case 0x2a: return ld_SS_xword<IXY>();
		case 0x2b: return dec_SS<IXY>();
		case 0x2c: return inc_R<IXYL>();
		case 0x2d: return dec_R<IXYL>();
		case 0x2e: return ld_R_byte<IXYL>();
		case 0x34: return inc_xix<IXY>();
		case 0x35: return dec_xix<IXY>();
		case 0x36: return ld_xix_byte<IXY>();
		case 0x39: return add_SS_TT<IXY,SP>();

		case 0x44: return ld_R_R<B,IXYH>();
		case 0x45: return ld_R_R<B,IXYL>();
		case 0x4c: return ld_R_R<C,IXYH>();
		case 0x4d: return ld_R_R<C,IXYL>();
		case 0x54: return ld_R_R<D,IXYH>();
		case 0x55: return ld_R_R<D,IXYL>();
		case 0x5c: return ld_R_R<E,IXYH>();
		case 0x5d: return ld_R_R<E,IXYL>();
		case 0x7c: return ld_R_R<A,IXYH>();
		case 0x7d: return ld_R_R<A,IXYL>();
		case 0x60: return ld_R_R<IXYH,B>();
		case 0x61: return ld_R_R<IXYH,C>();
		case 0x62: return ld_R_R<IXYH,D>();
		case 0x63: return ld_R_R<IXYH,E>();
		case 0x65: return ld_R_R<IXYH,IXL>();
		case 0x67: return ld_R_R<IXYH,A>();
		case 0x68: return ld_R_R<IXYL,B>();
		case 0x69: return ld_R_R<IXYL,C>();
		case 0x6a: return ld_R_R<IXYL,D>();
		case 0x6b: return ld_R_R<IXYL,E>();
		case 0x6c: return ld_R_R<IXYL,IXH>();
		case 0x6f: return ld_R_R<IXYL,A>();
		case 0x70: return ld_xix_R<IXY,B>();
		case 0x71: return ld_xix_R<IXY,C>();
		case 0x72: return ld_xix_R<IXY,D>();
		case 0x73: return ld_xix_R<IXY,E>();
		case 0x74: return ld_xix_R<IXY,H>();
		case 0x75: return ld_xix_R<IXY,L>();
		case 0x77: return ld_xix_R<IXY,A>();
		case 0x46: return ld_R_xix<B,IXY>();
		case 0x4e: return ld_R_xix<C,IXY>();
		case 0x56: return ld_R_xix<D,IXY>();
		case 0x5e: return ld_R_xix<E,IXY>();
		case 0x66: return ld_R_xix<H,IXY>();
		case 0x6e: return ld_R_xix<L,IXY>();
		case 0x7e: return ld_R_xix<A,IXY>();

		case 0x84: return add_a_R<IXYH>();
		case 0x85: return add_a_R<IXYL>();
		case 0x86: return add_a_xix<IXY>();
		case 0x8c: return adc_a_R<IXYH>();
		case 0x8d: return adc_a_R<IXYL>();
		case 0x8e: return adc_a_xix<IXY>();
		case 0x94: return sub_R<IXYH>();
		case 0x95: return sub_R<IXYL>();
		case 0x96: return sub_xix<IXY>();
		case 0x9c: return sbc_a_R<IXYH>();
		case 0x9d: return sbc_a_R<IXYL>();
		case 0x9e: return sbc_a_xix<IXY>();
		case 0xa4: return and_R<IXYH>();
		case 0xa5: return and_R<IXYL>();
		case 0xa6: return and_xix<IXY>();
		case 0xac: return xor_R<IXYH>();
		case 0xad: return xor_R<IXYL>();
		case 0xae: return xor_xix<IXY>();
		case 0xb4: return or_R<IXYH>();
		case 0xb5: return or_R<IXYL>();
		case 0xb6: return or_xix<IXY>();
		case 0xbc: return cp_R<IXYH>();
		case 0xbd: return cp_R<IXYL>();
		case 0xbe: return cp_xix<IXY>();

		case 0xe1: return pop_SS<IXY>();
		case 0xe3: return ex_xsp_SS<IXY>();
		case 0xe5: return push_SS<IXY>();
		case 0xe9: return jp_SS<IXY>();
		case 0xf9: return ld_sp_SS<IXY>();
		case 0xcb: return xy_cb<IXY>();
		case 0xdd: return xy<IX, IXH, IXL>();
		case 0xfd: return xy<IY, IYH, IYL>();
	}
	assert(false); return 0;
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

} // namespace openmsx

