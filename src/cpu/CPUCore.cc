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
	T::PRE_IO(port);
	EmuTime time = T::getTimeFast();
	scheduler.schedule(time);
	byte result = interface->readIO(port, time);
	T::POST_IO(port);
	return result;
}

template <class T> inline void CPUCore<T>::WRITE_PORT(unsigned port, byte value, unsigned cc)
{
	memptr = port + 1; // not 16-bit
	T::PRE_IO(port);
	EmuTime time = T::getTimeFast();
	scheduler.schedule(time);
	interface->writeIO(port, value, time);
	T::POST_IO(port);
}

template <class T> byte CPUCore<T>::RDMEM_OPCODEslow(unsigned address, unsigned cc)
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
template <class T> ALWAYS_INLINE byte CPUCore<T>::RDMEM_OPCODE(unsigned cc)
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
		return RDMEM_OPCODEslow(address, cc); // not inlined
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

template <class T> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD_PC(unsigned cc)
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
		return RD_WORD_PC_slow(cc);
	}
}

template <class T> unsigned CPUCore<T>::RD_WORD_PC_slow(unsigned cc)
{
	unsigned res = RDMEM_OPCODE(cc);
	res         += RDMEM_OPCODE(cc + T::CC_MEM) << 8;
	return res;
}

template <class T> byte CPUCore<T>::RDMEMslow(unsigned address, unsigned cc)
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
template <class T> ALWAYS_INLINE byte CPUCore<T>::RDMEM(unsigned address, unsigned cc)
{
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(line != NULL)) {
		// cached, fast path
		T::PRE_RDMEM(address);
		T::POST_MEM(address);
		return line[address];
	} else {
		return RDMEMslow(address, cc); // not inlined
	}
}

template <class T> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD(unsigned address, unsigned cc)
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
		return RD_WORD_slow(address, cc);
	}
}

template <class T> unsigned CPUCore<T>::RD_WORD_slow(unsigned address, unsigned cc)
{
	unsigned res = RDMEM(address, cc);
	res         += RDMEM((address + 1) & 0xFFFF, cc + T::CC_MEM) << 8;
	return res;
}

template <class T> void CPUCore<T>::WRMEMslow(unsigned address, byte value, unsigned cc)
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
template <class T> ALWAYS_INLINE void CPUCore<T>::WRMEM(
	unsigned address, byte value, unsigned cc)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(line != NULL)) {
		// cached, fast path
		T::PRE_WRMEM(address);
		T::POST_MEM(address);
		line[address] = value;
	} else {
		WRMEMslow(address, value, cc);	// not inlined
	}
}

template <class T> ALWAYS_INLINE void CPUCore<T>::WR_WORD(
	unsigned address, unsigned value, unsigned cc)
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
		WR_WORD_slow(address, value, cc);
	}
}

template <class T> void CPUCore<T>::WR_WORD_slow(
	unsigned address, unsigned value, unsigned cc)
{
	WRMEM( address,               value & 255, cc);
	WRMEM((address + 1) & 0xFFFF, value >> 8,  cc + T::CC_MEM);
}

template <class T> ALWAYS_INLINE void CPUCore<T>::WR_WORD_rev(
	unsigned address, unsigned value, unsigned cc)
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
		WR_WORD_rev_slow(address, value, cc);
	}
}

// same as WR_WORD, but writes high byte first
template <class T> void CPUCore<T>::WR_WORD_rev_slow(
	unsigned address, unsigned value, unsigned cc)
{
	WRMEM((address + 1) & 0xFFFF, value >> 8,  cc);
	WRMEM( address,               value & 255, cc + T::CC_MEM);
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
	T::end(T::CC_NMI);
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
	T::end(T::CC_IRQ0);
}

// IM1 interrupt
template <class T> inline void CPUCore<T>::irq1()
{
	M1Cycle(); // see note in irq0()
	R.setHALT(false);
	R.di();
	PUSH(R.getPC(), T::EE_IRQ1_1);
	R.setPC(0x0038);
	T::end(T::CC_IRQ1);
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
	T::end(T::CC_IRQ2);
}

template <class T> ALWAYS_INLINE void CPUCore<T>::executeInstruction1(byte opcode)
{
	M1Cycle();
	switch (opcode) {
		case 0x00: nop(); break;
		case 0x01: ld_bc_word(); break;
		case 0x02: ld_xbc_a(); break;
		case 0x03: inc_bc(); break;
		case 0x04: inc_b(); break;
		case 0x05: dec_b(); break;
		case 0x06: ld_b_byte(); break;
		case 0x07: rlca(); break;
		case 0x08: ex_af_af(); break;
		case 0x09: add_hl_bc(); break;
		case 0x0a: ld_a_xbc(); break;
		case 0x0b: dec_bc(); break;
		case 0x0c: inc_c(); break;
		case 0x0d: dec_c(); break;
		case 0x0e: ld_c_byte(); break;
		case 0x0f: rrca(); break;
		case 0x10: djnz(); break;
		case 0x11: ld_de_word(); break;
		case 0x12: ld_xde_a(); break;
		case 0x13: inc_de(); break;
		case 0x14: inc_d(); break;
		case 0x15: dec_d(); break;
		case 0x16: ld_d_byte(); break;
		case 0x17: rla(); break;
		case 0x18: jr(); break;
		case 0x19: add_hl_de(); break;
		case 0x1a: ld_a_xde(); break;
		case 0x1b: dec_de(); break;
		case 0x1c: inc_e(); break;
		case 0x1d: dec_e(); break;
		case 0x1e: ld_e_byte(); break;
		case 0x1f: rra(); break;
		case 0x20: jr_nz(); break;
		case 0x21: ld_hl_word(); break;
		case 0x22: ld_xword_hl(); break;
		case 0x23: inc_hl(); break;
		case 0x24: inc_h(); break;
		case 0x25: dec_h(); break;
		case 0x26: ld_h_byte(); break;
		case 0x27: daa(); break;
		case 0x28: jr_z(); break;
		case 0x29: add_hl_hl(); break;
		case 0x2a: ld_hl_xword(); break;
		case 0x2b: dec_hl(); break;
		case 0x2c: inc_l(); break;
		case 0x2d: dec_l(); break;
		case 0x2e: ld_l_byte(); break;
		case 0x2f: cpl(); break;
		case 0x30: jr_nc(); break;
		case 0x31: ld_sp_word(); break;
		case 0x32: ld_xbyte_a(); break;
		case 0x33: inc_sp(); break;
		case 0x34: inc_xhl(); break;
		case 0x35: dec_xhl(); break;
		case 0x36: ld_xhl_byte(); break;
		case 0x37: scf(); break;
		case 0x38: jr_c(); break;
		case 0x39: add_hl_sp(); break;
		case 0x3a: ld_a_xbyte(); break;
		case 0x3b: dec_sp(); break;
		case 0x3c: inc_a(); break;
		case 0x3d: dec_a(); break;
		case 0x3e: ld_a_byte(); break;
		case 0x3f: ccf(); break;

		case 0x40: nop(); break;
		case 0x41: ld_b_c(); break;
		case 0x42: ld_b_d(); break;
		case 0x43: ld_b_e(); break;
		case 0x44: ld_b_h(); break;
		case 0x45: ld_b_l(); break;
		case 0x46: ld_b_xhl(); break;
		case 0x47: ld_b_a(); break;
		case 0x48: ld_c_b(); break;
		case 0x49: nop(); break;
		case 0x4a: ld_c_d(); break;
		case 0x4b: ld_c_e(); break;
		case 0x4c: ld_c_h(); break;
		case 0x4d: ld_c_l(); break;
		case 0x4e: ld_c_xhl(); break;
		case 0x4f: ld_c_a(); break;
		case 0x50: ld_d_b(); break;
		case 0x51: ld_d_c(); break;
		case 0x52: nop(); break;
		case 0x53: ld_d_e(); break;
		case 0x54: ld_d_h(); break;
		case 0x55: ld_d_l(); break;
		case 0x56: ld_d_xhl(); break;
		case 0x57: ld_d_a(); break;
		case 0x58: ld_e_b(); break;
		case 0x59: ld_e_c(); break;
		case 0x5a: ld_e_d(); break;
		case 0x5b: nop(); break;
		case 0x5c: ld_e_h(); break;
		case 0x5d: ld_e_l(); break;
		case 0x5e: ld_e_xhl(); break;
		case 0x5f: ld_e_a(); break;
		case 0x60: ld_h_b(); break;
		case 0x61: ld_h_c(); break;
		case 0x62: ld_h_d(); break;
		case 0x63: ld_h_e(); break;
		case 0x64: nop(); break;
		case 0x65: ld_h_l(); break;
		case 0x66: ld_h_xhl(); break;
		case 0x67: ld_h_a(); break;
		case 0x68: ld_l_b(); break;
		case 0x69: ld_l_c(); break;
		case 0x6a: ld_l_d(); break;
		case 0x6b: ld_l_e(); break;
		case 0x6c: ld_l_h(); break;
		case 0x6d: nop(); break;
		case 0x6e: ld_l_xhl(); break;
		case 0x6f: ld_l_a(); break;
		case 0x70: ld_xhl_b(); break;
		case 0x71: ld_xhl_c(); break;
		case 0x72: ld_xhl_d(); break;
		case 0x73: ld_xhl_e(); break;
		case 0x74: ld_xhl_h(); break;
		case 0x75: ld_xhl_l(); break;
		case 0x76: halt(); break;
		case 0x77: ld_xhl_a(); break;
		case 0x78: ld_a_b(); break;
		case 0x79: ld_a_c(); break;
		case 0x7a: ld_a_d(); break;
		case 0x7b: ld_a_e(); break;
		case 0x7c: ld_a_h(); break;
		case 0x7d: ld_a_l(); break;
		case 0x7e: ld_a_xhl(); break;
		case 0x7f: nop(); break;

		case 0x80: add_a_b(); break;
		case 0x81: add_a_c(); break;
		case 0x82: add_a_d(); break;
		case 0x83: add_a_e(); break;
		case 0x84: add_a_h(); break;
		case 0x85: add_a_l(); break;
		case 0x86: add_a_xhl(); break;
		case 0x87: add_a_a(); break;
		case 0x88: adc_a_b(); break;
		case 0x89: adc_a_c(); break;
		case 0x8a: adc_a_d(); break;
		case 0x8b: adc_a_e(); break;
		case 0x8c: adc_a_h(); break;
		case 0x8d: adc_a_l(); break;
		case 0x8e: adc_a_xhl(); break;
		case 0x8f: adc_a_a(); break;
		case 0x90: sub_b(); break;
		case 0x91: sub_c(); break;
		case 0x92: sub_d(); break;
		case 0x93: sub_e(); break;
		case 0x94: sub_h(); break;
		case 0x95: sub_l(); break;
		case 0x96: sub_xhl(); break;
		case 0x97: sub_a(); break;
		case 0x98: sbc_a_b(); break;
		case 0x99: sbc_a_c(); break;
		case 0x9a: sbc_a_d(); break;
		case 0x9b: sbc_a_e(); break;
		case 0x9c: sbc_a_h(); break;
		case 0x9d: sbc_a_l(); break;
		case 0x9e: sbc_a_xhl(); break;
		case 0x9f: sbc_a_a(); break;
		case 0xa0: and_b(); break;
		case 0xa1: and_c(); break;
		case 0xa2: and_d(); break;
		case 0xa3: and_e(); break;
		case 0xa4: and_h(); break;
		case 0xa5: and_l(); break;
		case 0xa6: and_xhl(); break;
		case 0xa7: and_a(); break;
		case 0xa8: xor_b(); break;
		case 0xa9: xor_c(); break;
		case 0xaa: xor_d(); break;
		case 0xab: xor_e(); break;
		case 0xac: xor_h(); break;
		case 0xad: xor_l(); break;
		case 0xae: xor_xhl(); break;
		case 0xaf: xor_a(); break;
		case 0xb0: or_b(); break;
		case 0xb1: or_c(); break;
		case 0xb2: or_d(); break;
		case 0xb3: or_e(); break;
		case 0xb4: or_h(); break;
		case 0xb5: or_l(); break;
		case 0xb6: or_xhl(); break;
		case 0xb7: or_a(); break;
		case 0xb8: cp_b(); break;
		case 0xb9: cp_c(); break;
		case 0xba: cp_d(); break;
		case 0xbb: cp_e(); break;
		case 0xbc: cp_h(); break;
		case 0xbd: cp_l(); break;
		case 0xbe: cp_xhl(); break;
		case 0xbf: cp_a(); break;

		case 0xc0: ret_nz(); break;
		case 0xc1: pop_bc(); break;
		case 0xc2: jp_nz(); break;
		case 0xc3: jp(); break;
		case 0xc4: call_nz(); break;
		case 0xc5: push_bc(); break;
		case 0xc6: add_a_byte(); break;
		case 0xc7: rst_00(); break;
		case 0xc8: ret_z(); break;
		case 0xc9: ret(); break;
		case 0xca: jp_z(); break;
		case 0xcb: cb(); break;
		case 0xcc: call_z(); break;
		case 0xcd: call(); break;
		case 0xce: adc_a_byte(); break;
		case 0xcf: rst_08(); break;
		case 0xd0: ret_nc(); break;
		case 0xd1: pop_de(); break;
		case 0xd2: jp_nc(); break;
		case 0xd3: out_byte_a(); break;
		case 0xd4: call_nc(); break;
		case 0xd5: push_de(); break;
		case 0xd6: sub_byte(); break;
		case 0xd7: rst_10(); break;
		case 0xd8: ret_c(); break;
		case 0xd9: exx(); break;
		case 0xda: jp_c(); break;
		case 0xdb: in_a_byte(); break;
		case 0xdc: call_c(); break;
		case 0xdd: dd(); break;
		case 0xde: sbc_a_byte(); break;
		case 0xdf: rst_18(); break;
		case 0xe0: ret_po(); break;
		case 0xe1: pop_hl(); break;
		case 0xe2: jp_po(); break;
		case 0xe3: ex_xsp_hl(); break;
		case 0xe4: call_po(); break;
		case 0xe5: push_hl(); break;
		case 0xe6: and_byte(); break;
		case 0xe7: rst_20(); break;
		case 0xe8: ret_pe(); break;
		case 0xe9: jp_hl(); break;
		case 0xea: jp_pe(); break;
		case 0xeb: ex_de_hl(); break;
		case 0xec: call_pe(); break;
		case 0xed: ed(); break;
		case 0xee: xor_byte(); break;
		case 0xef: rst_28(); break;
		case 0xf0: ret_p(); break;
		case 0xf1: pop_af(); break;
		case 0xf2: jp_p(); break;
		case 0xf3: di(); break;
		case 0xf4: call_p(); break;
		case 0xf5: push_af(); break;
		case 0xf6: or_byte(); break;
		case 0xf7: rst_30(); break;
		case 0xf8: ret_m(); break;
		case 0xf9: ld_sp_hl(); break;
		case 0xfa: jp_m(); break;
		case 0xfb: ei(); break;
		case 0xfc: call_m(); break;
		case 0xfd: fd(); break;
		case 0xfe: cp_byte(); break;
		case 0xff: rst_38(); break;
	}
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
	executeInstruction1(opcode);
}

template <class T> ALWAYS_INLINE void CPUCore<T>::executeFastInline()
{
	T::R800Refresh();
	byte opcode = RDMEM_OPCODE(T::CC_MAIN);
	executeInstruction1(opcode);
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
template <class T> inline bool CPUCore<T>::C()  { return R.getF() & C_FLAG; }
template <class T> inline bool CPUCore<T>::NC() { return !C(); }
template <class T> inline bool CPUCore<T>::Z()  { return R.getF() & Z_FLAG; }
template <class T> inline bool CPUCore<T>::NZ() { return !Z(); }
template <class T> inline bool CPUCore<T>::M()  { return R.getF() & S_FLAG; }
template <class T> inline bool CPUCore<T>::P()  { return !M(); }
template <class T> inline bool CPUCore<T>::PE() { return R.getF() & V_FLAG; }
template <class T> inline bool CPUCore<T>::PO() { return !PE(); }


// LD r,r
template <class T> void CPUCore<T>::ld_a_b()     { R.setA(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_c()     { R.setA(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_d()     { R.setA(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_e()     { R.setA(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_h()     { R.setA(R.getH()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_l()     { R.setA(R.getL()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_ixh()   { R.setA(R.getIXh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_ixl()   { R.setA(R.getIXl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_iyh()   { R.setA(R.getIYh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_a_iyl()   { R.setA(R.getIYl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_a()     { R.setB(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_c()     { R.setB(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_d()     { R.setB(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_e()     { R.setB(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_h()     { R.setB(R.getH()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_l()     { R.setB(R.getL()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_ixh()   { R.setB(R.getIXh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_ixl()   { R.setB(R.getIXl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_iyh()   { R.setB(R.getIYh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_b_iyl()   { R.setB(R.getIYl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_a()     { R.setC(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_b()     { R.setC(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_d()     { R.setC(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_e()     { R.setC(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_h()     { R.setC(R.getH()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_l()     { R.setC(R.getL()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_ixh()   { R.setC(R.getIXh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_ixl()   { R.setC(R.getIXl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_iyh()   { R.setC(R.getIYh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_c_iyl()   { R.setC(R.getIYl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_a()     { R.setD(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_c()     { R.setD(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_b()     { R.setD(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_e()     { R.setD(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_h()     { R.setD(R.getH()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_l()     { R.setD(R.getL()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_ixh()   { R.setD(R.getIXh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_ixl()   { R.setD(R.getIXl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_iyh()   { R.setD(R.getIYh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_d_iyl()   { R.setD(R.getIYl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_a()     { R.setE(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_c()     { R.setE(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_b()     { R.setE(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_d()     { R.setE(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_h()     { R.setE(R.getH()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_l()     { R.setE(R.getL()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_ixh()   { R.setE(R.getIXh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_ixl()   { R.setE(R.getIXl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_iyh()   { R.setE(R.getIYh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_e_iyl()   { R.setE(R.getIYl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_h_a()     { R.setH(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_h_c()     { R.setH(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_h_b()     { R.setH(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_h_e()     { R.setH(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_h_d()     { R.setH(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_h_l()     { R.setH(R.getL()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_l_a()     { R.setL(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_l_c()     { R.setL(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_l_b()     { R.setL(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_l_e()     { R.setL(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_l_d()     { R.setL(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_l_h()     { R.setL(R.getH()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixh_a()   { R.setIXh(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixh_b()   { R.setIXh(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixh_c()   { R.setIXh(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixh_d()   { R.setIXh(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixh_e()   { R.setIXh(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixh_ixl() { R.setIXh(R.getIXl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixl_a()   { R.setIXl(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixl_b()   { R.setIXl(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixl_c()   { R.setIXl(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixl_d()   { R.setIXl(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixl_e()   { R.setIXl(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_ixl_ixh() { R.setIXl(R.getIXh()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyh_a()   { R.setIYh(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyh_b()   { R.setIYh(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyh_c()   { R.setIYh(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyh_d()   { R.setIYh(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyh_e()   { R.setIYh(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyh_iyl() { R.setIYh(R.getIYl()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyl_a()   { R.setIYl(R.getA()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyl_b()   { R.setIYl(R.getB()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyl_c()   { R.setIYl(R.getC()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyl_d()   { R.setIYl(R.getD()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyl_e()   { R.setIYl(R.getE()); T::end(T::CC_LD_R_R); }
template <class T> void CPUCore<T>::ld_iyl_iyh() { R.setIYl(R.getIYh()); T::end(T::CC_LD_R_R); }

// LD SP,ss
template <class T> void CPUCore<T>::ld_sp_hl() {
	R.setSP(R.getHL()); T::end(T::CC_LD_SP_HL);
}
template <class T> void CPUCore<T>::ld_sp_ix() {
	R.setSP(R.getIX()); T::end(T::CC_LD_SP_HL);
}
template <class T> void CPUCore<T>::ld_sp_iy() {
	R.setSP(R.getIY()); T::end(T::CC_LD_SP_HL);
}

// LD (ss),a
template <class T> inline void CPUCore<T>::WR_X_A(unsigned x)
{
	WRMEM(x, R.getA(), T::CC_LD_SS_A_1);
	T::end(T::CC_LD_SS_A);
}
template <class T> void CPUCore<T>::ld_xbc_a() { WR_X_A(R.getBC()); }
template <class T> void CPUCore<T>::ld_xde_a() { WR_X_A(R.getDE()); }

// LD (HL),r
template <class T> inline void CPUCore<T>::WR_HL_X(byte val)
{
	WRMEM(R.getHL(), val, T::CC_LD_HL_R_1);
	T::end(T::CC_LD_HL_R);
}
template <class T> void CPUCore<T>::ld_xhl_a() { WR_HL_X(R.getA()); }
template <class T> void CPUCore<T>::ld_xhl_b() { WR_HL_X(R.getB()); }
template <class T> void CPUCore<T>::ld_xhl_c() { WR_HL_X(R.getC()); }
template <class T> void CPUCore<T>::ld_xhl_d() { WR_HL_X(R.getD()); }
template <class T> void CPUCore<T>::ld_xhl_e() { WR_HL_X(R.getE()); }
template <class T> void CPUCore<T>::ld_xhl_h() { WR_HL_X(R.getH()); }
template <class T> void CPUCore<T>::ld_xhl_l() { WR_HL_X(R.getL()); }

// LD (HL),n
template <class T> void CPUCore<T>::ld_xhl_byte()
{
	byte val = RDMEM_OPCODE(T::CC_LD_HL_N_1);
	WRMEM(R.getHL(), val, T::CC_LD_HL_N_2);
	T::end(T::CC_LD_HL_N);
}

// LD (IX+e),r
template <class T> inline void CPUCore<T>::WR_XIX(byte val)
{
	offset ofst = RDMEM_OPCODE(T::CC_LD_XIX_R_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_R_2);
	T::end(T::CC_LD_XIX_R);
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
	offset ofst = RDMEM_OPCODE(T::CC_LD_XIX_R_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_R_2);
	T::end(T::CC_LD_XIX_R);
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
	unsigned tmp = RD_WORD_PC(T::CC_LD_XIX_N_1);
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	memptr = (R.getIX() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_N_2);
	T::end(T::CC_LD_XIX_N);
}

// LD (IY+e),n
template <class T> void CPUCore<T>::ld_xiy_byte()
{
	unsigned tmp = RD_WORD_PC(T::CC_LD_XIX_N_1);
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	memptr = (R.getIY() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_N_2);
	T::end(T::CC_LD_XIX_N);
}

// LD (nn),A
template <class T> void CPUCore<T>::ld_xbyte_a()
{
	unsigned x = RD_WORD_PC(T::CC_LD_NN_A_1);
	memptr = R.getA() << 8;
	WRMEM(x, R.getA(), T::CC_LD_NN_A_2);
	T::end(T::CC_LD_NN_A);
}

// LD (nn),ss
template <class T> inline void CPUCore<T>::WR_NN_Y(unsigned reg, int ee)
{
	memptr = RD_WORD_PC(T::CC_LD_XX_HL_1 + ee);
	WR_WORD(memptr, reg, T::CC_LD_XX_HL_2 + ee);
	T::end(T::CC_LD_XX_HL + ee);
}
template <class T> void CPUCore<T>::ld_xword_hl()  { WR_NN_Y(R.getHL(), 0); }
template <class T> void CPUCore<T>::ld_xword_ix()  { WR_NN_Y(R.getIX(), 0); }
template <class T> void CPUCore<T>::ld_xword_iy()  { WR_NN_Y(R.getIY(), 0); }
template <class T> void CPUCore<T>::ld_xword_bc2() { WR_NN_Y(R.getBC(), T::EE_ED); }
template <class T> void CPUCore<T>::ld_xword_de2() { WR_NN_Y(R.getDE(), T::EE_ED); }
template <class T> void CPUCore<T>::ld_xword_hl2() { WR_NN_Y(R.getHL(), T::EE_ED); }
template <class T> void CPUCore<T>::ld_xword_sp2() { WR_NN_Y(R.getSP(), T::EE_ED); }

// LD A,(ss)
template <class T> void CPUCore<T>::ld_a_xbc() { R.setA(RDMEM(R.getBC(), T::CC_LD_A_SS_1)); T::end(T::CC_LD_A_SS); }
template <class T> void CPUCore<T>::ld_a_xde() { R.setA(RDMEM(R.getDE(), T::CC_LD_A_SS_1)); T::end(T::CC_LD_A_SS); }

// LD A,(nn)
template <class T> void CPUCore<T>::ld_a_xbyte()
{
	unsigned addr = RD_WORD_PC(T::CC_LD_A_NN_1);
	memptr = addr + 1;
	R.setA(RDMEM(addr, T::CC_LD_A_NN_2));
	T::end(T::CC_LD_A_NN);
}

// LD r,n
template <class T> void CPUCore<T>::ld_a_byte()   { R.setA  (RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_b_byte()   { R.setB  (RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_c_byte()   { R.setC  (RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_d_byte()   { R.setD  (RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_e_byte()   { R.setE  (RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_h_byte()   { R.setH  (RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_l_byte()   { R.setL  (RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_ixh_byte() { R.setIXh(RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_ixl_byte() { R.setIXl(RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_iyh_byte() { R.setIYh(RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }
template <class T> void CPUCore<T>::ld_iyl_byte() { R.setIYl(RDMEM_OPCODE(T::CC_LD_R_N_1)); T::end(T::CC_LD_R_N); }

// LD r,(hl)
template <class T> void CPUCore<T>::ld_a_xhl() { R.setA(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); T::end(T::CC_LD_R_HL); }
template <class T> void CPUCore<T>::ld_b_xhl() { R.setB(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); T::end(T::CC_LD_R_HL); }
template <class T> void CPUCore<T>::ld_c_xhl() { R.setC(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); T::end(T::CC_LD_R_HL); }
template <class T> void CPUCore<T>::ld_d_xhl() { R.setD(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); T::end(T::CC_LD_R_HL); }
template <class T> void CPUCore<T>::ld_e_xhl() { R.setE(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); T::end(T::CC_LD_R_HL); }
template <class T> void CPUCore<T>::ld_h_xhl() { R.setH(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); T::end(T::CC_LD_R_HL); }
template <class T> void CPUCore<T>::ld_l_xhl() { R.setL(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); T::end(T::CC_LD_R_HL); }

// LD r,(IX+e)
template <class T> inline byte CPUCore<T>::RD_R_XIX()
{
	offset ofst = RDMEM_OPCODE(T::CC_LD_R_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	byte result = RDMEM(memptr, T::CC_LD_R_XIX_2);
	T::end(T::CC_LD_R_XIX);
	return result;
}
template <class T> void CPUCore<T>::ld_a_xix() { R.setA(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_b_xix() { R.setB(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_c_xix() { R.setC(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_d_xix() { R.setD(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_e_xix() { R.setE(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_h_xix() { R.setH(RD_R_XIX()); }
template <class T> void CPUCore<T>::ld_l_xix() { R.setL(RD_R_XIX()); }

// LD r,(IY+e)
template <class T> inline byte CPUCore<T>::RD_R_XIY()
{
	offset ofst = RDMEM_OPCODE(T::CC_LD_R_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	byte result = RDMEM(memptr, T::CC_LD_R_XIX_2);
	T::end(T::CC_LD_R_XIX);
	return result;
}
template <class T> void CPUCore<T>::ld_a_xiy() { R.setA(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_b_xiy() { R.setB(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_c_xiy() { R.setC(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_d_xiy() { R.setD(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_e_xiy() { R.setE(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_h_xiy() { R.setH(RD_R_XIY()); }
template <class T> void CPUCore<T>::ld_l_xiy() { R.setL(RD_R_XIY()); }

// LD ss,(nn)
template <class T> inline unsigned CPUCore<T>::RD_P_XX(int ee)
{
	unsigned addr = RD_WORD_PC(T::CC_LD_HL_XX_1 + ee);
	memptr = addr + 1; // not 16-bit
	unsigned result = RD_WORD(addr, T::CC_LD_HL_XX_2 + ee);
	T::end(T::CC_LD_HL_XX + ee);
	return result;
}
template <class T> void CPUCore<T>::ld_hl_xword()  { R.setHL(RD_P_XX(0)); }
template <class T> void CPUCore<T>::ld_ix_xword()  { R.setIX(RD_P_XX(0)); }
template <class T> void CPUCore<T>::ld_iy_xword()  { R.setIY(RD_P_XX(0)); }
template <class T> void CPUCore<T>::ld_bc_xword2() { R.setBC(RD_P_XX(T::EE_ED)); }
template <class T> void CPUCore<T>::ld_de_xword2() { R.setDE(RD_P_XX(T::EE_ED)); }
template <class T> void CPUCore<T>::ld_hl_xword2() { R.setHL(RD_P_XX(T::EE_ED)); }
template <class T> void CPUCore<T>::ld_sp_xword2() { R.setSP(RD_P_XX(T::EE_ED)); }

// LD ss,nn
template <class T> void CPUCore<T>::ld_bc_word() { R.setBC(RD_WORD_PC(T::CC_LD_SS_NN_1)); T::end(T::CC_LD_SS_NN); }
template <class T> void CPUCore<T>::ld_de_word() { R.setDE(RD_WORD_PC(T::CC_LD_SS_NN_1)); T::end(T::CC_LD_SS_NN); }
template <class T> void CPUCore<T>::ld_hl_word() { R.setHL(RD_WORD_PC(T::CC_LD_SS_NN_1)); T::end(T::CC_LD_SS_NN); }
template <class T> void CPUCore<T>::ld_ix_word() { R.setIX(RD_WORD_PC(T::CC_LD_SS_NN_1)); T::end(T::CC_LD_SS_NN); }
template <class T> void CPUCore<T>::ld_iy_word() { R.setIY(RD_WORD_PC(T::CC_LD_SS_NN_1)); T::end(T::CC_LD_SS_NN); }
template <class T> void CPUCore<T>::ld_sp_word() { R.setSP(RD_WORD_PC(T::CC_LD_SS_NN_1)); T::end(T::CC_LD_SS_NN); }


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
template <class T> inline void CPUCore<T>::adc_a_a()
{
	unsigned res = 2 * R.getA() + ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       (res & H_FLAG) |
	       (((R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
	T::end(T::CC_CP_R);
}
template <class T> void CPUCore<T>::adc_a_b()   { ADC(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_c()   { ADC(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_d()   { ADC(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_e()   { ADC(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_h()   { ADC(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_l()   { ADC(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_ixl() { ADC(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_ixh() { ADC(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_iyl() { ADC(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_iyh() { ADC(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::adc_a_byte(){ ADC(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::adc_a_xhl() { ADC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::adc_a_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	ADC(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::adc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	ADC(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
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
template <class T> inline void CPUCore<T>::add_a_a()
{
	unsigned res = 2 * R.getA();
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       (res & H_FLAG) |
	       (((R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
	T::end(T::CC_CP_R);
}
template <class T> void CPUCore<T>::add_a_b()   { ADD(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_c()   { ADD(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_d()   { ADD(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_e()   { ADD(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_h()   { ADD(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_l()   { ADD(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_ixl() { ADD(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_ixh() { ADD(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_iyl() { ADD(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_iyh() { ADD(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::add_a_byte(){ ADD(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::add_a_xhl() { ADD(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::add_a_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	ADD(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::add_a_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	ADD(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}

// AND r
template <class T> inline void CPUCore<T>::AND(byte reg)
{
	R.setA(R.getA() & reg);
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
}
template <class T> void CPUCore<T>::and_a()
{
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
	T::end(T::CC_CP_R);
}
template <class T> void CPUCore<T>::and_b()   { AND(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_c()   { AND(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_d()   { AND(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_e()   { AND(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_h()   { AND(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_l()   { AND(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_ixh() { AND(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_ixl() { AND(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_iyh() { AND(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_iyl() { AND(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::and_byte(){ AND(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::and_xhl() { AND(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::and_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	AND(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::and_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	AND(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
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
template <class T> void CPUCore<T>::cp_a()
{
	R.setF(ZS0 | N_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG))); // XY from operand, not from result
	T::end(T::CC_CP_R);
}
template <class T> void CPUCore<T>::cp_b()   { CP(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_c()   { CP(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_d()   { CP(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_e()   { CP(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_h()   { CP(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_l()   { CP(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_ixh() { CP(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_ixl() { CP(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_iyh() { CP(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_iyl() { CP(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::cp_byte(){ CP(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::cp_xhl() { CP(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::cp_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	CP(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::cp_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	CP(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}

// OR r
template <class T> inline void CPUCore<T>::OR(byte reg)
{
	R.setA(R.getA() | reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> void CPUCore<T>::or_a()   { R.setF(ZSPXYTable[R.getA()]); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_b()   { OR(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_c()   { OR(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_d()   { OR(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_e()   { OR(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_h()   { OR(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_l()   { OR(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_ixh() { OR(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_ixl() { OR(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_iyh() { OR(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_iyl() { OR(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::or_byte(){ OR(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::or_xhl() { OR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::or_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	OR(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::or_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	OR(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
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
template <class T> void CPUCore<T>::sbc_a_a()
{
	R.setAF((R.getF() & C_FLAG) ?
	        (255 * 256 | ZSXY255 | C_FLAG | H_FLAG | N_FLAG) :
	        (  0 * 256 | ZSXY0   |                   N_FLAG));
	T::end(T::CC_CP_R);
}
template <class T> void CPUCore<T>::sbc_a_b()   { SBC(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_c()   { SBC(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_d()   { SBC(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_e()   { SBC(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_h()   { SBC(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_l()   { SBC(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_ixh() { SBC(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_ixl() { SBC(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_iyh() { SBC(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_iyl() { SBC(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sbc_a_byte(){ SBC(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::sbc_a_xhl() { SBC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::sbc_a_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	SBC(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::sbc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	SBC(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
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
template <class T> void CPUCore<T>::sub_a()
{
	R.setAF(0 * 256 | ZSXY0 | N_FLAG);
	T::end(T::CC_CP_R);
}
template <class T> void CPUCore<T>::sub_b()   { SUB(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_c()   { SUB(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_d()   { SUB(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_e()   { SUB(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_h()   { SUB(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_l()   { SUB(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_ixh() { SUB(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_ixl() { SUB(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_iyh() { SUB(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_iyl() { SUB(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::sub_byte(){ SUB(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::sub_xhl() { SUB(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::sub_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	SUB(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::sub_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	SUB(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}

// XOR r
template <class T> inline void CPUCore<T>::XOR(byte reg)
{
	R.setA(R.getA() ^ reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> void CPUCore<T>::xor_a()   { R.setAF(0 * 256 + ZSPXY0); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_b()   { XOR(R.getB()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_c()   { XOR(R.getC()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_d()   { XOR(R.getD()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_e()   { XOR(R.getE()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_h()   { XOR(R.getH()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_l()   { XOR(R.getL()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_ixh() { XOR(R.getIXh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_ixl() { XOR(R.getIXl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_iyh() { XOR(R.getIYh()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_iyl() { XOR(R.getIYl()); T::end(T::CC_CP_R); }
template <class T> void CPUCore<T>::xor_byte(){ XOR(RDMEM_OPCODE(T::CC_CP_N_1)); T::end(T::CC_CP_N); }
template <class T> void CPUCore<T>::xor_xhl() { XOR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); T::end(T::CC_CP_XHL); }
template <class T> void CPUCore<T>::xor_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	XOR(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
}
template <class T> void CPUCore<T>::xor_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	XOR(RDMEM(memptr, T::CC_CP_XIX_2));
	T::end(T::CC_CP_XIX);
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
template <class T> void CPUCore<T>::dec_a()   { R.setA(  DEC(R.getA())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_b()   { R.setB(  DEC(R.getB())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_c()   { R.setC(  DEC(R.getC())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_d()   { R.setD(  DEC(R.getD())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_e()   { R.setE(  DEC(R.getE())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_h()   { R.setH(  DEC(R.getH())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_l()   { R.setL(  DEC(R.getL())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_ixh() { R.setIXh(DEC(R.getIXh())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_ixl() { R.setIXl(DEC(R.getIXl())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_iyh() { R.setIYh(DEC(R.getIYh())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::dec_iyl() { R.setIYl(DEC(R.getIYl())); T::end(T::CC_INC_R); }

template <class T> inline void CPUCore<T>::DEC_X(unsigned x, int ee)
{
	byte val = DEC(RDMEM(x, T::CC_INC_XHL_1 + ee));
	WRMEM(x, val, T::CC_INC_XHL_2 + ee);
	T::end(T::CC_INC_XHL + ee);
}
template <class T> void CPUCore<T>::dec_xhl() { DEC_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::dec_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	DEC_X(memptr, T::EE_INC_XIX);
}
template <class T> void CPUCore<T>::dec_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	DEC_X(memptr, T::EE_INC_XIX);
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
template <class T> void CPUCore<T>::inc_a()   { R.setA(  INC(R.getA())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_b()   { R.setB(  INC(R.getB())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_c()   { R.setC(  INC(R.getC())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_d()   { R.setD(  INC(R.getD())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_e()   { R.setE(  INC(R.getE())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_h()   { R.setH(  INC(R.getH())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_l()   { R.setL(  INC(R.getL())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_ixh() { R.setIXh(INC(R.getIXh())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_ixl() { R.setIXl(INC(R.getIXl())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_iyh() { R.setIYh(INC(R.getIYh())); T::end(T::CC_INC_R); }
template <class T> void CPUCore<T>::inc_iyl() { R.setIYl(INC(R.getIYl())); T::end(T::CC_INC_R); }

template <class T> inline void CPUCore<T>::INC_X(unsigned x, int ee)
{
	byte val = INC(RDMEM(x, T::CC_INC_XHL_1 + ee));
	WRMEM(x, val, T::CC_INC_XHL_2 + ee);
	T::end(T::CC_INC_XHL + ee);
}
template <class T> void CPUCore<T>::inc_xhl() { INC_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::inc_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	INC_X(memptr, T::EE_INC_XIX);
}
template <class T> void CPUCore<T>::inc_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	INC_X(memptr, T::EE_INC_XIX);
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
	T::end(T::CC_ADC_HL_SS);
}
template <class T> void CPUCore<T>::adc_hl_hl()
{
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
	T::end(T::CC_ADC_HL_SS);
}
template <class T> void CPUCore<T>::adc_hl_bc() { ADCW(R.getBC()); }
template <class T> void CPUCore<T>::adc_hl_de() { ADCW(R.getDE()); }
template <class T> void CPUCore<T>::adc_hl_sp() { ADCW(R.getSP()); }

// ADD HL/IX/IY,ss
template <class T> inline unsigned CPUCore<T>::ADDW(unsigned reg1, unsigned reg2)
{
	memptr = reg1 + 1; // not 16-bit
	unsigned res = reg1 + reg2;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	       (res >> 16) | // C_FLAG
	       ((res >> 8) & (X_FLAG | Y_FLAG)));
	T::end(T::CC_ADD_HL_SS);
	return res & 0xFFFF;
}
template <class T> inline unsigned CPUCore<T>::ADDW2(unsigned reg)
{
	memptr = reg + 1; // not 16-bit
	unsigned res = 2 * reg;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (res >> 16) | // C_FLAG
	       ((res >> 8) & (H_FLAG | X_FLAG | Y_FLAG)));
	T::end(T::CC_ADD_HL_SS);
	return res & 0xFFFF;
}
template <class T> void CPUCore<T>::add_hl_bc() {
	R.setHL(ADDW(R.getHL(), R.getBC()));
}
template <class T> void CPUCore<T>::add_hl_de() {
	R.setHL(ADDW(R.getHL(), R.getDE()));
}
template <class T> void CPUCore<T>::add_hl_hl() {
	R.setHL(ADDW2(R.getHL()));
}
template <class T> void CPUCore<T>::add_hl_sp() {
	R.setHL(ADDW(R.getHL(), R.getSP()));
}
template <class T> void CPUCore<T>::add_ix_bc() {
	R.setIX(ADDW(R.getIX(), R.getBC()));
}
template <class T> void CPUCore<T>::add_ix_de() {
	R.setIX(ADDW(R.getIX(), R.getDE()));
}
template <class T> void CPUCore<T>::add_ix_ix() {
	R.setIX(ADDW2(R.getIX()));
}
template <class T> void CPUCore<T>::add_ix_sp() {
	R.setIX(ADDW(R.getIX(), R.getSP()));
}
template <class T> void CPUCore<T>::add_iy_bc() {
	R.setIY(ADDW(R.getIY(), R.getBC()));
}
template <class T> void CPUCore<T>::add_iy_de() {
	R.setIY(ADDW(R.getIY(), R.getDE()));
}
template <class T> void CPUCore<T>::add_iy_iy() {
	R.setIY(ADDW2(R.getIY()));
}
template <class T> void CPUCore<T>::add_iy_sp() {
	R.setIY(ADDW(R.getIY(), R.getSP()));
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
	T::end(T::CC_ADC_HL_SS);
}
template <class T> void CPUCore<T>::sbc_hl_hl()
{
	memptr = R.getHL() + 1; // not 16-bit
	if (R.getF() & C_FLAG) {
		R.setF(C_FLAG | H_FLAG | S_FLAG | X_FLAG | Y_FLAG | N_FLAG);
		R.setHL(0xFFFF);
	} else {
		R.setF(Z_FLAG | N_FLAG);
		R.setHL(0);
	}
	T::end(T::CC_ADC_HL_SS);
}
template <class T> void CPUCore<T>::sbc_hl_bc() { SBCW(R.getBC()); }
template <class T> void CPUCore<T>::sbc_hl_de() { SBCW(R.getDE()); }
template <class T> void CPUCore<T>::sbc_hl_sp() { SBCW(R.getSP()); }


// DEC ss
template <class T> void CPUCore<T>::dec_bc() {
	R.setBC(R.getBC() - 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::dec_de() {
	R.setDE(R.getDE() - 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::dec_hl() {
	R.setHL(R.getHL() - 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::dec_ix() {
	R.setIX(R.getIX() - 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::dec_iy() {
	R.setIY(R.getIY() - 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::dec_sp() {
	R.setSP(R.getSP() - 1); T::end(T::CC_INC_SS);
}

// INC ss
template <class T> void CPUCore<T>::inc_bc() {
	R.setBC(R.getBC() + 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::inc_de() {
	R.setDE(R.getDE() + 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::inc_hl() {
	R.setHL(R.getHL() + 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::inc_ix() {
	R.setIX(R.getIX() + 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::inc_iy() {
	R.setIY(R.getIY() + 1); T::end(T::CC_INC_SS);
}
template <class T> void CPUCore<T>::inc_sp() {
	R.setSP(R.getSP() + 1); T::end(T::CC_INC_SS);
}


// BIT n,r
template <class T> inline void CPUCore<T>::BIT(byte b, byte reg)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[reg & (1 << b)] |
	       (reg & (X_FLAG | Y_FLAG)) |
	       H_FLAG);
}
template <class T> void CPUCore<T>::bit_0_a() { BIT(0, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_0_b() { BIT(0, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_0_c() { BIT(0, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_0_d() { BIT(0, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_0_e() { BIT(0, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_0_h() { BIT(0, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_0_l() { BIT(0, R.getL()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_1_a() { BIT(1, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_1_b() { BIT(1, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_1_c() { BIT(1, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_1_d() { BIT(1, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_1_e() { BIT(1, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_1_h() { BIT(1, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_1_l() { BIT(1, R.getL()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_2_a() { BIT(2, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_2_b() { BIT(2, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_2_c() { BIT(2, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_2_d() { BIT(2, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_2_e() { BIT(2, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_2_h() { BIT(2, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_2_l() { BIT(2, R.getL()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_3_a() { BIT(3, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_3_b() { BIT(3, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_3_c() { BIT(3, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_3_d() { BIT(3, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_3_e() { BIT(3, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_3_h() { BIT(3, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_3_l() { BIT(3, R.getL()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_4_a() { BIT(4, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_4_b() { BIT(4, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_4_c() { BIT(4, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_4_d() { BIT(4, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_4_e() { BIT(4, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_4_h() { BIT(4, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_4_l() { BIT(4, R.getL()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_5_a() { BIT(5, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_5_b() { BIT(5, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_5_c() { BIT(5, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_5_d() { BIT(5, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_5_e() { BIT(5, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_5_h() { BIT(5, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_5_l() { BIT(5, R.getL()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_6_a() { BIT(6, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_6_b() { BIT(6, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_6_c() { BIT(6, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_6_d() { BIT(6, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_6_e() { BIT(6, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_6_h() { BIT(6, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_6_l() { BIT(6, R.getL()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_7_a() { BIT(7, R.getA()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_7_b() { BIT(7, R.getB()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_7_c() { BIT(7, R.getC()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_7_d() { BIT(7, R.getD()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_7_e() { BIT(7, R.getE()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_7_h() { BIT(7, R.getH()); T::end(T::CC_BIT_R); }
template <class T> void CPUCore<T>::bit_7_l() { BIT(7, R.getL()); T::end(T::CC_BIT_R); }

template <class T> inline void CPUCore<T>::BIT_HL(byte bit)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(R.getHL(), T::CC_BIT_XHL_1) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	T::end(T::CC_BIT_XHL);
}
template <class T> void CPUCore<T>::bit_0_xhl() { BIT_HL(0); }
template <class T> void CPUCore<T>::bit_1_xhl() { BIT_HL(1); }
template <class T> void CPUCore<T>::bit_2_xhl() { BIT_HL(2); }
template <class T> void CPUCore<T>::bit_3_xhl() { BIT_HL(3); }
template <class T> void CPUCore<T>::bit_4_xhl() { BIT_HL(4); }
template <class T> void CPUCore<T>::bit_5_xhl() { BIT_HL(5); }
template <class T> void CPUCore<T>::bit_6_xhl() { BIT_HL(6); }
template <class T> void CPUCore<T>::bit_7_xhl() { BIT_HL(7); }

template <class T> inline void CPUCore<T>::BIT_IX(byte bit, int ofst)
{
	memptr = (R.getIX() + ofst) & 0xFFFF;
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(memptr, T::CC_BIT_XIX_1) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	T::end(T::CC_BIT_XIX);
}
template <class T> void CPUCore<T>::bit_0_xix(int o) { BIT_IX(0, o); }
template <class T> void CPUCore<T>::bit_1_xix(int o) { BIT_IX(1, o); }
template <class T> void CPUCore<T>::bit_2_xix(int o) { BIT_IX(2, o); }
template <class T> void CPUCore<T>::bit_3_xix(int o) { BIT_IX(3, o); }
template <class T> void CPUCore<T>::bit_4_xix(int o) { BIT_IX(4, o); }
template <class T> void CPUCore<T>::bit_5_xix(int o) { BIT_IX(5, o); }
template <class T> void CPUCore<T>::bit_6_xix(int o) { BIT_IX(6, o); }
template <class T> void CPUCore<T>::bit_7_xix(int o) { BIT_IX(7, o); }

template <class T> inline void CPUCore<T>::BIT_IY(byte bit, int ofst)
{
	memptr = (R.getIY() + ofst) & 0xFFFF;
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(memptr, T::CC_BIT_XIX_1) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	T::end(T::CC_BIT_XIX);
}
template <class T> void CPUCore<T>::bit_0_xiy(int o) { BIT_IY(0, o); }
template <class T> void CPUCore<T>::bit_1_xiy(int o) { BIT_IY(1, o); }
template <class T> void CPUCore<T>::bit_2_xiy(int o) { BIT_IY(2, o); }
template <class T> void CPUCore<T>::bit_3_xiy(int o) { BIT_IY(3, o); }
template <class T> void CPUCore<T>::bit_4_xiy(int o) { BIT_IY(4, o); }
template <class T> void CPUCore<T>::bit_5_xiy(int o) { BIT_IY(5, o); }
template <class T> void CPUCore<T>::bit_6_xiy(int o) { BIT_IY(6, o); }
template <class T> void CPUCore<T>::bit_7_xiy(int o) { BIT_IY(7, o); }


// RES n,r
template <class T> inline byte CPUCore<T>::RES(unsigned b, byte reg)
{
	return reg & ~(1 << b);
}
template <class T> void CPUCore<T>::res_0_a() { R.setA(RES(0, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_0_b() { R.setB(RES(0, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_0_c() { R.setC(RES(0, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_0_d() { R.setD(RES(0, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_0_e() { R.setE(RES(0, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_0_h() { R.setH(RES(0, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_0_l() { R.setL(RES(0, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_1_a() { R.setA(RES(1, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_1_b() { R.setB(RES(1, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_1_c() { R.setC(RES(1, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_1_d() { R.setD(RES(1, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_1_e() { R.setE(RES(1, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_1_h() { R.setH(RES(1, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_1_l() { R.setL(RES(1, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_2_a() { R.setA(RES(2, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_2_b() { R.setB(RES(2, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_2_c() { R.setC(RES(2, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_2_d() { R.setD(RES(2, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_2_e() { R.setE(RES(2, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_2_h() { R.setH(RES(2, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_2_l() { R.setL(RES(2, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_3_a() { R.setA(RES(3, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_3_b() { R.setB(RES(3, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_3_c() { R.setC(RES(3, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_3_d() { R.setD(RES(3, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_3_e() { R.setE(RES(3, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_3_h() { R.setH(RES(3, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_3_l() { R.setL(RES(3, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_4_a() { R.setA(RES(4, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_4_b() { R.setB(RES(4, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_4_c() { R.setC(RES(4, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_4_d() { R.setD(RES(4, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_4_e() { R.setE(RES(4, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_4_h() { R.setH(RES(4, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_4_l() { R.setL(RES(4, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_5_a() { R.setA(RES(5, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_5_b() { R.setB(RES(5, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_5_c() { R.setC(RES(5, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_5_d() { R.setD(RES(5, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_5_e() { R.setE(RES(5, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_5_h() { R.setH(RES(5, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_5_l() { R.setL(RES(5, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_6_a() { R.setA(RES(6, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_6_b() { R.setB(RES(6, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_6_c() { R.setC(RES(6, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_6_d() { R.setD(RES(6, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_6_e() { R.setE(RES(6, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_6_h() { R.setH(RES(6, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_6_l() { R.setL(RES(6, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_7_a() { R.setA(RES(7, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_7_b() { R.setB(RES(7, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_7_c() { R.setC(RES(7, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_7_d() { R.setD(RES(7, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_7_e() { R.setE(RES(7, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_7_h() { R.setH(RES(7, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::res_7_l() { R.setL(RES(7, R.getL())); T::end(T::CC_SET_R); }

template <class T> inline byte CPUCore<T>::RES_X(unsigned bit, unsigned x, int ee)
{
	byte res = RES(bit, RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::RES_X_(unsigned bit, unsigned x)
{
	memptr = x;
	return RES_X(bit, x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::RES_X_X(unsigned bit, int ofst)
{
	return RES_X_(bit, (R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RES_X_Y(unsigned bit, int ofst)
{
	return RES_X_(bit, (R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::res_0_xhl()   { RES_X(0, R.getHL(), 0); }
template <class T> void CPUCore<T>::res_1_xhl()   { RES_X(1, R.getHL(), 0); }
template <class T> void CPUCore<T>::res_2_xhl()   { RES_X(2, R.getHL(), 0); }
template <class T> void CPUCore<T>::res_3_xhl()   { RES_X(3, R.getHL(), 0); }
template <class T> void CPUCore<T>::res_4_xhl()   { RES_X(4, R.getHL(), 0); }
template <class T> void CPUCore<T>::res_5_xhl()   { RES_X(5, R.getHL(), 0); }
template <class T> void CPUCore<T>::res_6_xhl()   { RES_X(6, R.getHL(), 0); }
template <class T> void CPUCore<T>::res_7_xhl()   { RES_X(7, R.getHL(), 0); }

template <class T> void CPUCore<T>::res_0_xix(int o)   { RES_X_X(0, o); }
template <class T> void CPUCore<T>::res_1_xix(int o)   { RES_X_X(1, o); }
template <class T> void CPUCore<T>::res_2_xix(int o)   { RES_X_X(2, o); }
template <class T> void CPUCore<T>::res_3_xix(int o)   { RES_X_X(3, o); }
template <class T> void CPUCore<T>::res_4_xix(int o)   { RES_X_X(4, o); }
template <class T> void CPUCore<T>::res_5_xix(int o)   { RES_X_X(5, o); }
template <class T> void CPUCore<T>::res_6_xix(int o)   { RES_X_X(6, o); }
template <class T> void CPUCore<T>::res_7_xix(int o)   { RES_X_X(7, o); }

template <class T> void CPUCore<T>::res_0_xiy(int o)   { RES_X_Y(0, o); }
template <class T> void CPUCore<T>::res_1_xiy(int o)   { RES_X_Y(1, o); }
template <class T> void CPUCore<T>::res_2_xiy(int o)   { RES_X_Y(2, o); }
template <class T> void CPUCore<T>::res_3_xiy(int o)   { RES_X_Y(3, o); }
template <class T> void CPUCore<T>::res_4_xiy(int o)   { RES_X_Y(4, o); }
template <class T> void CPUCore<T>::res_5_xiy(int o)   { RES_X_Y(5, o); }
template <class T> void CPUCore<T>::res_6_xiy(int o)   { RES_X_Y(6, o); }
template <class T> void CPUCore<T>::res_7_xiy(int o)   { RES_X_Y(7, o); }

template <class T> void CPUCore<T>::res_0_xix_a(int o) { R.setA(RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_b(int o) { R.setB(RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_c(int o) { R.setC(RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_d(int o) { R.setD(RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_e(int o) { R.setE(RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_h(int o) { R.setH(RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_0_xix_l(int o) { R.setL(RES_X_X(0, o)); }
template <class T> void CPUCore<T>::res_1_xix_a(int o) { R.setA(RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_b(int o) { R.setB(RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_c(int o) { R.setC(RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_d(int o) { R.setD(RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_e(int o) { R.setE(RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_h(int o) { R.setH(RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_1_xix_l(int o) { R.setL(RES_X_X(1, o)); }
template <class T> void CPUCore<T>::res_2_xix_a(int o) { R.setA(RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_b(int o) { R.setB(RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_c(int o) { R.setC(RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_d(int o) { R.setD(RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_e(int o) { R.setE(RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_h(int o) { R.setH(RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_2_xix_l(int o) { R.setL(RES_X_X(2, o)); }
template <class T> void CPUCore<T>::res_3_xix_a(int o) { R.setA(RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_b(int o) { R.setB(RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_c(int o) { R.setC(RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_d(int o) { R.setD(RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_e(int o) { R.setE(RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_h(int o) { R.setH(RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_3_xix_l(int o) { R.setL(RES_X_X(3, o)); }
template <class T> void CPUCore<T>::res_4_xix_a(int o) { R.setA(RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_b(int o) { R.setB(RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_c(int o) { R.setC(RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_d(int o) { R.setD(RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_e(int o) { R.setE(RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_h(int o) { R.setH(RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_4_xix_l(int o) { R.setL(RES_X_X(4, o)); }
template <class T> void CPUCore<T>::res_5_xix_a(int o) { R.setA(RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_b(int o) { R.setB(RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_c(int o) { R.setC(RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_d(int o) { R.setD(RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_e(int o) { R.setE(RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_h(int o) { R.setH(RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_5_xix_l(int o) { R.setL(RES_X_X(5, o)); }
template <class T> void CPUCore<T>::res_6_xix_a(int o) { R.setA(RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_b(int o) { R.setB(RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_c(int o) { R.setC(RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_d(int o) { R.setD(RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_e(int o) { R.setE(RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_h(int o) { R.setH(RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_6_xix_l(int o) { R.setL(RES_X_X(6, o)); }
template <class T> void CPUCore<T>::res_7_xix_a(int o) { R.setA(RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_b(int o) { R.setB(RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_c(int o) { R.setC(RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_d(int o) { R.setD(RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_e(int o) { R.setE(RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_h(int o) { R.setH(RES_X_X(7, o)); }
template <class T> void CPUCore<T>::res_7_xix_l(int o) { R.setL(RES_X_X(7, o)); }

template <class T> void CPUCore<T>::res_0_xiy_a(int o) { R.setA(RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_b(int o) { R.setB(RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_c(int o) { R.setC(RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_d(int o) { R.setD(RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_e(int o) { R.setE(RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_h(int o) { R.setH(RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_0_xiy_l(int o) { R.setL(RES_X_Y(0, o)); }
template <class T> void CPUCore<T>::res_1_xiy_a(int o) { R.setA(RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_b(int o) { R.setB(RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_c(int o) { R.setC(RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_d(int o) { R.setD(RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_e(int o) { R.setE(RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_h(int o) { R.setH(RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_1_xiy_l(int o) { R.setL(RES_X_Y(1, o)); }
template <class T> void CPUCore<T>::res_2_xiy_a(int o) { R.setA(RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_b(int o) { R.setB(RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_c(int o) { R.setC(RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_d(int o) { R.setD(RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_e(int o) { R.setE(RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_h(int o) { R.setH(RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_2_xiy_l(int o) { R.setL(RES_X_Y(2, o)); }
template <class T> void CPUCore<T>::res_3_xiy_a(int o) { R.setA(RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_b(int o) { R.setB(RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_c(int o) { R.setC(RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_d(int o) { R.setD(RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_e(int o) { R.setE(RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_h(int o) { R.setH(RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_3_xiy_l(int o) { R.setL(RES_X_Y(3, o)); }
template <class T> void CPUCore<T>::res_4_xiy_a(int o) { R.setA(RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_b(int o) { R.setB(RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_c(int o) { R.setC(RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_d(int o) { R.setD(RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_e(int o) { R.setE(RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_h(int o) { R.setH(RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_4_xiy_l(int o) { R.setL(RES_X_Y(4, o)); }
template <class T> void CPUCore<T>::res_5_xiy_a(int o) { R.setA(RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_b(int o) { R.setB(RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_c(int o) { R.setC(RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_d(int o) { R.setD(RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_e(int o) { R.setE(RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_h(int o) { R.setH(RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_5_xiy_l(int o) { R.setL(RES_X_Y(5, o)); }
template <class T> void CPUCore<T>::res_6_xiy_a(int o) { R.setA(RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_b(int o) { R.setB(RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_c(int o) { R.setC(RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_d(int o) { R.setD(RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_e(int o) { R.setE(RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_h(int o) { R.setH(RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_6_xiy_l(int o) { R.setL(RES_X_Y(6, o)); }
template <class T> void CPUCore<T>::res_7_xiy_a(int o) { R.setA(RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_b(int o) { R.setB(RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_c(int o) { R.setC(RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_d(int o) { R.setD(RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_e(int o) { R.setE(RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_h(int o) { R.setH(RES_X_Y(7, o)); }
template <class T> void CPUCore<T>::res_7_xiy_l(int o) { R.setL(RES_X_Y(7, o)); }


// SET n,r
template <class T> inline byte CPUCore<T>::SET(unsigned b, byte reg)
{
	return reg | (1 << b);
}
template <class T> void CPUCore<T>::set_0_a() { R.setA(SET(0, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_0_b() { R.setB(SET(0, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_0_c() { R.setC(SET(0, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_0_d() { R.setD(SET(0, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_0_e() { R.setE(SET(0, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_0_h() { R.setH(SET(0, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_0_l() { R.setL(SET(0, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_1_a() { R.setA(SET(1, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_1_b() { R.setB(SET(1, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_1_c() { R.setC(SET(1, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_1_d() { R.setD(SET(1, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_1_e() { R.setE(SET(1, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_1_h() { R.setH(SET(1, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_1_l() { R.setL(SET(1, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_2_a() { R.setA(SET(2, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_2_b() { R.setB(SET(2, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_2_c() { R.setC(SET(2, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_2_d() { R.setD(SET(2, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_2_e() { R.setE(SET(2, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_2_h() { R.setH(SET(2, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_2_l() { R.setL(SET(2, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_3_a() { R.setA(SET(3, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_3_b() { R.setB(SET(3, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_3_c() { R.setC(SET(3, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_3_d() { R.setD(SET(3, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_3_e() { R.setE(SET(3, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_3_h() { R.setH(SET(3, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_3_l() { R.setL(SET(3, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_4_a() { R.setA(SET(4, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_4_b() { R.setB(SET(4, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_4_c() { R.setC(SET(4, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_4_d() { R.setD(SET(4, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_4_e() { R.setE(SET(4, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_4_h() { R.setH(SET(4, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_4_l() { R.setL(SET(4, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_5_a() { R.setA(SET(5, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_5_b() { R.setB(SET(5, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_5_c() { R.setC(SET(5, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_5_d() { R.setD(SET(5, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_5_e() { R.setE(SET(5, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_5_h() { R.setH(SET(5, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_5_l() { R.setL(SET(5, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_6_a() { R.setA(SET(6, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_6_b() { R.setB(SET(6, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_6_c() { R.setC(SET(6, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_6_d() { R.setD(SET(6, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_6_e() { R.setE(SET(6, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_6_h() { R.setH(SET(6, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_6_l() { R.setL(SET(6, R.getL())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_7_a() { R.setA(SET(7, R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_7_b() { R.setB(SET(7, R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_7_c() { R.setC(SET(7, R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_7_d() { R.setD(SET(7, R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_7_e() { R.setE(SET(7, R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_7_h() { R.setH(SET(7, R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::set_7_l() { R.setL(SET(7, R.getL())); T::end(T::CC_SET_R); }

template <class T> inline byte CPUCore<T>::SET_X(unsigned bit, unsigned x, int ee)
{
	byte res = SET(bit, RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::SET_X_(unsigned bit, unsigned x)
{
	memptr = x;
	return SET_X(bit, x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::SET_X_X(unsigned bit, int ofst)
{
	return SET_X_(bit, (R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SET_X_Y(unsigned bit, int ofst)
{
	return SET_X_(bit, (R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::set_0_xhl()   { SET_X(0, R.getHL(), 0); }
template <class T> void CPUCore<T>::set_1_xhl()   { SET_X(1, R.getHL(), 0); }
template <class T> void CPUCore<T>::set_2_xhl()   { SET_X(2, R.getHL(), 0); }
template <class T> void CPUCore<T>::set_3_xhl()   { SET_X(3, R.getHL(), 0); }
template <class T> void CPUCore<T>::set_4_xhl()   { SET_X(4, R.getHL(), 0); }
template <class T> void CPUCore<T>::set_5_xhl()   { SET_X(5, R.getHL(), 0); }
template <class T> void CPUCore<T>::set_6_xhl()   { SET_X(6, R.getHL(), 0); }
template <class T> void CPUCore<T>::set_7_xhl()   { SET_X(7, R.getHL(), 0); }

template <class T> void CPUCore<T>::set_0_xix(int o)   { SET_X_X(0, o); }
template <class T> void CPUCore<T>::set_1_xix(int o)   { SET_X_X(1, o); }
template <class T> void CPUCore<T>::set_2_xix(int o)   { SET_X_X(2, o); }
template <class T> void CPUCore<T>::set_3_xix(int o)   { SET_X_X(3, o); }
template <class T> void CPUCore<T>::set_4_xix(int o)   { SET_X_X(4, o); }
template <class T> void CPUCore<T>::set_5_xix(int o)   { SET_X_X(5, o); }
template <class T> void CPUCore<T>::set_6_xix(int o)   { SET_X_X(6, o); }
template <class T> void CPUCore<T>::set_7_xix(int o)   { SET_X_X(7, o); }

template <class T> void CPUCore<T>::set_0_xiy(int o)   { SET_X_Y(0, o); }
template <class T> void CPUCore<T>::set_1_xiy(int o)   { SET_X_Y(1, o); }
template <class T> void CPUCore<T>::set_2_xiy(int o)   { SET_X_Y(2, o); }
template <class T> void CPUCore<T>::set_3_xiy(int o)   { SET_X_Y(3, o); }
template <class T> void CPUCore<T>::set_4_xiy(int o)   { SET_X_Y(4, o); }
template <class T> void CPUCore<T>::set_5_xiy(int o)   { SET_X_Y(5, o); }
template <class T> void CPUCore<T>::set_6_xiy(int o)   { SET_X_Y(6, o); }
template <class T> void CPUCore<T>::set_7_xiy(int o)   { SET_X_Y(7, o); }

template <class T> void CPUCore<T>::set_0_xix_a(int o) { R.setA(SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_b(int o) { R.setB(SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_c(int o) { R.setC(SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_d(int o) { R.setD(SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_e(int o) { R.setE(SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_h(int o) { R.setH(SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_0_xix_l(int o) { R.setL(SET_X_X(0, o)); }
template <class T> void CPUCore<T>::set_1_xix_a(int o) { R.setA(SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_b(int o) { R.setB(SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_c(int o) { R.setC(SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_d(int o) { R.setD(SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_e(int o) { R.setE(SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_h(int o) { R.setH(SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_1_xix_l(int o) { R.setL(SET_X_X(1, o)); }
template <class T> void CPUCore<T>::set_2_xix_a(int o) { R.setA(SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_b(int o) { R.setB(SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_c(int o) { R.setC(SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_d(int o) { R.setD(SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_e(int o) { R.setE(SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_h(int o) { R.setH(SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_2_xix_l(int o) { R.setL(SET_X_X(2, o)); }
template <class T> void CPUCore<T>::set_3_xix_a(int o) { R.setA(SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_b(int o) { R.setB(SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_c(int o) { R.setC(SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_d(int o) { R.setD(SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_e(int o) { R.setE(SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_h(int o) { R.setH(SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_3_xix_l(int o) { R.setL(SET_X_X(3, o)); }
template <class T> void CPUCore<T>::set_4_xix_a(int o) { R.setA(SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_b(int o) { R.setB(SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_c(int o) { R.setC(SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_d(int o) { R.setD(SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_e(int o) { R.setE(SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_h(int o) { R.setH(SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_4_xix_l(int o) { R.setL(SET_X_X(4, o)); }
template <class T> void CPUCore<T>::set_5_xix_a(int o) { R.setA(SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_b(int o) { R.setB(SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_c(int o) { R.setC(SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_d(int o) { R.setD(SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_e(int o) { R.setE(SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_h(int o) { R.setH(SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_5_xix_l(int o) { R.setL(SET_X_X(5, o)); }
template <class T> void CPUCore<T>::set_6_xix_a(int o) { R.setA(SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_b(int o) { R.setB(SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_c(int o) { R.setC(SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_d(int o) { R.setD(SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_e(int o) { R.setE(SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_h(int o) { R.setH(SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_6_xix_l(int o) { R.setL(SET_X_X(6, o)); }
template <class T> void CPUCore<T>::set_7_xix_a(int o) { R.setA(SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_b(int o) { R.setB(SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_c(int o) { R.setC(SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_d(int o) { R.setD(SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_e(int o) { R.setE(SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_h(int o) { R.setH(SET_X_X(7, o)); }
template <class T> void CPUCore<T>::set_7_xix_l(int o) { R.setL(SET_X_X(7, o)); }

template <class T> void CPUCore<T>::set_0_xiy_a(int o) { R.setA(SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_b(int o) { R.setB(SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_c(int o) { R.setC(SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_d(int o) { R.setD(SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_e(int o) { R.setE(SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_h(int o) { R.setH(SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_0_xiy_l(int o) { R.setL(SET_X_Y(0, o)); }
template <class T> void CPUCore<T>::set_1_xiy_a(int o) { R.setA(SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_b(int o) { R.setB(SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_c(int o) { R.setC(SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_d(int o) { R.setD(SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_e(int o) { R.setE(SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_h(int o) { R.setH(SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_1_xiy_l(int o) { R.setL(SET_X_Y(1, o)); }
template <class T> void CPUCore<T>::set_2_xiy_a(int o) { R.setA(SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_b(int o) { R.setB(SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_c(int o) { R.setC(SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_d(int o) { R.setD(SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_e(int o) { R.setE(SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_h(int o) { R.setH(SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_2_xiy_l(int o) { R.setL(SET_X_Y(2, o)); }
template <class T> void CPUCore<T>::set_3_xiy_a(int o) { R.setA(SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_b(int o) { R.setB(SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_c(int o) { R.setC(SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_d(int o) { R.setD(SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_e(int o) { R.setE(SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_h(int o) { R.setH(SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_3_xiy_l(int o) { R.setL(SET_X_Y(3, o)); }
template <class T> void CPUCore<T>::set_4_xiy_a(int o) { R.setA(SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_b(int o) { R.setB(SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_c(int o) { R.setC(SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_d(int o) { R.setD(SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_e(int o) { R.setE(SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_h(int o) { R.setH(SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_4_xiy_l(int o) { R.setL(SET_X_Y(4, o)); }
template <class T> void CPUCore<T>::set_5_xiy_a(int o) { R.setA(SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_b(int o) { R.setB(SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_c(int o) { R.setC(SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_d(int o) { R.setD(SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_e(int o) { R.setE(SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_h(int o) { R.setH(SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_5_xiy_l(int o) { R.setL(SET_X_Y(5, o)); }
template <class T> void CPUCore<T>::set_6_xiy_a(int o) { R.setA(SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_b(int o) { R.setB(SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_c(int o) { R.setC(SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_d(int o) { R.setD(SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_e(int o) { R.setE(SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_h(int o) { R.setH(SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_6_xiy_l(int o) { R.setL(SET_X_Y(6, o)); }
template <class T> void CPUCore<T>::set_7_xiy_a(int o) { R.setA(SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_b(int o) { R.setB(SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_c(int o) { R.setC(SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_d(int o) { R.setD(SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_e(int o) { R.setE(SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_h(int o) { R.setH(SET_X_Y(7, o)); }
template <class T> void CPUCore<T>::set_7_xiy_l(int o) { R.setL(SET_X_Y(7, o)); }


// RL r
template <class T> inline byte CPUCore<T>::RL(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | ((R.getF() & C_FLAG) ? 0x01 : 0);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RL_X(unsigned x, int ee)
{
	byte res = RL(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::RL_X_(unsigned x)
{
	memptr = x; return RL_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::RL_X_X(int ofst)
{
	return RL_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RL_X_Y(int ofst)
{
	return RL_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rl_a() { R.setA(RL(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rl_b() { R.setB(RL(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rl_c() { R.setC(RL(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rl_d() { R.setD(RL(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rl_e() { R.setE(RL(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rl_h() { R.setH(RL(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rl_l() { R.setL(RL(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::rl_xhl()      { RL_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::rl_xix(int o) { RL_X_X(o); }
template <class T> void CPUCore<T>::rl_xiy(int o) { RL_X_Y(o); }

template <class T> void CPUCore<T>::rl_xix_a(int o) { R.setA(RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_b(int o) { R.setB(RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_c(int o) { R.setC(RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_d(int o) { R.setD(RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_e(int o) { R.setE(RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_h(int o) { R.setH(RL_X_X(o)); }
template <class T> void CPUCore<T>::rl_xix_l(int o) { R.setL(RL_X_X(o)); }

template <class T> void CPUCore<T>::rl_xiy_a(int o) { R.setA(RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_b(int o) { R.setB(RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_c(int o) { R.setC(RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_d(int o) { R.setD(RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_e(int o) { R.setE(RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_h(int o) { R.setH(RL_X_Y(o)); }
template <class T> void CPUCore<T>::rl_xiy_l(int o) { R.setL(RL_X_Y(o)); }


// RLC r
template <class T> inline byte CPUCore<T>::RLC(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | c;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RLC_X(unsigned x, int ee)
{
	byte res = RLC(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::RLC_X_(unsigned x)
{
	memptr = x; return RLC_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::RLC_X_X(int ofst)
{
	return RLC_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RLC_X_Y(int ofst)
{
	return RLC_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rlc_a() { R.setA(RLC(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rlc_b() { R.setB(RLC(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rlc_c() { R.setC(RLC(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rlc_d() { R.setD(RLC(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rlc_e() { R.setE(RLC(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rlc_h() { R.setH(RLC(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rlc_l() { R.setL(RLC(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::rlc_xhl()      { RLC_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::rlc_xix(int o) { RLC_X_X(o); }
template <class T> void CPUCore<T>::rlc_xiy(int o) { RLC_X_Y(o); }

template <class T> void CPUCore<T>::rlc_xix_a(int o) { R.setA(RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_b(int o) { R.setB(RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_c(int o) { R.setC(RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_d(int o) { R.setD(RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_e(int o) { R.setE(RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_h(int o) { R.setH(RLC_X_X(o)); }
template <class T> void CPUCore<T>::rlc_xix_l(int o) { R.setL(RLC_X_X(o)); }

template <class T> void CPUCore<T>::rlc_xiy_a(int o) { R.setA(RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_b(int o) { R.setB(RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_c(int o) { R.setC(RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_d(int o) { R.setD(RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_e(int o) { R.setE(RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_h(int o) { R.setH(RLC_X_Y(o)); }
template <class T> void CPUCore<T>::rlc_xiy_l(int o) { R.setL(RLC_X_Y(o)); }


// RR r
template <class T> inline byte CPUCore<T>::RR(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | ((R.getF() & C_FLAG) << 7);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RR_X(unsigned x, int ee)
{
	byte res = RR(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::RR_X_(unsigned x)
{
	memptr = x; return RR_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::RR_X_X(int ofst)
{
	return RR_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RR_X_Y(int ofst)
{
	return RR_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rr_a() { R.setA(RR(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rr_b() { R.setB(RR(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rr_c() { R.setC(RR(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rr_d() { R.setD(RR(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rr_e() { R.setE(RR(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rr_h() { R.setH(RR(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rr_l() { R.setL(RR(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::rr_xhl()      { RR_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::rr_xix(int o) { RR_X_X(o); }
template <class T> void CPUCore<T>::rr_xiy(int o) { RR_X_Y(o); }

template <class T> void CPUCore<T>::rr_xix_a(int o) { R.setA(RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_b(int o) { R.setB(RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_c(int o) { R.setC(RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_d(int o) { R.setD(RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_e(int o) { R.setE(RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_h(int o) { R.setH(RR_X_X(o)); }
template <class T> void CPUCore<T>::rr_xix_l(int o) { R.setL(RR_X_X(o)); }

template <class T> void CPUCore<T>::rr_xiy_a(int o) { R.setA(RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_b(int o) { R.setB(RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_c(int o) { R.setC(RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_d(int o) { R.setD(RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_e(int o) { R.setE(RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_h(int o) { R.setH(RR_X_Y(o)); }
template <class T> void CPUCore<T>::rr_xiy_l(int o) { R.setL(RR_X_Y(o)); }


// RRC r
template <class T> inline byte CPUCore<T>::RRC(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (c << 7);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::RRC_X(unsigned x, int ee)
{
	byte res = RRC(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::RRC_X_(unsigned x)
{
	memptr = x; return RRC_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::RRC_X_X(int ofst)
{
	return RRC_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::RRC_X_Y(int ofst)
{
	return RRC_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::rrc_a() { R.setA(RRC(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rrc_b() { R.setB(RRC(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rrc_c() { R.setC(RRC(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rrc_d() { R.setD(RRC(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rrc_e() { R.setE(RRC(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rrc_h() { R.setH(RRC(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::rrc_l() { R.setL(RRC(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::rrc_xhl()      { RRC_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::rrc_xix(int o) { RRC_X_X(o); }
template <class T> void CPUCore<T>::rrc_xiy(int o) { RRC_X_Y(o); }

template <class T> void CPUCore<T>::rrc_xix_a(int o) { R.setA(RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_b(int o) { R.setB(RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_c(int o) { R.setC(RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_d(int o) { R.setD(RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_e(int o) { R.setE(RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_h(int o) { R.setH(RRC_X_X(o)); }
template <class T> void CPUCore<T>::rrc_xix_l(int o) { R.setL(RRC_X_X(o)); }

template <class T> void CPUCore<T>::rrc_xiy_a(int o) { R.setA(RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_b(int o) { R.setB(RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_c(int o) { R.setC(RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_d(int o) { R.setD(RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_e(int o) { R.setE(RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_h(int o) { R.setH(RRC_X_Y(o)); }
template <class T> void CPUCore<T>::rrc_xiy_l(int o) { R.setL(RRC_X_Y(o)); }


// SLA r
template <class T> inline byte CPUCore<T>::SLA(byte reg)
{
	byte c = reg >> 7;
	reg <<= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLA_X(unsigned x, int ee)
{
	byte res = SLA(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::SLA_X_(unsigned x)
{
	memptr = x; return SLA_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::SLA_X_X(int ofst)
{
	return SLA_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SLA_X_Y(int ofst)
{
	return SLA_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::sla_a() { R.setA(SLA(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sla_b() { R.setB(SLA(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sla_c() { R.setC(SLA(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sla_d() { R.setD(SLA(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sla_e() { R.setE(SLA(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sla_h() { R.setH(SLA(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sla_l() { R.setL(SLA(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::sla_xhl()      { SLA_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::sla_xix(int o) { SLA_X_X(o); }
template <class T> void CPUCore<T>::sla_xiy(int o) { SLA_X_Y(o); }

template <class T> void CPUCore<T>::sla_xix_a(int o) { R.setA(SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_b(int o) { R.setB(SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_c(int o) { R.setC(SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_d(int o) { R.setD(SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_e(int o) { R.setE(SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_h(int o) { R.setH(SLA_X_X(o)); }
template <class T> void CPUCore<T>::sla_xix_l(int o) { R.setL(SLA_X_X(o)); }

template <class T> void CPUCore<T>::sla_xiy_a(int o) { R.setA(SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_b(int o) { R.setB(SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_c(int o) { R.setC(SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_d(int o) { R.setD(SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_e(int o) { R.setE(SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_h(int o) { R.setH(SLA_X_Y(o)); }
template <class T> void CPUCore<T>::sla_xiy_l(int o) { R.setL(SLA_X_Y(o)); }


// SLL r
template <class T> inline byte CPUCore<T>::SLL(byte reg)
{
	byte c = reg >> 7;
	reg = (reg << 1) | 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SLL_X(unsigned x, int ee)
{
	byte res = SLL(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::SLL_X_(unsigned x)
{
	memptr = x; return SLL_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::SLL_X_X(int ofst)
{
	return SLL_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SLL_X_Y(int ofst)
{
	return SLL_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::sll_a() { R.setA(SLL(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sll_b() { R.setB(SLL(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sll_c() { R.setC(SLL(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sll_d() { R.setD(SLL(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sll_e() { R.setE(SLL(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sll_h() { R.setH(SLL(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sll_l() { R.setL(SLL(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::sll_xhl()      { SLL_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::sll_xix(int o) { SLL_X_X(o); }
template <class T> void CPUCore<T>::sll_xiy(int o) { SLL_X_Y(o); }

template <class T> void CPUCore<T>::sll_xix_a(int o) { R.setA(SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_b(int o) { R.setB(SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_c(int o) { R.setC(SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_d(int o) { R.setD(SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_e(int o) { R.setE(SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_h(int o) { R.setH(SLL_X_X(o)); }
template <class T> void CPUCore<T>::sll_xix_l(int o) { R.setL(SLL_X_X(o)); }

template <class T> void CPUCore<T>::sll_xiy_a(int o) { R.setA(SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_b(int o) { R.setB(SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_c(int o) { R.setC(SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_d(int o) { R.setD(SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_e(int o) { R.setE(SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_h(int o) { R.setH(SLL_X_Y(o)); }
template <class T> void CPUCore<T>::sll_xiy_l(int o) { R.setL(SLL_X_Y(o)); }


// SRA r
template <class T> inline byte CPUCore<T>::SRA(byte reg)
{
	byte c = reg & 1;
	reg = (reg >> 1) | (reg & 0x80);
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRA_X(unsigned x, int ee)
{
	byte res = SRA(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::SRA_X_(unsigned x)
{
	memptr = x; return SRA_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::SRA_X_X(int ofst)
{
	return SRA_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SRA_X_Y(int ofst)
{
	return SRA_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::sra_a() { R.setA(SRA(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sra_b() { R.setB(SRA(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sra_c() { R.setC(SRA(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sra_d() { R.setD(SRA(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sra_e() { R.setE(SRA(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sra_h() { R.setH(SRA(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::sra_l() { R.setL(SRA(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::sra_xhl()      { SRA_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::sra_xix(int o) { SRA_X_X(o); }
template <class T> void CPUCore<T>::sra_xiy(int o) { SRA_X_Y(o); }

template <class T> void CPUCore<T>::sra_xix_a(int o) { R.setA(SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_b(int o) { R.setB(SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_c(int o) { R.setC(SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_d(int o) { R.setD(SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_e(int o) { R.setE(SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_h(int o) { R.setH(SRA_X_X(o)); }
template <class T> void CPUCore<T>::sra_xix_l(int o) { R.setL(SRA_X_X(o)); }

template <class T> void CPUCore<T>::sra_xiy_a(int o) { R.setA(SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_b(int o) { R.setB(SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_c(int o) { R.setC(SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_d(int o) { R.setD(SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_e(int o) { R.setE(SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_h(int o) { R.setH(SRA_X_Y(o)); }
template <class T> void CPUCore<T>::sra_xiy_l(int o) { R.setL(SRA_X_Y(o)); }


// SRL R
template <class T> inline byte CPUCore<T>::SRL(byte reg)
{
	byte c = reg & 1;
	reg >>= 1;
	R.setF(ZSPXYTable[reg] | (c ? C_FLAG : 0));
	return reg;
}
template <class T> inline byte CPUCore<T>::SRL_X(unsigned x, int ee)
{
	byte res = SRL(RDMEM(x, T::CC_SET_XHL_1 + ee));
	WRMEM(x, res, T::CC_SET_XHL_2 + ee);
	T::end(T::CC_SET_XHL + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::SRL_X_(unsigned x)
{
	memptr = x; return SRL_X(x, T::EE_SET_XIX);
}
template <class T> inline byte CPUCore<T>::SRL_X_X(int ofst)
{
	return SRL_X_((R.getIX() + ofst) & 0xFFFF);
}
template <class T> inline byte CPUCore<T>::SRL_X_Y(int ofst)
{
	return SRL_X_((R.getIY() + ofst) & 0xFFFF);
}

template <class T> void CPUCore<T>::srl_a() { R.setA(SRL(R.getA())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::srl_b() { R.setB(SRL(R.getB())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::srl_c() { R.setC(SRL(R.getC())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::srl_d() { R.setD(SRL(R.getD())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::srl_e() { R.setE(SRL(R.getE())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::srl_h() { R.setH(SRL(R.getH())); T::end(T::CC_SET_R); }
template <class T> void CPUCore<T>::srl_l() { R.setL(SRL(R.getL())); T::end(T::CC_SET_R); }

template <class T> void CPUCore<T>::srl_xhl()      { SRL_X(R.getHL(), 0); }
template <class T> void CPUCore<T>::srl_xix(int o) { SRL_X_X(o); }
template <class T> void CPUCore<T>::srl_xiy(int o) { SRL_X_Y(o); }

template <class T> void CPUCore<T>::srl_xix_a(int o) { R.setA(SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_b(int o) { R.setB(SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_c(int o) { R.setC(SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_d(int o) { R.setD(SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_e(int o) { R.setE(SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_h(int o) { R.setH(SRL_X_X(o)); }
template <class T> void CPUCore<T>::srl_xix_l(int o) { R.setL(SRL_X_X(o)); }

template <class T> void CPUCore<T>::srl_xiy_a(int o) { R.setA(SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_b(int o) { R.setB(SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_c(int o) { R.setC(SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_d(int o) { R.setD(SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_e(int o) { R.setE(SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_h(int o) { R.setH(SRL_X_Y(o)); }
template <class T> void CPUCore<T>::srl_xiy_l(int o) { R.setL(SRL_X_Y(o)); }


// RLA RLCA RRA RRCA
template <class T> void CPUCore<T>::rla()
{
	byte c = R.getF() & C_FLAG;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x80) ? C_FLAG : 0));
	R.setA((R.getA() << 1) | (c ? 1 : 0));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	T::end(T::CC_RLA);
}
template <class T> void CPUCore<T>::rlca()
{
	R.setA((R.getA() << 1) | (R.getA() >> 7));
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() & (Y_FLAG | X_FLAG | C_FLAG)));
	T::end(T::CC_RLA);
}
template <class T> void CPUCore<T>::rra()
{
	byte c = (R.getF() & C_FLAG) << 7;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x01) ? C_FLAG : 0));
	R.setA((R.getA() >> 1) | c);
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	T::end(T::CC_RLA);
}
template <class T> void CPUCore<T>::rrca()
{
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() &  C_FLAG));
	R.setA((R.getA() >> 1) | (R.getA() << 7));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	T::end(T::CC_RLA);
}


// RLD
template <class T> void CPUCore<T>::rld()
{
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	memptr = R.getHL() + 1; // not 16-bit
	WRMEM(R.getHL(), (val << 4) | (R.getA() & 0x0F), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val >> 4));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
	T::end(T::CC_RLD);
}

// RRD
template <class T> void CPUCore<T>::rrd()
{
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	memptr = R.getHL() + 1; // not 16-bit
	WRMEM(R.getHL(), (val >> 4) | (R.getA() << 4), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val & 0x0F));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
	T::end(T::CC_RLD);
}


// PUSH ss
template <class T> inline void CPUCore<T>::PUSH(unsigned reg, int ee)
{
	R.setSP(R.getSP() - 2);
	WR_WORD_rev(R.getSP(), reg, T::CC_PUSH_1 + ee);
}
template <class T> void CPUCore<T>::push_af() { PUSH(R.getAF(), 0); T::end(T::CC_PUSH); }
template <class T> void CPUCore<T>::push_bc() { PUSH(R.getBC(), 0); T::end(T::CC_PUSH); }
template <class T> void CPUCore<T>::push_de() { PUSH(R.getDE(), 0); T::end(T::CC_PUSH); }
template <class T> void CPUCore<T>::push_hl() { PUSH(R.getHL(), 0); T::end(T::CC_PUSH); }
template <class T> void CPUCore<T>::push_ix() { PUSH(R.getIX(), 0); T::end(T::CC_PUSH); }
template <class T> void CPUCore<T>::push_iy() { PUSH(R.getIY(), 0); T::end(T::CC_PUSH); }


// POP ss
template <class T> inline unsigned CPUCore<T>::POP(int ee)
{
	unsigned addr = R.getSP();
	R.setSP(addr + 2);
	return RD_WORD(addr, T::CC_POP_1 + ee);
}
template <class T> void CPUCore<T>::pop_af() { R.setAF(POP(0)); T::end(T::CC_POP); }
template <class T> void CPUCore<T>::pop_bc() { R.setBC(POP(0)); T::end(T::CC_POP); }
template <class T> void CPUCore<T>::pop_de() { R.setDE(POP(0)); T::end(T::CC_POP); }
template <class T> void CPUCore<T>::pop_hl() { R.setHL(POP(0)); T::end(T::CC_POP); }
template <class T> void CPUCore<T>::pop_ix() { R.setIX(POP(0)); T::end(T::CC_POP); }
template <class T> void CPUCore<T>::pop_iy() { R.setIY(POP(0)); T::end(T::CC_POP); }


// CALL nn / CALL cc,nn
template <class T> inline void CPUCore<T>::CALL()
{
	memptr = RD_WORD_PC(T::CC_CALL_1);
	PUSH(R.getPC(), T::EE_CALL);
	R.setPC(memptr);
	T::end(T::CC_CALL_A);
}
template <class T> inline void CPUCore<T>::SKIP_CALL()
{
	memptr = RD_WORD_PC(T::CC_CALL_1);
	T::end(T::CC_CALL_B);
}
template <class T> void CPUCore<T>::call()    { CALL(); }
template <class T> void CPUCore<T>::call_c()  { if (C())  CALL(); else SKIP_CALL(); }
template <class T> void CPUCore<T>::call_m()  { if (M())  CALL(); else SKIP_CALL(); }
template <class T> void CPUCore<T>::call_nc() { if (NC()) CALL(); else SKIP_CALL(); }
template <class T> void CPUCore<T>::call_nz() { if (NZ()) CALL(); else SKIP_CALL(); }
template <class T> void CPUCore<T>::call_p()  { if (P())  CALL(); else SKIP_CALL(); }
template <class T> void CPUCore<T>::call_pe() { if (PE()) CALL(); else SKIP_CALL(); }
template <class T> void CPUCore<T>::call_po() { if (PO()) CALL(); else SKIP_CALL(); }
template <class T> void CPUCore<T>::call_z()  { if (Z())  CALL(); else SKIP_CALL(); }


// RST n
template <class T> inline void CPUCore<T>::RST(unsigned x)
{
	PUSH(R.getPC(), 0);
	memptr = x;
	R.setPC(x);
	T::end(T::CC_RST);
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
template <class T> inline void CPUCore<T>::RET(bool cond, int ee)
{
	if (cond) {
		memptr = POP(ee);
		R.setPC(memptr);
		T::end(T::CC_RET_A + ee);
	} else {
		T::end(T::CC_RET_B + ee);
	}
}
template <class T> void CPUCore<T>::ret()    { RET(true, 0); }
template <class T> void CPUCore<T>::ret_c()  { RET(C(),  T::EE_RET_C); }
template <class T> void CPUCore<T>::ret_m()  { RET(M(),  T::EE_RET_C); }
template <class T> void CPUCore<T>::ret_nc() { RET(NC(), T::EE_RET_C); }
template <class T> void CPUCore<T>::ret_nz() { RET(NZ(), T::EE_RET_C); }
template <class T> void CPUCore<T>::ret_p()  { RET(P(),  T::EE_RET_C); }
template <class T> void CPUCore<T>::ret_pe() { RET(PE(), T::EE_RET_C); }
template <class T> void CPUCore<T>::ret_po() { RET(PO(), T::EE_RET_C); }
template <class T> void CPUCore<T>::ret_z()  { RET(Z(),  T::EE_RET_C); }

template <class T> void CPUCore<T>::reti()
{
	// same as retn
	R.setIFF1(R.getIFF2());
	R.setNextIFF1(R.getIFF2());
	setSlowInstructions();
	RET(true, T::EE_RETN);
}
template <class T> void CPUCore<T>::retn()
{
	R.setIFF1(R.getIFF2());
	R.setNextIFF1(R.getIFF2());
	setSlowInstructions();
	RET(true, T::EE_RETN);
}


// JP ss
template <class T> void CPUCore<T>::jp_hl() { R.setPC(R.getHL()); T::end(T::CC_JP_HL); }
template <class T> void CPUCore<T>::jp_ix() { R.setPC(R.getIX()); T::end(T::CC_JP_HL); }
template <class T> void CPUCore<T>::jp_iy() { R.setPC(R.getIY()); T::end(T::CC_JP_HL); }

// JP nn / JP cc,nn
template <class T> inline void CPUCore<T>::JP()
{
	memptr = RD_WORD_PC(T::CC_JP_1);
	R.setPC(memptr);
	end(T::CC_JP);
}
template <class T> inline void CPUCore<T>::SKIP_JP()
{
	memptr = RD_WORD_PC(T::CC_JP_1);
	end(T::CC_JP);
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
template <class T> inline void CPUCore<T>::JR(int ee)
{
	offset ofst = RDMEM_OPCODE(T::CC_JR_1 + ee);
	R.setPC((R.getPC() + ofst) & 0xFFFF);
	memptr = R.getPC();
	T::end(T::CC_JR_A + ee);
}
template <class T> inline void CPUCore<T>::SKIP_JR(int ee)
{
	RDMEM_OPCODE(T::CC_JR_1 + ee); // ignore return value
	T::end(T::CC_JR_B + ee);
}
template <class T> void CPUCore<T>::jr()    { JR(0); }
template <class T> void CPUCore<T>::jr_c()  { if (C())  JR(0); else SKIP_JR(0); }
template <class T> void CPUCore<T>::jr_nc() { if (NC()) JR(0); else SKIP_JR(0); }
template <class T> void CPUCore<T>::jr_nz() { if (NZ()) JR(0); else SKIP_JR(0); }
template <class T> void CPUCore<T>::jr_z()  { if (Z())  JR(0); else SKIP_JR(0); }

// DJNZ e
template <class T> void CPUCore<T>::djnz()
{
	byte b = R.getB();
	R.setB(--b);
	if (b) JR(T::EE_DJNZ); else SKIP_JR(T::EE_DJNZ);
}

// EX (SP),ss
template <class T> inline unsigned CPUCore<T>::EX_SP(unsigned reg)
{
	memptr = RD_WORD(R.getSP(), T::CC_EX_SP_HL_1);
	WR_WORD_rev(R.getSP(), reg, T::CC_EX_SP_HL_2);
	T::end(T::CC_EX_SP_HL);
	return memptr;
}
template <class T> void CPUCore<T>::ex_xsp_hl() { R.setHL(EX_SP(R.getHL())); }
template <class T> void CPUCore<T>::ex_xsp_ix() { R.setIX(EX_SP(R.getIX())); }
template <class T> void CPUCore<T>::ex_xsp_iy() { R.setIY(EX_SP(R.getIY())); }


// IN r,(c)
template <class T> inline byte CPUCore<T>::IN()
{
	byte res = READ_PORT(R.getBC(), T::CC_IN_R_C_1);
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[res]);
	T::end(T::CC_IN_R_C);
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
	unsigned y = RDMEM_OPCODE(T::CC_IN_A_N_1) + 256 * R.getA();
	R.setA(READ_PORT(y, T::CC_IN_A_N_2));
	T::end(T::CC_IN_A_N);
}


// OUT (c),r
template <class T> inline void CPUCore<T>::OUT(byte val)
{
	WRITE_PORT(R.getBC(), val, T::CC_OUT_C_R_1);
	T::end(T::CC_OUT_C_R);
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
	unsigned y = RDMEM_OPCODE(T::CC_OUT_N_A_1) + 256 * R.getA();
	WRITE_PORT(y, R.getA(), T::CC_OUT_N_A_2);
	T::end(T::CC_OUT_N_A);
}


// block CP
template <class T> inline void CPUCore<T>::BLOCK_CP(int increase, bool repeat)
{
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
		T::end(T::CC_CPIR);
	} else {
		T::end(T::CC_CPI);
	}
}
template <class T> void CPUCore<T>::cpd()  { BLOCK_CP(-1, false); }
template <class T> void CPUCore<T>::cpi()  { BLOCK_CP( 1, false); }
template <class T> void CPUCore<T>::cpdr() { BLOCK_CP(-1, true ); }
template <class T> void CPUCore<T>::cpir() { BLOCK_CP( 1, true ); }


// block LD
template <class T> inline void CPUCore<T>::BLOCK_LD(int increase, bool repeat)
{
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
		T::end(T::CC_LDIR);
	} else {
		T::end(T::CC_LDI);
	}
}
template <class T> void CPUCore<T>::ldd()  { BLOCK_LD(-1, false); }
template <class T> void CPUCore<T>::ldi()  { BLOCK_LD( 1, false); }
template <class T> void CPUCore<T>::lddr() { BLOCK_LD(-1, true ); }
template <class T> void CPUCore<T>::ldir() { BLOCK_LD( 1, true ); }


// block IN
template <class T> inline void CPUCore<T>::BLOCK_IN(int increase, bool repeat)
{
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
		T::end(T::CC_INIR);
	} else {
		T::end(T::CC_INI);
	}
}
template <class T> void CPUCore<T>::ind()  { BLOCK_IN(-1, false); }
template <class T> void CPUCore<T>::ini()  { BLOCK_IN( 1, false); }
template <class T> void CPUCore<T>::indr() { BLOCK_IN(-1, true ); }
template <class T> void CPUCore<T>::inir() { BLOCK_IN( 1, true ); }


// block OUT
template <class T> inline void CPUCore<T>::BLOCK_OUT(int increase, bool repeat)
{
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
		T::end(T::CC_OTIR);
	} else {
		T::end(T::CC_OUTI);
	}
}
template <class T> void CPUCore<T>::outd() { BLOCK_OUT(-1, false); }
template <class T> void CPUCore<T>::outi() { BLOCK_OUT( 1, false); }
template <class T> void CPUCore<T>::otdr() { BLOCK_OUT(-1, true ); }
template <class T> void CPUCore<T>::otir() { BLOCK_OUT( 1, true ); }


// various
template <class T> void CPUCore<T>::nop() { T::end(T::CC_NOP); }
template <class T> void CPUCore<T>::ccf()
{
	R.setF(((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	       ((R.getF() & C_FLAG) << 4) | // H_FLAG
	       (R.getA() & (X_FLAG | Y_FLAG))                  ) ^ C_FLAG);
	T::end(T::CC_CCF);
}
template <class T> void CPUCore<T>::cpl()
{
	R.setA(R.getA() ^ 0xFF);
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	       H_FLAG | N_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));
	T::end(T::CC_CPL);

}
template <class T> void CPUCore<T>::daa()
{
	unsigned i = R.getA();
	i |= (R.getF() & (C_FLAG | N_FLAG)) << 8; // 0x100 0x200
	i |= (R.getF() &  H_FLAG)           << 6; // 0x400
	R.setAF(DAATable[i]);
	T::end(T::CC_DAA);
}
template <class T> void CPUCore<T>::neg()
{
	 byte i = R.getA();
	 R.setA(0);
	 SUB(i);
	T::end(T::CC_NEG);
}
template <class T> void CPUCore<T>::scf()
{
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       C_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));
	T::end(T::CC_SCF);
}

template <class T> void CPUCore<T>::ex_af_af()
{
	unsigned t = R.getAF2(); R.setAF2(R.getAF()); R.setAF(t);
	T::end(T::CC_EX);
}
template <class T> void CPUCore<T>::ex_de_hl()
{
	unsigned t = R.getDE(); R.setDE(R.getHL()); R.setHL(t);
	T::end(T::CC_EX);
}
template <class T> void CPUCore<T>::exx()
{
	unsigned t1 = R.getBC2(); R.setBC2(R.getBC()); R.setBC(t1);
	unsigned t2 = R.getDE2(); R.setDE2(R.getDE()); R.setDE(t2);
	unsigned t3 = R.getHL2(); R.setHL2(R.getHL()); R.setHL(t3);
	T::end(T::CC_EX);
}

template <class T> void CPUCore<T>::di()
{
	R.di();
	T::end(T::CC_DI);
}
template <class T> void CPUCore<T>::ei()
{
	R.setIFF1(false);	// no ints after this instruction
	R.setNextIFF1(true);	// but allow them after next instruction
	R.setIFF2(true);
	setSlowInstructions();
	T::end(T::CC_EI);
}
template <class T> void CPUCore<T>::halt()
{
	R.setHALT(true);
	setSlowInstructions();

	if (!(R.getIFF1() || R.getNextIFF1() || R.getIFF2())) {
		motherboard.getMSXCliComm().printWarning(
			"DI; HALT detected, which means a hang. "
			"You can just as well reset the MSX now...\n");
	}
	T::end(T::CC_HALT);
}
template <class T> void CPUCore<T>::im_0() { R.setIM(0); T::end(T::CC_IM); }
template <class T> void CPUCore<T>::im_1() { R.setIM(1); T::end(T::CC_IM); }
template <class T> void CPUCore<T>::im_2() { R.setIM(2); T::end(T::CC_IM); }

// LD A,I/R
template <class T> void CPUCore<T>::ld_a_i()
{
	R.setA(R.getI());
	R.setF((R.getF() & C_FLAG) |
	       ZSXYTable[R.getA()] |
	       (R.getIFF2() ? V_FLAG : 0));
	T::end(T::CC_LD_A_I);
}
template <class T> void CPUCore<T>::ld_a_r()
{
	R.setA(R.getR());
	R.setF((R.getF() & C_FLAG) |
	       ZSXYTable[R.getA()] |
	       (R.getIFF2() ? V_FLAG : 0));
	T::end(T::CC_LD_A_I);
}

// LD I/R,A
template <class T> void CPUCore<T>::ld_i_a()
{
	R.setI(R.getA());
	T::end(T::CC_LD_A_I);
}
template <class T> void CPUCore<T>::ld_r_a()
{
	R.setR(R.getA());
	T::end(T::CC_LD_A_I);
}

// MULUB A,r
template <class T> inline void CPUCore<T>::MULUB(byte reg)
{
	// TODO check flags
	R.setHL(unsigned(R.getA()) * reg);
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (R.getHL() ? 0 : Z_FLAG) |
	       ((R.getHL() & 0x8000) ? C_FLAG : 0));
	T::end(T::CC_MULUB);
}
//template <class T> void CPUCore<T>::mulub_a_xhl()   { } // TODO
//template <class T> void CPUCore<T>::mulub_a_a()     { } // TODO
template <class T> void CPUCore<T>::mulub_a_b()   { MULUB(R.getB()); }
template <class T> void CPUCore<T>::mulub_a_c()   { MULUB(R.getC()); }
template <class T> void CPUCore<T>::mulub_a_d()   { MULUB(R.getD()); }
template <class T> void CPUCore<T>::mulub_a_e()   { MULUB(R.getE()); }
//template <class T> void CPUCore<T>::mulub_a_h()     { } // TODO
//template <class T> void CPUCore<T>::mulub_a_l()     { } // TODO

// MULUW HL,ss
template <class T> inline void CPUCore<T>::MULUW(unsigned reg)
{
	// TODO check flags
	unsigned res = unsigned(R.getHL()) * reg;
	R.setDE(res >> 16);
	R.setHL(res & 0xffff);
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (res ? 0 : Z_FLAG) |
	       ((res & 0x80000000) ? C_FLAG : 0));
	T::end(T::CC_MULUW);
}
template <class T> void CPUCore<T>::muluw_hl_bc() { MULUW(R.getBC()); }
//template <class T> void CPUCore<T>::muluw_hl_de()   { } // TODO
//template <class T> void CPUCore<T>::muluw_hl_hl()   { } // TODO
template <class T> void CPUCore<T>::muluw_hl_sp() { MULUW(R.getSP()); }


// prefixes
template <class T> void CPUCore<T>::dd_cb()
{
	unsigned tmp = RD_WORD_PC(T::CC_DD_CB);
	offset ofst = tmp & 0xFF;
	unsigned opcode = tmp >> 8;
	switch (opcode) {
		case 0x00: rlc_xix_b(ofst); break;
		case 0x01: rlc_xix_c(ofst); break;
		case 0x02: rlc_xix_d(ofst); break;
		case 0x03: rlc_xix_e(ofst); break;
		case 0x04: rlc_xix_h(ofst); break;
		case 0x05: rlc_xix_l(ofst); break;
		case 0x06: rlc_xix(ofst); break;
		case 0x07: rlc_xix_a(ofst); break;
		case 0x08: rrc_xix_b(ofst); break;
		case 0x09: rrc_xix_c(ofst); break;
		case 0x0a: rrc_xix_d(ofst); break;
		case 0x0b: rrc_xix_e(ofst); break;
		case 0x0c: rrc_xix_h(ofst); break;
		case 0x0d: rrc_xix_l(ofst); break;
		case 0x0e: rrc_xix(ofst); break;
		case 0x0f: rrc_xix_a(ofst); break;
		case 0x10: rl_xix_b(ofst); break;
		case 0x11: rl_xix_c(ofst); break;
		case 0x12: rl_xix_d(ofst); break;
		case 0x13: rl_xix_e(ofst); break;
		case 0x14: rl_xix_h(ofst); break;
		case 0x15: rl_xix_l(ofst); break;
		case 0x16: rl_xix(ofst); break;
		case 0x17: rl_xix_a(ofst); break;
		case 0x18: rr_xix_b(ofst); break;
		case 0x19: rr_xix_c(ofst); break;
		case 0x1a: rr_xix_d(ofst); break;
		case 0x1b: rr_xix_e(ofst); break;
		case 0x1c: rr_xix_h(ofst); break;
		case 0x1d: rr_xix_l(ofst); break;
		case 0x1e: rr_xix(ofst); break;
		case 0x1f: rr_xix_a(ofst); break;
		case 0x20: sla_xix_b(ofst); break;
		case 0x21: sla_xix_c(ofst); break;
		case 0x22: sla_xix_d(ofst); break;
		case 0x23: sla_xix_e(ofst); break;
		case 0x24: sla_xix_h(ofst); break;
		case 0x25: sla_xix_l(ofst); break;
		case 0x26: sla_xix(ofst); break;
		case 0x27: sla_xix_a(ofst); break;
		case 0x28: sra_xix_b(ofst); break;
		case 0x29: sra_xix_c(ofst); break;
		case 0x2a: sra_xix_d(ofst); break;
		case 0x2b: sra_xix_e(ofst); break;
		case 0x2c: sra_xix_h(ofst); break;
		case 0x2d: sra_xix_l(ofst); break;
		case 0x2e: sra_xix(ofst); break;
		case 0x2f: sra_xix_a(ofst); break;
		case 0x30: sll_xix_b(ofst); break;
		case 0x31: sll_xix_c(ofst); break;
		case 0x32: sll_xix_d(ofst); break;
		case 0x33: sll_xix_e(ofst); break;
		case 0x34: sll_xix_h(ofst); break;
		case 0x35: sll_xix_l(ofst); break;
		case 0x36: sll_xix(ofst); break;
		case 0x37: sll_xix_a(ofst); break;
		case 0x38: srl_xix_b(ofst); break;
		case 0x39: srl_xix_c(ofst); break;
		case 0x3a: srl_xix_d(ofst); break;
		case 0x3b: srl_xix_e(ofst); break;
		case 0x3c: srl_xix_h(ofst); break;
		case 0x3d: srl_xix_l(ofst); break;
		case 0x3e: srl_xix(ofst); break;
		case 0x3f: srl_xix_a(ofst); break;

		case 0x40: bit_0_xix(ofst); break;
		case 0x41: bit_0_xix(ofst); break;
		case 0x42: bit_0_xix(ofst); break;
		case 0x43: bit_0_xix(ofst); break;
		case 0x44: bit_0_xix(ofst); break;
		case 0x45: bit_0_xix(ofst); break;
		case 0x46: bit_0_xix(ofst); break;
		case 0x47: bit_0_xix(ofst); break;
		case 0x48: bit_1_xix(ofst); break;
		case 0x49: bit_1_xix(ofst); break;
		case 0x4a: bit_1_xix(ofst); break;
		case 0x4b: bit_1_xix(ofst); break;
		case 0x4c: bit_1_xix(ofst); break;
		case 0x4d: bit_1_xix(ofst); break;
		case 0x4e: bit_1_xix(ofst); break;
		case 0x4f: bit_1_xix(ofst); break;
		case 0x50: bit_2_xix(ofst); break;
		case 0x51: bit_2_xix(ofst); break;
		case 0x52: bit_2_xix(ofst); break;
		case 0x53: bit_2_xix(ofst); break;
		case 0x54: bit_2_xix(ofst); break;
		case 0x55: bit_2_xix(ofst); break;
		case 0x56: bit_2_xix(ofst); break;
		case 0x57: bit_2_xix(ofst); break;
		case 0x58: bit_3_xix(ofst); break;
		case 0x59: bit_3_xix(ofst); break;
		case 0x5a: bit_3_xix(ofst); break;
		case 0x5b: bit_3_xix(ofst); break;
		case 0x5c: bit_3_xix(ofst); break;
		case 0x5d: bit_3_xix(ofst); break;
		case 0x5e: bit_3_xix(ofst); break;
		case 0x5f: bit_3_xix(ofst); break;
		case 0x60: bit_4_xix(ofst); break;
		case 0x61: bit_4_xix(ofst); break;
		case 0x62: bit_4_xix(ofst); break;
		case 0x63: bit_4_xix(ofst); break;
		case 0x64: bit_4_xix(ofst); break;
		case 0x65: bit_4_xix(ofst); break;
		case 0x66: bit_4_xix(ofst); break;
		case 0x67: bit_4_xix(ofst); break;
		case 0x68: bit_5_xix(ofst); break;
		case 0x69: bit_5_xix(ofst); break;
		case 0x6a: bit_5_xix(ofst); break;
		case 0x6b: bit_5_xix(ofst); break;
		case 0x6c: bit_5_xix(ofst); break;
		case 0x6d: bit_5_xix(ofst); break;
		case 0x6e: bit_5_xix(ofst); break;
		case 0x6f: bit_5_xix(ofst); break;
		case 0x70: bit_6_xix(ofst); break;
		case 0x71: bit_6_xix(ofst); break;
		case 0x72: bit_6_xix(ofst); break;
		case 0x73: bit_6_xix(ofst); break;
		case 0x74: bit_6_xix(ofst); break;
		case 0x75: bit_6_xix(ofst); break;
		case 0x76: bit_6_xix(ofst); break;
		case 0x77: bit_6_xix(ofst); break;
		case 0x78: bit_7_xix(ofst); break;
		case 0x79: bit_7_xix(ofst); break;
		case 0x7a: bit_7_xix(ofst); break;
		case 0x7b: bit_7_xix(ofst); break;
		case 0x7c: bit_7_xix(ofst); break;
		case 0x7d: bit_7_xix(ofst); break;
		case 0x7e: bit_7_xix(ofst); break;
		case 0x7f: bit_7_xix(ofst); break;

		case 0x80: res_0_xix_b(ofst); break;
		case 0x81: res_0_xix_c(ofst); break;
		case 0x82: res_0_xix_d(ofst); break;
		case 0x83: res_0_xix_e(ofst); break;
		case 0x84: res_0_xix_h(ofst); break;
		case 0x85: res_0_xix_l(ofst); break;
		case 0x86: res_0_xix(ofst); break;
		case 0x87: res_0_xix_a(ofst); break;
		case 0x88: res_1_xix_b(ofst); break;
		case 0x89: res_1_xix_c(ofst); break;
		case 0x8a: res_1_xix_d(ofst); break;
		case 0x8b: res_1_xix_e(ofst); break;
		case 0x8c: res_1_xix_h(ofst); break;
		case 0x8d: res_1_xix_l(ofst); break;
		case 0x8e: res_1_xix(ofst); break;
		case 0x8f: res_1_xix_a(ofst); break;
		case 0x90: res_2_xix_b(ofst); break;
		case 0x91: res_2_xix_c(ofst); break;
		case 0x92: res_2_xix_d(ofst); break;
		case 0x93: res_2_xix_e(ofst); break;
		case 0x94: res_2_xix_h(ofst); break;
		case 0x95: res_2_xix_l(ofst); break;
		case 0x96: res_2_xix(ofst); break;
		case 0x97: res_2_xix_a(ofst); break;
		case 0x98: res_3_xix_b(ofst); break;
		case 0x99: res_3_xix_c(ofst); break;
		case 0x9a: res_3_xix_d(ofst); break;
		case 0x9b: res_3_xix_e(ofst); break;
		case 0x9c: res_3_xix_h(ofst); break;
		case 0x9d: res_3_xix_l(ofst); break;
		case 0x9e: res_3_xix(ofst); break;
		case 0x9f: res_3_xix_a(ofst); break;
		case 0xa0: res_4_xix_b(ofst); break;
		case 0xa1: res_4_xix_c(ofst); break;
		case 0xa2: res_4_xix_d(ofst); break;
		case 0xa3: res_4_xix_e(ofst); break;
		case 0xa4: res_4_xix_h(ofst); break;
		case 0xa5: res_4_xix_l(ofst); break;
		case 0xa6: res_4_xix(ofst); break;
		case 0xa7: res_4_xix_a(ofst); break;
		case 0xa8: res_5_xix_b(ofst); break;
		case 0xa9: res_5_xix_c(ofst); break;
		case 0xaa: res_5_xix_d(ofst); break;
		case 0xab: res_5_xix_e(ofst); break;
		case 0xac: res_5_xix_h(ofst); break;
		case 0xad: res_5_xix_l(ofst); break;
		case 0xae: res_5_xix(ofst); break;
		case 0xaf: res_5_xix_a(ofst); break;
		case 0xb0: res_6_xix_b(ofst); break;
		case 0xb1: res_6_xix_c(ofst); break;
		case 0xb2: res_6_xix_d(ofst); break;
		case 0xb3: res_6_xix_e(ofst); break;
		case 0xb4: res_6_xix_h(ofst); break;
		case 0xb5: res_6_xix_l(ofst); break;
		case 0xb6: res_6_xix(ofst); break;
		case 0xb7: res_6_xix_a(ofst); break;
		case 0xb8: res_7_xix_b(ofst); break;
		case 0xb9: res_7_xix_c(ofst); break;
		case 0xba: res_7_xix_d(ofst); break;
		case 0xbb: res_7_xix_e(ofst); break;
		case 0xbc: res_7_xix_h(ofst); break;
		case 0xbd: res_7_xix_l(ofst); break;
		case 0xbe: res_7_xix(ofst); break;
		case 0xbf: res_7_xix_a(ofst); break;

		case 0xc0: set_0_xix_b(ofst); break;
		case 0xc1: set_0_xix_c(ofst); break;
		case 0xc2: set_0_xix_d(ofst); break;
		case 0xc3: set_0_xix_e(ofst); break;
		case 0xc4: set_0_xix_h(ofst); break;
		case 0xc5: set_0_xix_l(ofst); break;
		case 0xc6: set_0_xix(ofst); break;
		case 0xc7: set_0_xix_a(ofst); break;
		case 0xc8: set_1_xix_b(ofst); break;
		case 0xc9: set_1_xix_c(ofst); break;
		case 0xca: set_1_xix_d(ofst); break;
		case 0xcb: set_1_xix_e(ofst); break;
		case 0xcc: set_1_xix_h(ofst); break;
		case 0xcd: set_1_xix_l(ofst); break;
		case 0xce: set_1_xix(ofst); break;
		case 0xcf: set_1_xix_a(ofst); break;
		case 0xd0: set_2_xix_b(ofst); break;
		case 0xd1: set_2_xix_c(ofst); break;
		case 0xd2: set_2_xix_d(ofst); break;
		case 0xd3: set_2_xix_e(ofst); break;
		case 0xd4: set_2_xix_h(ofst); break;
		case 0xd5: set_2_xix_l(ofst); break;
		case 0xd6: set_2_xix(ofst); break;
		case 0xd7: set_2_xix_a(ofst); break;
		case 0xd8: set_3_xix_b(ofst); break;
		case 0xd9: set_3_xix_c(ofst); break;
		case 0xda: set_3_xix_d(ofst); break;
		case 0xdb: set_3_xix_e(ofst); break;
		case 0xdc: set_3_xix_h(ofst); break;
		case 0xdd: set_3_xix_l(ofst); break;
		case 0xde: set_3_xix(ofst); break;
		case 0xdf: set_3_xix_a(ofst); break;
		case 0xe0: set_4_xix_b(ofst); break;
		case 0xe1: set_4_xix_c(ofst); break;
		case 0xe2: set_4_xix_d(ofst); break;
		case 0xe3: set_4_xix_e(ofst); break;
		case 0xe4: set_4_xix_h(ofst); break;
		case 0xe5: set_4_xix_l(ofst); break;
		case 0xe6: set_4_xix(ofst); break;
		case 0xe7: set_4_xix_a(ofst); break;
		case 0xe8: set_5_xix_b(ofst); break;
		case 0xe9: set_5_xix_c(ofst); break;
		case 0xea: set_5_xix_d(ofst); break;
		case 0xeb: set_5_xix_e(ofst); break;
		case 0xec: set_5_xix_h(ofst); break;
		case 0xed: set_5_xix_l(ofst); break;
		case 0xee: set_5_xix(ofst); break;
		case 0xef: set_5_xix_a(ofst); break;
		case 0xf0: set_6_xix_b(ofst); break;
		case 0xf1: set_6_xix_c(ofst); break;
		case 0xf2: set_6_xix_d(ofst); break;
		case 0xf3: set_6_xix_e(ofst); break;
		case 0xf4: set_6_xix_h(ofst); break;
		case 0xf5: set_6_xix_l(ofst); break;
		case 0xf6: set_6_xix(ofst); break;
		case 0xf7: set_6_xix_a(ofst); break;
		case 0xf8: set_7_xix_b(ofst); break;
		case 0xf9: set_7_xix_c(ofst); break;
		case 0xfa: set_7_xix_d(ofst); break;
		case 0xfb: set_7_xix_e(ofst); break;
		case 0xfc: set_7_xix_h(ofst); break;
		case 0xfd: set_7_xix_l(ofst); break;
		case 0xfe: set_7_xix(ofst); break;
		case 0xff: set_7_xix_a(ofst); break;
	}
}

template <class T> void CPUCore<T>::fd_cb()
{
	unsigned tmp = RD_WORD_PC(T::CC_DD_CB);
	offset ofst = tmp & 0xFF;
	unsigned opcode = tmp >> 8;
	switch (opcode) {
		case 0x00: rlc_xiy_b(ofst); break;
		case 0x01: rlc_xiy_c(ofst); break;
		case 0x02: rlc_xiy_d(ofst); break;
		case 0x03: rlc_xiy_e(ofst); break;
		case 0x04: rlc_xiy_h(ofst); break;
		case 0x05: rlc_xiy_l(ofst); break;
		case 0x06: rlc_xiy(ofst); break;
		case 0x07: rlc_xiy_a(ofst); break;
		case 0x08: rrc_xiy_b(ofst); break;
		case 0x09: rrc_xiy_c(ofst); break;
		case 0x0a: rrc_xiy_d(ofst); break;
		case 0x0b: rrc_xiy_e(ofst); break;
		case 0x0c: rrc_xiy_h(ofst); break;
		case 0x0d: rrc_xiy_l(ofst); break;
		case 0x0e: rrc_xiy(ofst); break;
		case 0x0f: rrc_xiy_a(ofst); break;
		case 0x10: rl_xiy_b(ofst); break;
		case 0x11: rl_xiy_c(ofst); break;
		case 0x12: rl_xiy_d(ofst); break;
		case 0x13: rl_xiy_e(ofst); break;
		case 0x14: rl_xiy_h(ofst); break;
		case 0x15: rl_xiy_l(ofst); break;
		case 0x16: rl_xiy(ofst); break;
		case 0x17: rl_xiy_a(ofst); break;
		case 0x18: rr_xiy_b(ofst); break;
		case 0x19: rr_xiy_c(ofst); break;
		case 0x1a: rr_xiy_d(ofst); break;
		case 0x1b: rr_xiy_e(ofst); break;
		case 0x1c: rr_xiy_h(ofst); break;
		case 0x1d: rr_xiy_l(ofst); break;
		case 0x1e: rr_xiy(ofst); break;
		case 0x1f: rr_xiy_a(ofst); break;
		case 0x20: sla_xiy_b(ofst); break;
		case 0x21: sla_xiy_c(ofst); break;
		case 0x22: sla_xiy_d(ofst); break;
		case 0x23: sla_xiy_e(ofst); break;
		case 0x24: sla_xiy_h(ofst); break;
		case 0x25: sla_xiy_l(ofst); break;
		case 0x26: sla_xiy(ofst); break;
		case 0x27: sla_xiy_a(ofst); break;
		case 0x28: sra_xiy_b(ofst); break;
		case 0x29: sra_xiy_c(ofst); break;
		case 0x2a: sra_xiy_d(ofst); break;
		case 0x2b: sra_xiy_e(ofst); break;
		case 0x2c: sra_xiy_h(ofst); break;
		case 0x2d: sra_xiy_l(ofst); break;
		case 0x2e: sra_xiy(ofst); break;
		case 0x2f: sra_xiy_a(ofst); break;
		case 0x30: sll_xiy_b(ofst); break;
		case 0x31: sll_xiy_c(ofst); break;
		case 0x32: sll_xiy_d(ofst); break;
		case 0x33: sll_xiy_e(ofst); break;
		case 0x34: sll_xiy_h(ofst); break;
		case 0x35: sll_xiy_l(ofst); break;
		case 0x36: sll_xiy(ofst); break;
		case 0x37: sll_xiy_a(ofst); break;
		case 0x38: srl_xiy_b(ofst); break;
		case 0x39: srl_xiy_c(ofst); break;
		case 0x3a: srl_xiy_d(ofst); break;
		case 0x3b: srl_xiy_e(ofst); break;
		case 0x3c: srl_xiy_h(ofst); break;
		case 0x3d: srl_xiy_l(ofst); break;
		case 0x3e: srl_xiy(ofst); break;
		case 0x3f: srl_xiy_a(ofst); break;

		case 0x40: bit_0_xiy(ofst); break;
		case 0x41: bit_0_xiy(ofst); break;
		case 0x42: bit_0_xiy(ofst); break;
		case 0x43: bit_0_xiy(ofst); break;
		case 0x44: bit_0_xiy(ofst); break;
		case 0x45: bit_0_xiy(ofst); break;
		case 0x46: bit_0_xiy(ofst); break;
		case 0x47: bit_0_xiy(ofst); break;
		case 0x48: bit_1_xiy(ofst); break;
		case 0x49: bit_1_xiy(ofst); break;
		case 0x4a: bit_1_xiy(ofst); break;
		case 0x4b: bit_1_xiy(ofst); break;
		case 0x4c: bit_1_xiy(ofst); break;
		case 0x4d: bit_1_xiy(ofst); break;
		case 0x4e: bit_1_xiy(ofst); break;
		case 0x4f: bit_1_xiy(ofst); break;
		case 0x50: bit_2_xiy(ofst); break;
		case 0x51: bit_2_xiy(ofst); break;
		case 0x52: bit_2_xiy(ofst); break;
		case 0x53: bit_2_xiy(ofst); break;
		case 0x54: bit_2_xiy(ofst); break;
		case 0x55: bit_2_xiy(ofst); break;
		case 0x56: bit_2_xiy(ofst); break;
		case 0x57: bit_2_xiy(ofst); break;
		case 0x58: bit_3_xiy(ofst); break;
		case 0x59: bit_3_xiy(ofst); break;
		case 0x5a: bit_3_xiy(ofst); break;
		case 0x5b: bit_3_xiy(ofst); break;
		case 0x5c: bit_3_xiy(ofst); break;
		case 0x5d: bit_3_xiy(ofst); break;
		case 0x5e: bit_3_xiy(ofst); break;
		case 0x5f: bit_3_xiy(ofst); break;
		case 0x60: bit_4_xiy(ofst); break;
		case 0x61: bit_4_xiy(ofst); break;
		case 0x62: bit_4_xiy(ofst); break;
		case 0x63: bit_4_xiy(ofst); break;
		case 0x64: bit_4_xiy(ofst); break;
		case 0x65: bit_4_xiy(ofst); break;
		case 0x66: bit_4_xiy(ofst); break;
		case 0x67: bit_4_xiy(ofst); break;
		case 0x68: bit_5_xiy(ofst); break;
		case 0x69: bit_5_xiy(ofst); break;
		case 0x6a: bit_5_xiy(ofst); break;
		case 0x6b: bit_5_xiy(ofst); break;
		case 0x6c: bit_5_xiy(ofst); break;
		case 0x6d: bit_5_xiy(ofst); break;
		case 0x6e: bit_5_xiy(ofst); break;
		case 0x6f: bit_5_xiy(ofst); break;
		case 0x70: bit_6_xiy(ofst); break;
		case 0x71: bit_6_xiy(ofst); break;
		case 0x72: bit_6_xiy(ofst); break;
		case 0x73: bit_6_xiy(ofst); break;
		case 0x74: bit_6_xiy(ofst); break;
		case 0x75: bit_6_xiy(ofst); break;
		case 0x76: bit_6_xiy(ofst); break;
		case 0x77: bit_6_xiy(ofst); break;
		case 0x78: bit_7_xiy(ofst); break;
		case 0x79: bit_7_xiy(ofst); break;
		case 0x7a: bit_7_xiy(ofst); break;
		case 0x7b: bit_7_xiy(ofst); break;
		case 0x7c: bit_7_xiy(ofst); break;
		case 0x7d: bit_7_xiy(ofst); break;
		case 0x7e: bit_7_xiy(ofst); break;
		case 0x7f: bit_7_xiy(ofst); break;

		case 0x80: res_0_xiy_b(ofst); break;
		case 0x81: res_0_xiy_c(ofst); break;
		case 0x82: res_0_xiy_d(ofst); break;
		case 0x83: res_0_xiy_e(ofst); break;
		case 0x84: res_0_xiy_h(ofst); break;
		case 0x85: res_0_xiy_l(ofst); break;
		case 0x86: res_0_xiy(ofst); break;
		case 0x87: res_0_xiy_a(ofst); break;
		case 0x88: res_1_xiy_b(ofst); break;
		case 0x89: res_1_xiy_c(ofst); break;
		case 0x8a: res_1_xiy_d(ofst); break;
		case 0x8b: res_1_xiy_e(ofst); break;
		case 0x8c: res_1_xiy_h(ofst); break;
		case 0x8d: res_1_xiy_l(ofst); break;
		case 0x8e: res_1_xiy(ofst); break;
		case 0x8f: res_1_xiy_a(ofst); break;
		case 0x90: res_2_xiy_b(ofst); break;
		case 0x91: res_2_xiy_c(ofst); break;
		case 0x92: res_2_xiy_d(ofst); break;
		case 0x93: res_2_xiy_e(ofst); break;
		case 0x94: res_2_xiy_h(ofst); break;
		case 0x95: res_2_xiy_l(ofst); break;
		case 0x96: res_2_xiy(ofst); break;
		case 0x97: res_2_xiy_a(ofst); break;
		case 0x98: res_3_xiy_b(ofst); break;
		case 0x99: res_3_xiy_c(ofst); break;
		case 0x9a: res_3_xiy_d(ofst); break;
		case 0x9b: res_3_xiy_e(ofst); break;
		case 0x9c: res_3_xiy_h(ofst); break;
		case 0x9d: res_3_xiy_l(ofst); break;
		case 0x9e: res_3_xiy(ofst); break;
		case 0x9f: res_3_xiy_a(ofst); break;
		case 0xa0: res_4_xiy_b(ofst); break;
		case 0xa1: res_4_xiy_c(ofst); break;
		case 0xa2: res_4_xiy_d(ofst); break;
		case 0xa3: res_4_xiy_e(ofst); break;
		case 0xa4: res_4_xiy_h(ofst); break;
		case 0xa5: res_4_xiy_l(ofst); break;
		case 0xa6: res_4_xiy(ofst); break;
		case 0xa7: res_4_xiy_a(ofst); break;
		case 0xa8: res_5_xiy_b(ofst); break;
		case 0xa9: res_5_xiy_c(ofst); break;
		case 0xaa: res_5_xiy_d(ofst); break;
		case 0xab: res_5_xiy_e(ofst); break;
		case 0xac: res_5_xiy_h(ofst); break;
		case 0xad: res_5_xiy_l(ofst); break;
		case 0xae: res_5_xiy(ofst); break;
		case 0xaf: res_5_xiy_a(ofst); break;
		case 0xb0: res_6_xiy_b(ofst); break;
		case 0xb1: res_6_xiy_c(ofst); break;
		case 0xb2: res_6_xiy_d(ofst); break;
		case 0xb3: res_6_xiy_e(ofst); break;
		case 0xb4: res_6_xiy_h(ofst); break;
		case 0xb5: res_6_xiy_l(ofst); break;
		case 0xb6: res_6_xiy(ofst); break;
		case 0xb7: res_6_xiy_a(ofst); break;
		case 0xb8: res_7_xiy_b(ofst); break;
		case 0xb9: res_7_xiy_c(ofst); break;
		case 0xba: res_7_xiy_d(ofst); break;
		case 0xbb: res_7_xiy_e(ofst); break;
		case 0xbc: res_7_xiy_h(ofst); break;
		case 0xbd: res_7_xiy_l(ofst); break;
		case 0xbe: res_7_xiy(ofst); break;
		case 0xbf: res_7_xiy_a(ofst); break;

		case 0xc0: set_0_xiy_b(ofst); break;
		case 0xc1: set_0_xiy_c(ofst); break;
		case 0xc2: set_0_xiy_d(ofst); break;
		case 0xc3: set_0_xiy_e(ofst); break;
		case 0xc4: set_0_xiy_h(ofst); break;
		case 0xc5: set_0_xiy_l(ofst); break;
		case 0xc6: set_0_xiy(ofst); break;
		case 0xc7: set_0_xiy_a(ofst); break;
		case 0xc8: set_1_xiy_b(ofst); break;
		case 0xc9: set_1_xiy_c(ofst); break;
		case 0xca: set_1_xiy_d(ofst); break;
		case 0xcb: set_1_xiy_e(ofst); break;
		case 0xcc: set_1_xiy_h(ofst); break;
		case 0xcd: set_1_xiy_l(ofst); break;
		case 0xce: set_1_xiy(ofst); break;
		case 0xcf: set_1_xiy_a(ofst); break;
		case 0xd0: set_2_xiy_b(ofst); break;
		case 0xd1: set_2_xiy_c(ofst); break;
		case 0xd2: set_2_xiy_d(ofst); break;
		case 0xd3: set_2_xiy_e(ofst); break;
		case 0xd4: set_2_xiy_h(ofst); break;
		case 0xd5: set_2_xiy_l(ofst); break;
		case 0xd6: set_2_xiy(ofst); break;
		case 0xd7: set_2_xiy_a(ofst); break;
		case 0xd8: set_3_xiy_b(ofst); break;
		case 0xd9: set_3_xiy_c(ofst); break;
		case 0xda: set_3_xiy_d(ofst); break;
		case 0xdb: set_3_xiy_e(ofst); break;
		case 0xdc: set_3_xiy_h(ofst); break;
		case 0xdd: set_3_xiy_l(ofst); break;
		case 0xde: set_3_xiy(ofst); break;
		case 0xdf: set_3_xiy_a(ofst); break;
		case 0xe0: set_4_xiy_b(ofst); break;
		case 0xe1: set_4_xiy_c(ofst); break;
		case 0xe2: set_4_xiy_d(ofst); break;
		case 0xe3: set_4_xiy_e(ofst); break;
		case 0xe4: set_4_xiy_h(ofst); break;
		case 0xe5: set_4_xiy_l(ofst); break;
		case 0xe6: set_4_xiy(ofst); break;
		case 0xe7: set_4_xiy_a(ofst); break;
		case 0xe8: set_5_xiy_b(ofst); break;
		case 0xe9: set_5_xiy_c(ofst); break;
		case 0xea: set_5_xiy_d(ofst); break;
		case 0xeb: set_5_xiy_e(ofst); break;
		case 0xec: set_5_xiy_h(ofst); break;
		case 0xed: set_5_xiy_l(ofst); break;
		case 0xee: set_5_xiy(ofst); break;
		case 0xef: set_5_xiy_a(ofst); break;
		case 0xf0: set_6_xiy_b(ofst); break;
		case 0xf1: set_6_xiy_c(ofst); break;
		case 0xf2: set_6_xiy_d(ofst); break;
		case 0xf3: set_6_xiy_e(ofst); break;
		case 0xf4: set_6_xiy_h(ofst); break;
		case 0xf5: set_6_xiy_l(ofst); break;
		case 0xf6: set_6_xiy(ofst); break;
		case 0xf7: set_6_xiy_a(ofst); break;
		case 0xf8: set_7_xiy_b(ofst); break;
		case 0xf9: set_7_xiy_c(ofst); break;
		case 0xfa: set_7_xiy_d(ofst); break;
		case 0xfb: set_7_xiy_e(ofst); break;
		case 0xfc: set_7_xiy_h(ofst); break;
		case 0xfd: set_7_xiy_l(ofst); break;
		case 0xfe: set_7_xiy(ofst); break;
		case 0xff: set_7_xiy_a(ofst); break;
	}
}

template <class T> void CPUCore<T>::cb()
{
	byte opcode = RDMEM_OPCODE(T::CC_PREFIX);
	M1Cycle();
	switch (opcode) {
		case 0x00: rlc_b(); break;
		case 0x01: rlc_c(); break;
		case 0x02: rlc_d(); break;
		case 0x03: rlc_e(); break;
		case 0x04: rlc_h(); break;
		case 0x05: rlc_l(); break;
		case 0x06: rlc_xhl(); break;
		case 0x07: rlc_a(); break;
		case 0x08: rrc_b(); break;
		case 0x09: rrc_c(); break;
		case 0x0a: rrc_d(); break;
		case 0x0b: rrc_e(); break;
		case 0x0c: rrc_h(); break;
		case 0x0d: rrc_l(); break;
		case 0x0e: rrc_xhl(); break;
		case 0x0f: rrc_a(); break;
		case 0x10: rl_b(); break;
		case 0x11: rl_c(); break;
		case 0x12: rl_d(); break;
		case 0x13: rl_e(); break;
		case 0x14: rl_h(); break;
		case 0x15: rl_l(); break;
		case 0x16: rl_xhl(); break;
		case 0x17: rl_a(); break;
		case 0x18: rr_b(); break;
		case 0x19: rr_c(); break;
		case 0x1a: rr_d(); break;
		case 0x1b: rr_e(); break;
		case 0x1c: rr_h(); break;
		case 0x1d: rr_l(); break;
		case 0x1e: rr_xhl(); break;
		case 0x1f: rr_a(); break;
		case 0x20: sla_b(); break;
		case 0x21: sla_c(); break;
		case 0x22: sla_d(); break;
		case 0x23: sla_e(); break;
		case 0x24: sla_h(); break;
		case 0x25: sla_l(); break;
		case 0x26: sla_xhl(); break;
		case 0x27: sla_a(); break;
		case 0x28: sra_b(); break;
		case 0x29: sra_c(); break;
		case 0x2a: sra_d(); break;
		case 0x2b: sra_e(); break;
		case 0x2c: sra_h(); break;
		case 0x2d: sra_l(); break;
		case 0x2e: sra_xhl(); break;
		case 0x2f: sra_a(); break;
		case 0x30: sll_b(); break;
		case 0x31: sll_c(); break;
		case 0x32: sll_d(); break;
		case 0x33: sll_e(); break;
		case 0x34: sll_h(); break;
		case 0x35: sll_l(); break;
		case 0x36: sll_xhl(); break;
		case 0x37: sll_a(); break;
		case 0x38: srl_b(); break;
		case 0x39: srl_c(); break;
		case 0x3a: srl_d(); break;
		case 0x3b: srl_e(); break;
		case 0x3c: srl_h(); break;
		case 0x3d: srl_l(); break;
		case 0x3e: srl_xhl(); break;
		case 0x3f: srl_a(); break;

		case 0x40: bit_0_b(); break;
		case 0x41: bit_0_c(); break;
		case 0x42: bit_0_d(); break;
		case 0x43: bit_0_e(); break;
		case 0x44: bit_0_h(); break;
		case 0x45: bit_0_l(); break;
		case 0x46: bit_0_xhl(); break;
		case 0x47: bit_0_a(); break;
		case 0x48: bit_1_b(); break;
		case 0x49: bit_1_c(); break;
		case 0x4a: bit_1_d(); break;
		case 0x4b: bit_1_e(); break;
		case 0x4c: bit_1_h(); break;
		case 0x4d: bit_1_l(); break;
		case 0x4e: bit_1_xhl(); break;
		case 0x4f: bit_1_a(); break;
		case 0x50: bit_2_b(); break;
		case 0x51: bit_2_c(); break;
		case 0x52: bit_2_d(); break;
		case 0x53: bit_2_e(); break;
		case 0x54: bit_2_h(); break;
		case 0x55: bit_2_l(); break;
		case 0x56: bit_2_xhl(); break;
		case 0x57: bit_2_a(); break;
		case 0x58: bit_3_b(); break;
		case 0x59: bit_3_c(); break;
		case 0x5a: bit_3_d(); break;
		case 0x5b: bit_3_e(); break;
		case 0x5c: bit_3_h(); break;
		case 0x5d: bit_3_l(); break;
		case 0x5e: bit_3_xhl(); break;
		case 0x5f: bit_3_a(); break;
		case 0x60: bit_4_b(); break;
		case 0x61: bit_4_c(); break;
		case 0x62: bit_4_d(); break;
		case 0x63: bit_4_e(); break;
		case 0x64: bit_4_h(); break;
		case 0x65: bit_4_l(); break;
		case 0x66: bit_4_xhl(); break;
		case 0x67: bit_4_a(); break;
		case 0x68: bit_5_b(); break;
		case 0x69: bit_5_c(); break;
		case 0x6a: bit_5_d(); break;
		case 0x6b: bit_5_e(); break;
		case 0x6c: bit_5_h(); break;
		case 0x6d: bit_5_l(); break;
		case 0x6e: bit_5_xhl(); break;
		case 0x6f: bit_5_a(); break;
		case 0x70: bit_6_b(); break;
		case 0x71: bit_6_c(); break;
		case 0x72: bit_6_d(); break;
		case 0x73: bit_6_e(); break;
		case 0x74: bit_6_h(); break;
		case 0x75: bit_6_l(); break;
		case 0x76: bit_6_xhl(); break;
		case 0x77: bit_6_a(); break;
		case 0x78: bit_7_b(); break;
		case 0x79: bit_7_c(); break;
		case 0x7a: bit_7_d(); break;
		case 0x7b: bit_7_e(); break;
		case 0x7c: bit_7_h(); break;
		case 0x7d: bit_7_l(); break;
		case 0x7e: bit_7_xhl(); break;
		case 0x7f: bit_7_a(); break;

		case 0x80: res_0_b(); break;
		case 0x81: res_0_c(); break;
		case 0x82: res_0_d(); break;
		case 0x83: res_0_e(); break;
		case 0x84: res_0_h(); break;
		case 0x85: res_0_l(); break;
		case 0x86: res_0_xhl(); break;
		case 0x87: res_0_a(); break;
		case 0x88: res_1_b(); break;
		case 0x89: res_1_c(); break;
		case 0x8a: res_1_d(); break;
		case 0x8b: res_1_e(); break;
		case 0x8c: res_1_h(); break;
		case 0x8d: res_1_l(); break;
		case 0x8e: res_1_xhl(); break;
		case 0x8f: res_1_a(); break;
		case 0x90: res_2_b(); break;
		case 0x91: res_2_c(); break;
		case 0x92: res_2_d(); break;
		case 0x93: res_2_e(); break;
		case 0x94: res_2_h(); break;
		case 0x95: res_2_l(); break;
		case 0x96: res_2_xhl(); break;
		case 0x97: res_2_a(); break;
		case 0x98: res_3_b(); break;
		case 0x99: res_3_c(); break;
		case 0x9a: res_3_d(); break;
		case 0x9b: res_3_e(); break;
		case 0x9c: res_3_h(); break;
		case 0x9d: res_3_l(); break;
		case 0x9e: res_3_xhl(); break;
		case 0x9f: res_3_a(); break;
		case 0xa0: res_4_b(); break;
		case 0xa1: res_4_c(); break;
		case 0xa2: res_4_d(); break;
		case 0xa3: res_4_e(); break;
		case 0xa4: res_4_h(); break;
		case 0xa5: res_4_l(); break;
		case 0xa6: res_4_xhl(); break;
		case 0xa7: res_4_a(); break;
		case 0xa8: res_5_b(); break;
		case 0xa9: res_5_c(); break;
		case 0xaa: res_5_d(); break;
		case 0xab: res_5_e(); break;
		case 0xac: res_5_h(); break;
		case 0xad: res_5_l(); break;
		case 0xae: res_5_xhl(); break;
		case 0xaf: res_5_a(); break;
		case 0xb0: res_6_b(); break;
		case 0xb1: res_6_c(); break;
		case 0xb2: res_6_d(); break;
		case 0xb3: res_6_e(); break;
		case 0xb4: res_6_h(); break;
		case 0xb5: res_6_l(); break;
		case 0xb6: res_6_xhl(); break;
		case 0xb7: res_6_a(); break;
		case 0xb8: res_7_b(); break;
		case 0xb9: res_7_c(); break;
		case 0xba: res_7_d(); break;
		case 0xbb: res_7_e(); break;
		case 0xbc: res_7_h(); break;
		case 0xbd: res_7_l(); break;
		case 0xbe: res_7_xhl(); break;
		case 0xbf: res_7_a(); break;

		case 0xc0: set_0_b(); break;
		case 0xc1: set_0_c(); break;
		case 0xc2: set_0_d(); break;
		case 0xc3: set_0_e(); break;
		case 0xc4: set_0_h(); break;
		case 0xc5: set_0_l(); break;
		case 0xc6: set_0_xhl(); break;
		case 0xc7: set_0_a(); break;
		case 0xc8: set_1_b(); break;
		case 0xc9: set_1_c(); break;
		case 0xca: set_1_d(); break;
		case 0xcb: set_1_e(); break;
		case 0xcc: set_1_h(); break;
		case 0xcd: set_1_l(); break;
		case 0xce: set_1_xhl(); break;
		case 0xcf: set_1_a(); break;
		case 0xd0: set_2_b(); break;
		case 0xd1: set_2_c(); break;
		case 0xd2: set_2_d(); break;
		case 0xd3: set_2_e(); break;
		case 0xd4: set_2_h(); break;
		case 0xd5: set_2_l(); break;
		case 0xd6: set_2_xhl(); break;
		case 0xd7: set_2_a(); break;
		case 0xd8: set_3_b(); break;
		case 0xd9: set_3_c(); break;
		case 0xda: set_3_d(); break;
		case 0xdb: set_3_e(); break;
		case 0xdc: set_3_h(); break;
		case 0xdd: set_3_l(); break;
		case 0xde: set_3_xhl(); break;
		case 0xdf: set_3_a(); break;
		case 0xe0: set_4_b(); break;
		case 0xe1: set_4_c(); break;
		case 0xe2: set_4_d(); break;
		case 0xe3: set_4_e(); break;
		case 0xe4: set_4_h(); break;
		case 0xe5: set_4_l(); break;
		case 0xe6: set_4_xhl(); break;
		case 0xe7: set_4_a(); break;
		case 0xe8: set_5_b(); break;
		case 0xe9: set_5_c(); break;
		case 0xea: set_5_d(); break;
		case 0xeb: set_5_e(); break;
		case 0xec: set_5_h(); break;
		case 0xed: set_5_l(); break;
		case 0xee: set_5_xhl(); break;
		case 0xef: set_5_a(); break;
		case 0xf0: set_6_b(); break;
		case 0xf1: set_6_c(); break;
		case 0xf2: set_6_d(); break;
		case 0xf3: set_6_e(); break;
		case 0xf4: set_6_h(); break;
		case 0xf5: set_6_l(); break;
		case 0xf6: set_6_xhl(); break;
		case 0xf7: set_6_a(); break;
		case 0xf8: set_7_b(); break;
		case 0xf9: set_7_c(); break;
		case 0xfa: set_7_d(); break;
		case 0xfb: set_7_e(); break;
		case 0xfc: set_7_h(); break;
		case 0xfd: set_7_l(); break;
		case 0xfe: set_7_xhl(); break;
		case 0xff: set_7_a(); break;
	}
}

template <class T> void CPUCore<T>::ed()
{
	byte opcode = RDMEM_OPCODE(T::CC_PREFIX);
	M1Cycle();
	switch (opcode) {
		case 0x00: nop(); break;
		case 0x01: nop(); break;
		case 0x02: nop(); break;
		case 0x03: nop(); break;
		case 0x04: nop(); break;
		case 0x05: nop(); break;
		case 0x06: nop(); break;
		case 0x07: nop(); break;
		case 0x08: nop(); break;
		case 0x09: nop(); break;
		case 0x0a: nop(); break;
		case 0x0b: nop(); break;
		case 0x0c: nop(); break;
		case 0x0d: nop(); break;
		case 0x0e: nop(); break;
		case 0x0f: nop(); break;
		case 0x10: nop(); break;
		case 0x11: nop(); break;
		case 0x12: nop(); break;
		case 0x13: nop(); break;
		case 0x14: nop(); break;
		case 0x15: nop(); break;
		case 0x16: nop(); break;
		case 0x17: nop(); break;
		case 0x18: nop(); break;
		case 0x19: nop(); break;
		case 0x1a: nop(); break;
		case 0x1b: nop(); break;
		case 0x1c: nop(); break;
		case 0x1d: nop(); break;
		case 0x1e: nop(); break;
		case 0x1f: nop(); break;
		case 0x20: nop(); break;
		case 0x21: nop(); break;
		case 0x22: nop(); break;
		case 0x23: nop(); break;
		case 0x24: nop(); break;
		case 0x25: nop(); break;
		case 0x26: nop(); break;
		case 0x27: nop(); break;
		case 0x28: nop(); break;
		case 0x29: nop(); break;
		case 0x2a: nop(); break;
		case 0x2b: nop(); break;
		case 0x2c: nop(); break;
		case 0x2d: nop(); break;
		case 0x2e: nop(); break;
		case 0x2f: nop(); break;
		case 0x30: nop(); break;
		case 0x31: nop(); break;
		case 0x32: nop(); break;
		case 0x33: nop(); break;
		case 0x34: nop(); break;
		case 0x35: nop(); break;
		case 0x36: nop(); break;
		case 0x37: nop(); break;
		case 0x38: nop(); break;
		case 0x39: nop(); break;
		case 0x3a: nop(); break;
		case 0x3b: nop(); break;
		case 0x3c: nop(); break;
		case 0x3d: nop(); break;
		case 0x3e: nop(); break;
		case 0x3f: nop(); break;

		case 0x40: in_b_c(); break;
		case 0x41: out_c_b(); break;
		case 0x42: sbc_hl_bc(); break;
		case 0x43: ld_xword_bc2(); break;
		case 0x44: neg(); break;
		case 0x45: retn(); break;
		case 0x46: im_0(); break;
		case 0x47: ld_i_a(); break;
		case 0x48: in_c_c(); break;
		case 0x49: out_c_c(); break;
		case 0x4a: adc_hl_bc(); break;
		case 0x4b: ld_bc_xword2(); break;
		case 0x4c: neg(); break;
		case 0x4d: reti(); break;
		case 0x4e: im_0(); break;
		case 0x4f: ld_r_a(); break;
		case 0x50: in_d_c(); break;
		case 0x51: out_c_d(); break;
		case 0x52: sbc_hl_de(); break;
		case 0x53: ld_xword_de2(); break;
		case 0x54: neg(); break;
		case 0x55: retn(); break;
		case 0x56: im_1(); break;
		case 0x57: ld_a_i(); break;
		case 0x58: in_e_c(); break;
		case 0x59: out_c_e(); break;
		case 0x5a: adc_hl_de(); break;
		case 0x5b: ld_de_xword2(); break;
		case 0x5c: neg(); break;
		case 0x5d: retn(); break;
		case 0x5e: im_2(); break;
		case 0x5f: ld_a_r(); break;
		case 0x60: in_h_c(); break;
		case 0x61: out_c_h(); break;
		case 0x62: sbc_hl_hl(); break;
		case 0x63: ld_xword_hl2(); break;
		case 0x64: neg(); break;
		case 0x65: retn(); break;
		case 0x66: im_0(); break;
		case 0x67: rrd(); break;
		case 0x68: in_l_c(); break;
		case 0x69: out_c_l(); break;
		case 0x6a: adc_hl_hl(); break;
		case 0x6b: ld_hl_xword2(); break;
		case 0x6c: neg(); break;
		case 0x6d: retn(); break;
		case 0x6e: im_0(); break;
		case 0x6f: rld(); break;
		case 0x70: in_0_c(); break;
		case 0x71: out_c_0(); break;
		case 0x72: sbc_hl_sp(); break;
		case 0x73: ld_xword_sp2(); break;
		case 0x74: neg(); break;
		case 0x75: retn(); break;
		case 0x76: im_1(); break;
		case 0x77: nop(); break;
		case 0x78: in_a_c(); break;
		case 0x79: out_c_a(); break;
		case 0x7a: adc_hl_sp(); break;
		case 0x7b: ld_sp_xword2(); break;
		case 0x7c: neg(); break;
		case 0x7d: retn(); break;
		case 0x7e: im_2(); break;
		case 0x7f: nop(); break;

		case 0x80: nop(); break;
		case 0x81: nop(); break;
		case 0x82: nop(); break;
		case 0x83: nop(); break;
		case 0x84: nop(); break;
		case 0x85: nop(); break;
		case 0x86: nop(); break;
		case 0x87: nop(); break;
		case 0x88: nop(); break;
		case 0x89: nop(); break;
		case 0x8a: nop(); break;
		case 0x8b: nop(); break;
		case 0x8c: nop(); break;
		case 0x8d: nop(); break;
		case 0x8e: nop(); break;
		case 0x8f: nop(); break;
		case 0x90: nop(); break;
		case 0x91: nop(); break;
		case 0x92: nop(); break;
		case 0x93: nop(); break;
		case 0x94: nop(); break;
		case 0x95: nop(); break;
		case 0x96: nop(); break;
		case 0x97: nop(); break;
		case 0x98: nop(); break;
		case 0x99: nop(); break;
		case 0x9a: nop(); break;
		case 0x9b: nop(); break;
		case 0x9c: nop(); break;
		case 0x9d: nop(); break;
		case 0x9e: nop(); break;
		case 0x9f: nop(); break;
		case 0xa0: ldi(); break;
		case 0xa1: cpi(); break;
		case 0xa2: ini(); break;
		case 0xa3: outi(); break;
		case 0xa4: nop(); break;
		case 0xa5: nop(); break;
		case 0xa6: nop(); break;
		case 0xa7: nop(); break;
		case 0xa8: ldd(); break;
		case 0xa9: cpd(); break;
		case 0xaa: ind(); break;
		case 0xab: outd(); break;
		case 0xac: nop(); break;
		case 0xad: nop(); break;
		case 0xae: nop(); break;
		case 0xaf: nop(); break;
		case 0xb0: ldir(); break;
		case 0xb1: cpir(); break;
		case 0xb2: inir(); break;
		case 0xb3: otir(); break;
		case 0xb4: nop(); break;
		case 0xb5: nop(); break;
		case 0xb6: nop(); break;
		case 0xb7: nop(); break;
		case 0xb8: lddr(); break;
		case 0xb9: cpdr(); break;
		case 0xba: indr(); break;
		case 0xbb: otdr(); break;
		case 0xbc: nop(); break;
		case 0xbd: nop(); break;
		case 0xbe: nop(); break;
		case 0xbf: nop(); break;

		case 0xc0: nop(); break;
		case 0xc1: if (T::hasMul()) mulub_a_b(); else nop();
		           break;
		case 0xc2: nop(); break;
		case 0xc3: if (T::hasMul()) muluw_hl_bc(); else nop();
		           break;
		case 0xc4: nop(); break;
		case 0xc5: nop(); break;
		case 0xc6: nop(); break;
		case 0xc7: nop(); break;
		case 0xc8: nop(); break;
		case 0xc9: if (T::hasMul()) mulub_a_c(); else nop();
		           break;
		case 0xca: nop(); break;
		case 0xcb: nop(); break;
		case 0xcc: nop(); break;
		case 0xcd: nop(); break;
		case 0xce: nop(); break;
		case 0xcf: nop(); break;
		case 0xd0: nop(); break;
		case 0xd1: if (T::hasMul()) mulub_a_d(); else nop();
		           break;
		case 0xd2: nop(); break;
		case 0xd3: nop(); break;
		case 0xd4: nop(); break;
		case 0xd5: nop(); break;
		case 0xd6: nop(); break;
		case 0xd7: nop(); break;
		case 0xd8: nop(); break;
		case 0xd9: if (T::hasMul()) mulub_a_e(); else nop();
		           break;
		case 0xda: nop(); break;
		case 0xdb: nop(); break;
		case 0xdc: nop(); break;
		case 0xdd: nop(); break;
		case 0xde: nop(); break;
		case 0xdf: nop(); break;
		case 0xe0: nop(); break;
		case 0xe1: nop(); break;
		case 0xe2: nop(); break;
		case 0xe3: nop(); break;
		case 0xe4: nop(); break;
		case 0xe5: nop(); break;
		case 0xe6: nop(); break;
		case 0xe7: nop(); break;
		case 0xe8: nop(); break;
		case 0xe9: nop(); break;
		case 0xea: nop(); break;
		case 0xeb: nop(); break;
		case 0xec: nop(); break;
		case 0xed: nop(); break;
		case 0xee: nop(); break;
		case 0xef: nop(); break;
		case 0xf0: nop(); break;
		case 0xf1: nop(); break;
		case 0xf2: nop(); break;
		case 0xf3: if (T::hasMul()) muluw_hl_sp(); else nop();
		           break;
		case 0xf4: nop(); break;
		case 0xf5: nop(); break;
		case 0xf6: nop(); break;
		case 0xf7: nop(); break;
		case 0xf8: nop(); break;
		case 0xf9: nop(); break;
		case 0xfa: nop(); break;
		case 0xfb: nop(); break;
		case 0xfc: nop(); break;
		case 0xfd: nop(); break;
		case 0xfe: nop(); break;
		case 0xff: nop(); break;
	}
}

// TODO dd2
template <class T> void CPUCore<T>::dd()
{
	T::add(T::CC_DD);
	byte opcode = RDMEM_OPCODE(T::CC_MAIN);
	M1Cycle();
	switch (opcode) {
		case 0x00: nop(); break;
		case 0x01: ld_bc_word(); break;
		case 0x02: ld_xbc_a(); break;
		case 0x03: inc_bc(); break;
		case 0x04: inc_b(); break;
		case 0x05: dec_b(); break;
		case 0x06: ld_b_byte(); break;
		case 0x07: rlca(); break;
		case 0x08: ex_af_af(); break;
		case 0x09: add_ix_bc(); break;
		case 0x0a: ld_a_xbc(); break;
		case 0x0b: dec_bc(); break;
		case 0x0c: inc_c(); break;
		case 0x0d: dec_c(); break;
		case 0x0e: ld_c_byte(); break;
		case 0x0f: rrca(); break;
		case 0x10: djnz(); break;
		case 0x11: ld_de_word(); break;
		case 0x12: ld_xde_a(); break;
		case 0x13: inc_de(); break;
		case 0x14: inc_d(); break;
		case 0x15: dec_d(); break;
		case 0x16: ld_d_byte(); break;
		case 0x17: rla(); break;
		case 0x18: jr(); break;
		case 0x19: add_ix_de(); break;
		case 0x1a: ld_a_xde(); break;
		case 0x1b: dec_de(); break;
		case 0x1c: inc_e(); break;
		case 0x1d: dec_e(); break;
		case 0x1e: ld_e_byte(); break;
		case 0x1f: rra(); break;
		case 0x20: jr_nz(); break;
		case 0x21: ld_ix_word(); break;
		case 0x22: ld_xword_ix(); break;
		case 0x23: inc_ix(); break;
		case 0x24: inc_ixh(); break;
		case 0x25: dec_ixh(); break;
		case 0x26: ld_ixh_byte(); break;
		case 0x27: daa(); break;
		case 0x28: jr_z(); break;
		case 0x29: add_ix_ix(); break;
		case 0x2a: ld_ix_xword(); break;
		case 0x2b: dec_ix(); break;
		case 0x2c: inc_ixl(); break;
		case 0x2d: dec_ixl(); break;
		case 0x2e: ld_ixl_byte(); break;
		case 0x2f: cpl(); break;
		case 0x30: jr_nc(); break;
		case 0x31: ld_sp_word(); break;
		case 0x32: ld_xbyte_a(); break;
		case 0x33: inc_sp(); break;
		case 0x34: inc_xix(); break;
		case 0x35: dec_xix(); break;
		case 0x36: ld_xix_byte(); break;
		case 0x37: scf(); break;
		case 0x38: jr_c(); break;
		case 0x39: add_ix_sp(); break;
		case 0x3a: ld_a_xbyte(); break;
		case 0x3b: dec_sp(); break;
		case 0x3c: inc_a(); break;
		case 0x3d: dec_a(); break;
		case 0x3e: ld_a_byte(); break;
		case 0x3f: ccf(); break;

		case 0x40: nop(); break;
		case 0x41: ld_b_c(); break;
		case 0x42: ld_b_d(); break;
		case 0x43: ld_b_e(); break;
		case 0x44: ld_b_ixh(); break;
		case 0x45: ld_b_ixl(); break;
		case 0x46: ld_b_xix(); break;
		case 0x47: ld_b_a(); break;
		case 0x48: ld_c_b(); break;
		case 0x49: nop(); break;
		case 0x4a: ld_c_d(); break;
		case 0x4b: ld_c_e(); break;
		case 0x4c: ld_c_ixh(); break;
		case 0x4d: ld_c_ixl(); break;
		case 0x4e: ld_c_xix(); break;
		case 0x4f: ld_c_a(); break;
		case 0x50: ld_d_b(); break;
		case 0x51: ld_d_c(); break;
		case 0x52: nop(); break;
		case 0x53: ld_d_e(); break;
		case 0x54: ld_d_ixh(); break;
		case 0x55: ld_d_ixl(); break;
		case 0x56: ld_d_xix(); break;
		case 0x57: ld_d_a(); break;
		case 0x58: ld_e_b(); break;
		case 0x59: ld_e_c(); break;
		case 0x5a: ld_e_d(); break;
		case 0x5b: nop(); break;
		case 0x5c: ld_e_ixh(); break;
		case 0x5d: ld_e_ixl(); break;
		case 0x5e: ld_e_xix(); break;
		case 0x5f: ld_e_a(); break;
		case 0x60: ld_ixh_b(); break;
		case 0x61: ld_ixh_c(); break;
		case 0x62: ld_ixh_d(); break;
		case 0x63: ld_ixh_e(); break;
		case 0x64: nop(); break;
		case 0x65: ld_ixh_ixl(); break;
		case 0x66: ld_h_xix(); break;
		case 0x67: ld_ixh_a(); break;
		case 0x68: ld_ixl_b(); break;
		case 0x69: ld_ixl_c(); break;
		case 0x6a: ld_ixl_d(); break;
		case 0x6b: ld_ixl_e(); break;
		case 0x6c: ld_ixl_ixh(); break;
		case 0x6d: nop(); break;
		case 0x6e: ld_l_xix(); break;
		case 0x6f: ld_ixl_a(); break;
		case 0x70: ld_xix_b(); break;
		case 0x71: ld_xix_c(); break;
		case 0x72: ld_xix_d(); break;
		case 0x73: ld_xix_e(); break;
		case 0x74: ld_xix_h(); break;
		case 0x75: ld_xix_l(); break;
		case 0x76: halt(); break;
		case 0x77: ld_xix_a(); break;
		case 0x78: ld_a_b(); break;
		case 0x79: ld_a_c(); break;
		case 0x7a: ld_a_d(); break;
		case 0x7b: ld_a_e(); break;
		case 0x7c: ld_a_ixh(); break;
		case 0x7d: ld_a_ixl(); break;
		case 0x7e: ld_a_xix(); break;
		case 0x7f: nop(); break;

		case 0x80: add_a_b(); break;
		case 0x81: add_a_c(); break;
		case 0x82: add_a_d(); break;
		case 0x83: add_a_e(); break;
		case 0x84: add_a_ixh(); break;
		case 0x85: add_a_ixl(); break;
		case 0x86: add_a_xix(); break;
		case 0x87: add_a_a(); break;
		case 0x88: adc_a_b(); break;
		case 0x89: adc_a_c(); break;
		case 0x8a: adc_a_d(); break;
		case 0x8b: adc_a_e(); break;
		case 0x8c: adc_a_ixh(); break;
		case 0x8d: adc_a_ixl(); break;
		case 0x8e: adc_a_xix(); break;
		case 0x8f: adc_a_a(); break;
		case 0x90: sub_b(); break;
		case 0x91: sub_c(); break;
		case 0x92: sub_d(); break;
		case 0x93: sub_e(); break;
		case 0x94: sub_ixh(); break;
		case 0x95: sub_ixl(); break;
		case 0x96: sub_xix(); break;
		case 0x97: sub_a(); break;
		case 0x98: sbc_a_b(); break;
		case 0x99: sbc_a_c(); break;
		case 0x9a: sbc_a_d(); break;
		case 0x9b: sbc_a_e(); break;
		case 0x9c: sbc_a_ixh(); break;
		case 0x9d: sbc_a_ixl(); break;
		case 0x9e: sbc_a_xix(); break;
		case 0x9f: sbc_a_a(); break;
		case 0xa0: and_b(); break;
		case 0xa1: and_c(); break;
		case 0xa2: and_d(); break;
		case 0xa3: and_e(); break;
		case 0xa4: and_ixh(); break;
		case 0xa5: and_ixl(); break;
		case 0xa6: and_xix(); break;
		case 0xa7: and_a(); break;
		case 0xa8: xor_b(); break;
		case 0xa9: xor_c(); break;
		case 0xaa: xor_d(); break;
		case 0xab: xor_e(); break;
		case 0xac: xor_ixh(); break;
		case 0xad: xor_ixl(); break;
		case 0xae: xor_xix(); break;
		case 0xaf: xor_a(); break;
		case 0xb0: or_b(); break;
		case 0xb1: or_c(); break;
		case 0xb2: or_d(); break;
		case 0xb3: or_e(); break;
		case 0xb4: or_ixh(); break;
		case 0xb5: or_ixl(); break;
		case 0xb6: or_xix(); break;
		case 0xb7: or_a(); break;
		case 0xb8: cp_b(); break;
		case 0xb9: cp_c(); break;
		case 0xba: cp_d(); break;
		case 0xbb: cp_e(); break;
		case 0xbc: cp_ixh(); break;
		case 0xbd: cp_ixl(); break;
		case 0xbe: cp_xix(); break;
		case 0xbf: cp_a(); break;

		case 0xc0: ret_nz(); break;
		case 0xc1: pop_bc(); break;
		case 0xc2: jp_nz(); break;
		case 0xc3: jp(); break;
		case 0xc4: call_nz(); break;
		case 0xc5: push_bc(); break;
		case 0xc6: add_a_byte(); break;
		case 0xc7: rst_00(); break;
		case 0xc8: ret_z(); break;
		case 0xc9: ret(); break;
		case 0xca: jp_z(); break;
		case 0xcb: dd_cb(); break;
		case 0xcc: call_z(); break;
		case 0xcd: call(); break;
		case 0xce: adc_a_byte(); break;
		case 0xcf: rst_08(); break;
		case 0xd0: ret_nc(); break;
		case 0xd1: pop_de(); break;
		case 0xd2: jp_nc(); break;
		case 0xd3: out_byte_a(); break;
		case 0xd4: call_nc(); break;
		case 0xd5: push_de(); break;
		case 0xd6: sub_byte(); break;
		case 0xd7: rst_10(); break;
		case 0xd8: ret_c(); break;
		case 0xd9: exx(); break;
		case 0xda: jp_c(); break;
		case 0xdb: in_a_byte(); break;
		case 0xdc: call_c(); break;
		case 0xdd: dd(); break;
		case 0xde: sbc_a_byte(); break;
		case 0xdf: rst_18(); break;
		case 0xe0: ret_po(); break;
		case 0xe1: pop_ix(); break;
		case 0xe2: jp_po(); break;
		case 0xe3: ex_xsp_ix(); break;
		case 0xe4: call_po(); break;
		case 0xe5: push_ix(); break;
		case 0xe6: and_byte(); break;
		case 0xe7: rst_20(); break;
		case 0xe8: ret_pe(); break;
		case 0xe9: jp_ix(); break;
		case 0xea: jp_pe(); break;
		case 0xeb: ex_de_hl(); break;
		case 0xec: call_pe(); break;
		case 0xed: ed(); break;
		case 0xee: xor_byte(); break;
		case 0xef: rst_28(); break;
		case 0xf0: ret_p(); break;
		case 0xf1: pop_af(); break;
		case 0xf2: jp_p(); break;
		case 0xf3: di(); break;
		case 0xf4: call_p(); break;
		case 0xf5: push_af(); break;
		case 0xf6: or_byte(); break;
		case 0xf7: rst_30(); break;
		case 0xf8: ret_m(); break;
		case 0xf9: ld_sp_ix(); break;
		case 0xfa: jp_m(); break;
		case 0xfb: ei(); break;
		case 0xfc: call_m(); break;
		case 0xfd: fd(); break;
		case 0xfe: cp_byte(); break;
		case 0xff: rst_38(); break;
	}
}

// TODO fd2
template <class T> void CPUCore<T>::fd()
{
	T::add(T::CC_DD);
	byte opcode = RDMEM_OPCODE(T::CC_MAIN);
	M1Cycle();
	switch (opcode) {
		case 0x00: nop(); break;
		case 0x01: ld_bc_word(); break;
		case 0x02: ld_xbc_a(); break;
		case 0x03: inc_bc(); break;
		case 0x04: inc_b(); break;
		case 0x05: dec_b(); break;
		case 0x06: ld_b_byte(); break;
		case 0x07: rlca(); break;
		case 0x08: ex_af_af(); break;
		case 0x09: add_iy_bc(); break;
		case 0x0a: ld_a_xbc(); break;
		case 0x0b: dec_bc(); break;
		case 0x0c: inc_c(); break;
		case 0x0d: dec_c(); break;
		case 0x0e: ld_c_byte(); break;
		case 0x0f: rrca(); break;
		case 0x10: djnz(); break;
		case 0x11: ld_de_word(); break;
		case 0x12: ld_xde_a(); break;
		case 0x13: inc_de(); break;
		case 0x14: inc_d(); break;
		case 0x15: dec_d(); break;
		case 0x16: ld_d_byte(); break;
		case 0x17: rla(); break;
		case 0x18: jr(); break;
		case 0x19: add_iy_de(); break;
		case 0x1a: ld_a_xde(); break;
		case 0x1b: dec_de(); break;
		case 0x1c: inc_e(); break;
		case 0x1d: dec_e(); break;
		case 0x1e: ld_e_byte(); break;
		case 0x1f: rra(); break;
		case 0x20: jr_nz(); break;
		case 0x21: ld_iy_word(); break;
		case 0x22: ld_xword_iy(); break;
		case 0x23: inc_iy(); break;
		case 0x24: inc_iyh(); break;
		case 0x25: dec_iyh(); break;
		case 0x26: ld_iyh_byte(); break;
		case 0x27: daa(); break;
		case 0x28: jr_z(); break;
		case 0x29: add_iy_iy(); break;
		case 0x2a: ld_iy_xword(); break;
		case 0x2b: dec_iy(); break;
		case 0x2c: inc_iyl(); break;
		case 0x2d: dec_iyl(); break;
		case 0x2e: ld_iyl_byte(); break;
		case 0x2f: cpl(); break;
		case 0x30: jr_nc(); break;
		case 0x31: ld_sp_word(); break;
		case 0x32: ld_xbyte_a(); break;
		case 0x33: inc_sp(); break;
		case 0x34: inc_xiy(); break;
		case 0x35: dec_xiy(); break;
		case 0x36: ld_xiy_byte(); break;
		case 0x37: scf(); break;
		case 0x38: jr_c(); break;
		case 0x39: add_iy_sp(); break;
		case 0x3a: ld_a_xbyte(); break;
		case 0x3b: dec_sp(); break;
		case 0x3c: inc_a(); break;
		case 0x3d: dec_a(); break;
		case 0x3e: ld_a_byte(); break;
		case 0x3f: ccf(); break;

		case 0x40: nop(); break;
		case 0x41: ld_b_c(); break;
		case 0x42: ld_b_d(); break;
		case 0x43: ld_b_e(); break;
		case 0x44: ld_b_iyh(); break;
		case 0x45: ld_b_iyl(); break;
		case 0x46: ld_b_xiy(); break;
		case 0x47: ld_b_a(); break;
		case 0x48: ld_c_b(); break;
		case 0x49: nop(); break;
		case 0x4a: ld_c_d(); break;
		case 0x4b: ld_c_e(); break;
		case 0x4c: ld_c_iyh(); break;
		case 0x4d: ld_c_iyl(); break;
		case 0x4e: ld_c_xiy(); break;
		case 0x4f: ld_c_a(); break;
		case 0x50: ld_d_b(); break;
		case 0x51: ld_d_c(); break;
		case 0x52: nop(); break;
		case 0x53: ld_d_e(); break;
		case 0x54: ld_d_iyh(); break;
		case 0x55: ld_d_iyl(); break;
		case 0x56: ld_d_xiy(); break;
		case 0x57: ld_d_a(); break;
		case 0x58: ld_e_b(); break;
		case 0x59: ld_e_c(); break;
		case 0x5a: ld_e_d(); break;
		case 0x5b: nop(); break;
		case 0x5c: ld_e_iyh(); break;
		case 0x5d: ld_e_iyl(); break;
		case 0x5e: ld_e_xiy(); break;
		case 0x5f: ld_e_a(); break;
		case 0x60: ld_iyh_b(); break;
		case 0x61: ld_iyh_c(); break;
		case 0x62: ld_iyh_d(); break;
		case 0x63: ld_iyh_e(); break;
		case 0x64: nop(); break;
		case 0x65: ld_iyh_iyl(); break;
		case 0x66: ld_h_xiy(); break;
		case 0x67: ld_iyh_a(); break;
		case 0x68: ld_iyl_b(); break;
		case 0x69: ld_iyl_c(); break;
		case 0x6a: ld_iyl_d(); break;
		case 0x6b: ld_iyl_e(); break;
		case 0x6c: ld_iyl_iyh(); break;
		case 0x6d: nop(); break;
		case 0x6e: ld_l_xiy(); break;
		case 0x6f: ld_iyl_a(); break;
		case 0x70: ld_xiy_b(); break;
		case 0x71: ld_xiy_c(); break;
		case 0x72: ld_xiy_d(); break;
		case 0x73: ld_xiy_e(); break;
		case 0x74: ld_xiy_h(); break;
		case 0x75: ld_xiy_l(); break;
		case 0x76: halt(); break;
		case 0x77: ld_xiy_a(); break;
		case 0x78: ld_a_b(); break;
		case 0x79: ld_a_c(); break;
		case 0x7a: ld_a_d(); break;
		case 0x7b: ld_a_e(); break;
		case 0x7c: ld_a_iyh(); break;
		case 0x7d: ld_a_iyl(); break;
		case 0x7e: ld_a_xiy(); break;
		case 0x7f: nop(); break;

		case 0x80: add_a_b(); break;
		case 0x81: add_a_c(); break;
		case 0x82: add_a_d(); break;
		case 0x83: add_a_e(); break;
		case 0x84: add_a_iyh(); break;
		case 0x85: add_a_iyl(); break;
		case 0x86: add_a_xiy(); break;
		case 0x87: add_a_a(); break;
		case 0x88: adc_a_b(); break;
		case 0x89: adc_a_c(); break;
		case 0x8a: adc_a_d(); break;
		case 0x8b: adc_a_e(); break;
		case 0x8c: adc_a_iyh(); break;
		case 0x8d: adc_a_iyl(); break;
		case 0x8e: adc_a_xiy(); break;
		case 0x8f: adc_a_a(); break;
		case 0x90: sub_b(); break;
		case 0x91: sub_c(); break;
		case 0x92: sub_d(); break;
		case 0x93: sub_e(); break;
		case 0x94: sub_iyh(); break;
		case 0x95: sub_iyl(); break;
		case 0x96: sub_xiy(); break;
		case 0x97: sub_a(); break;
		case 0x98: sbc_a_b(); break;
		case 0x99: sbc_a_c(); break;
		case 0x9a: sbc_a_d(); break;
		case 0x9b: sbc_a_e(); break;
		case 0x9c: sbc_a_iyh(); break;
		case 0x9d: sbc_a_iyl(); break;
		case 0x9e: sbc_a_xiy(); break;
		case 0x9f: sbc_a_a(); break;
		case 0xa0: and_b(); break;
		case 0xa1: and_c(); break;
		case 0xa2: and_d(); break;
		case 0xa3: and_e(); break;
		case 0xa4: and_iyh(); break;
		case 0xa5: and_iyl(); break;
		case 0xa6: and_xiy(); break;
		case 0xa7: and_a(); break;
		case 0xa8: xor_b(); break;
		case 0xa9: xor_c(); break;
		case 0xaa: xor_d(); break;
		case 0xab: xor_e(); break;
		case 0xac: xor_iyh(); break;
		case 0xad: xor_iyl(); break;
		case 0xae: xor_xiy(); break;
		case 0xaf: xor_a(); break;
		case 0xb0: or_b(); break;
		case 0xb1: or_c(); break;
		case 0xb2: or_d(); break;
		case 0xb3: or_e(); break;
		case 0xb4: or_iyh(); break;
		case 0xb5: or_iyl(); break;
		case 0xb6: or_xiy(); break;
		case 0xb7: or_a(); break;
		case 0xb8: cp_b(); break;
		case 0xb9: cp_c(); break;
		case 0xba: cp_d(); break;
		case 0xbb: cp_e(); break;
		case 0xbc: cp_iyh(); break;
		case 0xbd: cp_iyl(); break;
		case 0xbe: cp_xiy(); break;
		case 0xbf: cp_a(); break;

		case 0xc0: ret_nz(); break;
		case 0xc1: pop_bc(); break;
		case 0xc2: jp_nz(); break;
		case 0xc3: jp(); break;
		case 0xc4: call_nz(); break;
		case 0xc5: push_bc(); break;
		case 0xc6: add_a_byte(); break;
		case 0xc7: rst_00(); break;
		case 0xc8: ret_z(); break;
		case 0xc9: ret(); break;
		case 0xca: jp_z(); break;
		case 0xcb: fd_cb(); break;
		case 0xcc: call_z(); break;
		case 0xcd: call(); break;
		case 0xce: adc_a_byte(); break;
		case 0xcf: rst_08(); break;
		case 0xd0: ret_nc(); break;
		case 0xd1: pop_de(); break;
		case 0xd2: jp_nc(); break;
		case 0xd3: out_byte_a(); break;
		case 0xd4: call_nc(); break;
		case 0xd5: push_de(); break;
		case 0xd6: sub_byte(); break;
		case 0xd7: rst_10(); break;
		case 0xd8: ret_c(); break;
		case 0xd9: exx(); break;
		case 0xda: jp_c(); break;
		case 0xdb: in_a_byte(); break;
		case 0xdc: call_c(); break;
		case 0xdd: dd(); break;
		case 0xde: sbc_a_byte(); break;
		case 0xdf: rst_18(); break;
		case 0xe0: ret_po(); break;
		case 0xe1: pop_iy(); break;
		case 0xe2: jp_po(); break;
		case 0xe3: ex_xsp_iy(); break;
		case 0xe4: call_po(); break;
		case 0xe5: push_iy(); break;
		case 0xe6: and_byte(); break;
		case 0xe7: rst_20(); break;
		case 0xe8: ret_pe(); break;
		case 0xe9: jp_iy(); break;
		case 0xea: jp_pe(); break;
		case 0xeb: ex_de_hl(); break;
		case 0xec: call_pe(); break;
		case 0xed: ed(); break;
		case 0xee: xor_byte(); break;
		case 0xef: rst_28(); break;
		case 0xf0: ret_p(); break;
		case 0xf1: pop_af(); break;
		case 0xf2: jp_p(); break;
		case 0xf3: di(); break;
		case 0xf4: call_p(); break;
		case 0xf5: push_af(); break;
		case 0xf6: or_byte(); break;
		case 0xf7: rst_30(); break;
		case 0xf8: ret_m(); break;
		case 0xf9: ld_sp_iy(); break;
		case 0xfa: jp_m(); break;
		case 0xfb: ei(); break;
		case 0xfc: call_m(); break;
		case 0xfd: fd(); break;
		case 0xfe: cp_byte(); break;
		case 0xff: rst_38(); break;
	}
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

} // namespace openmsx

