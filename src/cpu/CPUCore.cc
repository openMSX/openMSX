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
	T::PRE_IO(port);
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	byte result = interface->readIO(port, time);
	T::POST_IO(port);
	return result;
}

template <class T> inline void CPUCore<T>::WRITE_PORT(unsigned port, byte value, unsigned cc)
{
	memptr = port + 1; // not 16-bit
	T::PRE_IO(port);
	EmuTime time = T::getTimeFast(cc);
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
	EmuTime time = T::getTimeFast(cc);
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
	res         += RDMEM_OPCODE(cc + T::CC_RDMEM) << 8;
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
	EmuTime time = T::getTimeFast(cc);
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
	res         += RDMEM((address + 1) & 0xFFFF, cc + T::CC_RDMEM) << 8;
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
	EmuTime time = T::getTimeFast(cc);
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
	WRMEM((address + 1) & 0xFFFF, value >> 8,  cc + T::CC_WRMEM);
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
	WRMEM( address,               value & 255, cc + T::CC_WRMEM);
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
		case 0x01: return ld_bc_word();
		case 0x02: return ld_xbc_a();
		case 0x03: return inc_bc();
		case 0x04: return inc_b();
		case 0x05: return dec_b();
		case 0x06: return ld_b_byte();
		case 0x07: return rlca();
		case 0x08: return ex_af_af();
		case 0x09: return add_hl_bc();
		case 0x0a: return ld_a_xbc();
		case 0x0b: return dec_bc();
		case 0x0c: return inc_c();
		case 0x0d: return dec_c();
		case 0x0e: return ld_c_byte();
		case 0x0f: return rrca();
		case 0x10: return djnz();
		case 0x11: return ld_de_word();
		case 0x12: return ld_xde_a();
		case 0x13: return inc_de();
		case 0x14: return inc_d();
		case 0x15: return dec_d();
		case 0x16: return ld_d_byte();
		case 0x17: return rla();
		case 0x18: return jr();
		case 0x19: return add_hl_de();
		case 0x1a: return ld_a_xde();
		case 0x1b: return dec_de();
		case 0x1c: return inc_e();
		case 0x1d: return dec_e();
		case 0x1e: return ld_e_byte();
		case 0x1f: return rra();
		case 0x20: return jr_nz();
		case 0x21: return ld_hl_word();
		case 0x22: return ld_xword_hl();
		case 0x23: return inc_hl();
		case 0x24: return inc_h();
		case 0x25: return dec_h();
		case 0x26: return ld_h_byte();
		case 0x27: return daa();
		case 0x28: return jr_z();
		case 0x29: return add_hl_hl();
		case 0x2a: return ld_hl_xword();
		case 0x2b: return dec_hl();
		case 0x2c: return inc_l();
		case 0x2d: return dec_l();
		case 0x2e: return ld_l_byte();
		case 0x2f: return cpl();
		case 0x30: return jr_nc();
		case 0x31: return ld_sp_word();
		case 0x32: return ld_xbyte_a();
		case 0x33: return inc_sp();
		case 0x34: return inc_xhl();
		case 0x35: return dec_xhl();
		case 0x36: return ld_xhl_byte();
		case 0x37: return scf();
		case 0x38: return jr_c();
		case 0x39: return add_hl_sp();
		case 0x3a: return ld_a_xbyte();
		case 0x3b: return dec_sp();
		case 0x3c: return inc_a();
		case 0x3d: return dec_a();
		case 0x3e: return ld_a_byte();
		case 0x3f: return ccf();

		case 0x41: return ld_b_c();
		case 0x42: return ld_b_d();
		case 0x43: return ld_b_e();
		case 0x44: return ld_b_h();
		case 0x45: return ld_b_l();
		case 0x46: return ld_b_xhl();
		case 0x47: return ld_b_a();
		case 0x48: return ld_c_b();
		case 0x4a: return ld_c_d();
		case 0x4b: return ld_c_e();
		case 0x4c: return ld_c_h();
		case 0x4d: return ld_c_l();
		case 0x4e: return ld_c_xhl();
		case 0x4f: return ld_c_a();
		case 0x50: return ld_d_b();
		case 0x51: return ld_d_c();
		case 0x53: return ld_d_e();
		case 0x54: return ld_d_h();
		case 0x55: return ld_d_l();
		case 0x56: return ld_d_xhl();
		case 0x57: return ld_d_a();
		case 0x58: return ld_e_b();
		case 0x59: return ld_e_c();
		case 0x5a: return ld_e_d();
		case 0x5c: return ld_e_h();
		case 0x5d: return ld_e_l();
		case 0x5e: return ld_e_xhl();
		case 0x5f: return ld_e_a();
		case 0x60: return ld_h_b();
		case 0x61: return ld_h_c();
		case 0x62: return ld_h_d();
		case 0x63: return ld_h_e();
		case 0x65: return ld_h_l();
		case 0x66: return ld_h_xhl();
		case 0x67: return ld_h_a();
		case 0x68: return ld_l_b();
		case 0x69: return ld_l_c();
		case 0x6a: return ld_l_d();
		case 0x6b: return ld_l_e();
		case 0x6c: return ld_l_h();
		case 0x6e: return ld_l_xhl();
		case 0x6f: return ld_l_a();
		case 0x70: return ld_xhl_b();
		case 0x71: return ld_xhl_c();
		case 0x72: return ld_xhl_d();
		case 0x73: return ld_xhl_e();
		case 0x74: return ld_xhl_h();
		case 0x75: return ld_xhl_l();
		case 0x76: return halt();
		case 0x77: return ld_xhl_a();
		case 0x78: return ld_a_b();
		case 0x79: return ld_a_c();
		case 0x7a: return ld_a_d();
		case 0x7b: return ld_a_e();
		case 0x7c: return ld_a_h();
		case 0x7d: return ld_a_l();
		case 0x7e: return ld_a_xhl();

		case 0x80: return add_a_b();
		case 0x81: return add_a_c();
		case 0x82: return add_a_d();
		case 0x83: return add_a_e();
		case 0x84: return add_a_h();
		case 0x85: return add_a_l();
		case 0x86: return add_a_xhl();
		case 0x87: return add_a_a();
		case 0x88: return adc_a_b();
		case 0x89: return adc_a_c();
		case 0x8a: return adc_a_d();
		case 0x8b: return adc_a_e();
		case 0x8c: return adc_a_h();
		case 0x8d: return adc_a_l();
		case 0x8e: return adc_a_xhl();
		case 0x8f: return adc_a_a();
		case 0x90: return sub_b();
		case 0x91: return sub_c();
		case 0x92: return sub_d();
		case 0x93: return sub_e();
		case 0x94: return sub_h();
		case 0x95: return sub_l();
		case 0x96: return sub_xhl();
		case 0x97: return sub_a();
		case 0x98: return sbc_a_b();
		case 0x99: return sbc_a_c();
		case 0x9a: return sbc_a_d();
		case 0x9b: return sbc_a_e();
		case 0x9c: return sbc_a_h();
		case 0x9d: return sbc_a_l();
		case 0x9e: return sbc_a_xhl();
		case 0x9f: return sbc_a_a();
		case 0xa0: return and_b();
		case 0xa1: return and_c();
		case 0xa2: return and_d();
		case 0xa3: return and_e();
		case 0xa4: return and_h();
		case 0xa5: return and_l();
		case 0xa6: return and_xhl();
		case 0xa7: return and_a();
		case 0xa8: return xor_b();
		case 0xa9: return xor_c();
		case 0xaa: return xor_d();
		case 0xab: return xor_e();
		case 0xac: return xor_h();
		case 0xad: return xor_l();
		case 0xae: return xor_xhl();
		case 0xaf: return xor_a();
		case 0xb0: return or_b();
		case 0xb1: return or_c();
		case 0xb2: return or_d();
		case 0xb3: return or_e();
		case 0xb4: return or_h();
		case 0xb5: return or_l();
		case 0xb6: return or_xhl();
		case 0xb7: return or_a();
		case 0xb8: return cp_b();
		case 0xb9: return cp_c();
		case 0xba: return cp_d();
		case 0xbb: return cp_e();
		case 0xbc: return cp_h();
		case 0xbd: return cp_l();
		case 0xbe: return cp_xhl();
		case 0xbf: return cp_a();

		case 0xc0: return ret_nz();
		case 0xc1: return pop_bc();
		case 0xc2: return jp_nz();
		case 0xc3: return jp();
		case 0xc4: return call_nz();
		case 0xc5: return push_bc();
		case 0xc6: return add_a_byte();
		case 0xc7: return rst_00();
		case 0xc8: return ret_z();
		case 0xc9: return ret();
		case 0xca: return jp_z();
		case 0xcb: return cb();
		case 0xcc: return call_z();
		case 0xcd: return call();
		case 0xce: return adc_a_byte();
		case 0xcf: return rst_08();
		case 0xd0: return ret_nc();
		case 0xd1: return pop_de();
		case 0xd2: return jp_nc();
		case 0xd3: return out_byte_a();
		case 0xd4: return call_nc();
		case 0xd5: return push_de();
		case 0xd6: return sub_byte();
		case 0xd7: return rst_10();
		case 0xd8: return ret_c();
		case 0xd9: return exx();
		case 0xda: return jp_c();
		case 0xdb: return in_a_byte();
		case 0xdc: return call_c();
		case 0xdd: return dd();
		case 0xde: return sbc_a_byte();
		case 0xdf: return rst_18();
		case 0xe0: return ret_po();
		case 0xe1: return pop_hl();
		case 0xe2: return jp_po();
		case 0xe3: return ex_xsp_hl();
		case 0xe4: return call_po();
		case 0xe5: return push_hl();
		case 0xe6: return and_byte();
		case 0xe7: return rst_20();
		case 0xe8: return ret_pe();
		case 0xe9: return jp_hl();
		case 0xea: return jp_pe();
		case 0xeb: return ex_de_hl();
		case 0xec: return call_pe();
		case 0xed: return ed();
		case 0xee: return xor_byte();
		case 0xef: return rst_28();
		case 0xf0: return ret_p();
		case 0xf1: return pop_af();
		case 0xf2: return jp_p();
		case 0xf3: return di();
		case 0xf4: return call_p();
		case 0xf5: return push_af();
		case 0xf6: return or_byte();
		case 0xf7: return rst_30();
		case 0xf8: return ret_m();
		case 0xf9: return ld_sp_hl();
		case 0xfa: return jp_m();
		case 0xfb: return ei();
		case 0xfc: return call_m();
		case 0xfd: return fd();
		case 0xfe: return cp_byte();
		case 0xff: return rst_38();
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
template <class T> inline bool CPUCore<T>::C()  { return R.getF() & C_FLAG; }
template <class T> inline bool CPUCore<T>::NC() { return !C(); }
template <class T> inline bool CPUCore<T>::Z()  { return R.getF() & Z_FLAG; }
template <class T> inline bool CPUCore<T>::NZ() { return !Z(); }
template <class T> inline bool CPUCore<T>::M()  { return R.getF() & S_FLAG; }
template <class T> inline bool CPUCore<T>::P()  { return !M(); }
template <class T> inline bool CPUCore<T>::PE() { return R.getF() & V_FLAG; }
template <class T> inline bool CPUCore<T>::PO() { return !PE(); }


// LD r,r
template <class T> int CPUCore<T>::ld_a_b()     { R.setA(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_c()     { R.setA(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_d()     { R.setA(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_e()     { R.setA(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_h()     { R.setA(R.getH()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_l()     { R.setA(R.getL()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_ixh()   { R.setA(R.getIXh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_ixl()   { R.setA(R.getIXl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_iyh()   { R.setA(R.getIYh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_a_iyl()   { R.setA(R.getIYl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_a()     { R.setB(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_c()     { R.setB(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_d()     { R.setB(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_e()     { R.setB(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_h()     { R.setB(R.getH()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_l()     { R.setB(R.getL()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_ixh()   { R.setB(R.getIXh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_ixl()   { R.setB(R.getIXl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_iyh()   { R.setB(R.getIYh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_b_iyl()   { R.setB(R.getIYl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_a()     { R.setC(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_b()     { R.setC(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_d()     { R.setC(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_e()     { R.setC(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_h()     { R.setC(R.getH()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_l()     { R.setC(R.getL()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_ixh()   { R.setC(R.getIXh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_ixl()   { R.setC(R.getIXl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_iyh()   { R.setC(R.getIYh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_c_iyl()   { R.setC(R.getIYl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_a()     { R.setD(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_c()     { R.setD(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_b()     { R.setD(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_e()     { R.setD(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_h()     { R.setD(R.getH()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_l()     { R.setD(R.getL()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_ixh()   { R.setD(R.getIXh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_ixl()   { R.setD(R.getIXl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_iyh()   { R.setD(R.getIYh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_d_iyl()   { R.setD(R.getIYl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_a()     { R.setE(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_c()     { R.setE(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_b()     { R.setE(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_d()     { R.setE(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_h()     { R.setE(R.getH()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_l()     { R.setE(R.getL()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_ixh()   { R.setE(R.getIXh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_ixl()   { R.setE(R.getIXl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_iyh()   { R.setE(R.getIYh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_e_iyl()   { R.setE(R.getIYl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_h_a()     { R.setH(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_h_c()     { R.setH(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_h_b()     { R.setH(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_h_e()     { R.setH(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_h_d()     { R.setH(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_h_l()     { R.setH(R.getL()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_l_a()     { R.setL(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_l_c()     { R.setL(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_l_b()     { R.setL(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_l_e()     { R.setL(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_l_d()     { R.setL(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_l_h()     { R.setL(R.getH()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixh_a()   { R.setIXh(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixh_b()   { R.setIXh(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixh_c()   { R.setIXh(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixh_d()   { R.setIXh(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixh_e()   { R.setIXh(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixh_ixl() { R.setIXh(R.getIXl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixl_a()   { R.setIXl(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixl_b()   { R.setIXl(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixl_c()   { R.setIXl(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixl_d()   { R.setIXl(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixl_e()   { R.setIXl(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_ixl_ixh() { R.setIXl(R.getIXh()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyh_a()   { R.setIYh(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyh_b()   { R.setIYh(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyh_c()   { R.setIYh(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyh_d()   { R.setIYh(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyh_e()   { R.setIYh(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyh_iyl() { R.setIYh(R.getIYl()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyl_a()   { R.setIYl(R.getA()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyl_b()   { R.setIYl(R.getB()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyl_c()   { R.setIYl(R.getC()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyl_d()   { R.setIYl(R.getD()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyl_e()   { R.setIYl(R.getE()); return T::CC_LD_R_R; }
template <class T> int CPUCore<T>::ld_iyl_iyh() { R.setIYl(R.getIYh()); return T::CC_LD_R_R; }

// LD SP,ss
template <class T> int CPUCore<T>::ld_sp_hl() {
	R.setSP(R.getHL()); return T::CC_LD_SP_HL;
}
template <class T> int CPUCore<T>::ld_sp_ix() {
	R.setSP(R.getIX()); return T::CC_LD_SP_HL;
}
template <class T> int CPUCore<T>::ld_sp_iy() {
	R.setSP(R.getIY()); return T::CC_LD_SP_HL;
}

// LD (ss),a
template <class T> inline int CPUCore<T>::WR_X_A(unsigned x)
{
	WRMEM(x, R.getA(), T::CC_LD_SS_A_1);
	return T::CC_LD_SS_A;
}
template <class T> int CPUCore<T>::ld_xbc_a() { return WR_X_A(R.getBC()); }
template <class T> int CPUCore<T>::ld_xde_a() { return WR_X_A(R.getDE()); }

// LD (HL),r
template <class T> inline int CPUCore<T>::WR_HL_X(byte val)
{
	WRMEM(R.getHL(), val, T::CC_LD_HL_R_1);
	return T::CC_LD_HL_R;
}
template <class T> int CPUCore<T>::ld_xhl_a() { return WR_HL_X(R.getA()); }
template <class T> int CPUCore<T>::ld_xhl_b() { return WR_HL_X(R.getB()); }
template <class T> int CPUCore<T>::ld_xhl_c() { return WR_HL_X(R.getC()); }
template <class T> int CPUCore<T>::ld_xhl_d() { return WR_HL_X(R.getD()); }
template <class T> int CPUCore<T>::ld_xhl_e() { return WR_HL_X(R.getE()); }
template <class T> int CPUCore<T>::ld_xhl_h() { return WR_HL_X(R.getH()); }
template <class T> int CPUCore<T>::ld_xhl_l() { return WR_HL_X(R.getL()); }

// LD (HL),n
template <class T> int CPUCore<T>::ld_xhl_byte()
{
	byte val = RDMEM_OPCODE(T::CC_LD_HL_N_1);
	WRMEM(R.getHL(), val, T::CC_LD_HL_N_2);
	return T::CC_LD_HL_N;
}

// LD (IX+e),r
template <class T> inline int CPUCore<T>::WR_XIX(byte val)
{
	offset ofst = RDMEM_OPCODE(T::CC_LD_XIX_R_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_R_2);
	return T::CC_LD_XIX_R;
}
template <class T> int CPUCore<T>::ld_xix_a() { return WR_XIX(R.getA()); }
template <class T> int CPUCore<T>::ld_xix_b() { return WR_XIX(R.getB()); }
template <class T> int CPUCore<T>::ld_xix_c() { return WR_XIX(R.getC()); }
template <class T> int CPUCore<T>::ld_xix_d() { return WR_XIX(R.getD()); }
template <class T> int CPUCore<T>::ld_xix_e() { return WR_XIX(R.getE()); }
template <class T> int CPUCore<T>::ld_xix_h() { return WR_XIX(R.getH()); }
template <class T> int CPUCore<T>::ld_xix_l() { return WR_XIX(R.getL()); }

// LD (IY+e),r
template <class T> inline int CPUCore<T>::WR_XIY(byte val)
{
	offset ofst = RDMEM_OPCODE(T::CC_LD_XIX_R_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_R_2);
	return T::CC_LD_XIX_R;
}
template <class T> int CPUCore<T>::ld_xiy_a() { return WR_XIY(R.getA()); }
template <class T> int CPUCore<T>::ld_xiy_b() { return WR_XIY(R.getB()); }
template <class T> int CPUCore<T>::ld_xiy_c() { return WR_XIY(R.getC()); }
template <class T> int CPUCore<T>::ld_xiy_d() { return WR_XIY(R.getD()); }
template <class T> int CPUCore<T>::ld_xiy_e() { return WR_XIY(R.getE()); }
template <class T> int CPUCore<T>::ld_xiy_h() { return WR_XIY(R.getH()); }
template <class T> int CPUCore<T>::ld_xiy_l() { return WR_XIY(R.getL()); }

// LD (IX+e),n
template <class T> int CPUCore<T>::ld_xix_byte()
{
	unsigned tmp = RD_WORD_PC(T::CC_LD_XIX_N_1);
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	memptr = (R.getIX() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_N_2);
	return T::CC_LD_XIX_N;
}

// LD (IY+e),n
template <class T> int CPUCore<T>::ld_xiy_byte()
{
	unsigned tmp = RD_WORD_PC(T::CC_LD_XIX_N_1);
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	memptr = (R.getIY() + ofst) & 0xFFFF;
	WRMEM(memptr, val, T::CC_LD_XIX_N_2);
	return T::CC_LD_XIX_N;
}

// LD (nn),A
template <class T> int CPUCore<T>::ld_xbyte_a()
{
	unsigned x = RD_WORD_PC(T::CC_LD_NN_A_1);
	memptr = R.getA() << 8;
	WRMEM(x, R.getA(), T::CC_LD_NN_A_2);
	return T::CC_LD_NN_A;
}

// LD (nn),ss
template <class T> inline int CPUCore<T>::WR_NN_Y(unsigned reg, int ee)
{
	memptr = RD_WORD_PC(T::CC_LD_XX_HL_1 + ee);
	WR_WORD(memptr, reg, T::CC_LD_XX_HL_2 + ee);
	return T::CC_LD_XX_HL + ee;
}
template <class T> int CPUCore<T>::ld_xword_hl()  { return WR_NN_Y(R.getHL(), 0); }
template <class T> int CPUCore<T>::ld_xword_ix()  { return WR_NN_Y(R.getIX(), 0); }
template <class T> int CPUCore<T>::ld_xword_iy()  { return WR_NN_Y(R.getIY(), 0); }
template <class T> int CPUCore<T>::ld_xword_bc2() { return WR_NN_Y(R.getBC(), T::EE_ED); }
template <class T> int CPUCore<T>::ld_xword_de2() { return WR_NN_Y(R.getDE(), T::EE_ED); }
template <class T> int CPUCore<T>::ld_xword_hl2() { return WR_NN_Y(R.getHL(), T::EE_ED); }
template <class T> int CPUCore<T>::ld_xword_sp2() { return WR_NN_Y(R.getSP(), T::EE_ED); }

// LD A,(ss)
template <class T> int CPUCore<T>::ld_a_xbc() { R.setA(RDMEM(R.getBC(), T::CC_LD_A_SS_1)); return T::CC_LD_A_SS; }
template <class T> int CPUCore<T>::ld_a_xde() { R.setA(RDMEM(R.getDE(), T::CC_LD_A_SS_1)); return T::CC_LD_A_SS; }

// LD A,(nn)
template <class T> int CPUCore<T>::ld_a_xbyte()
{
	unsigned addr = RD_WORD_PC(T::CC_LD_A_NN_1);
	memptr = addr + 1;
	R.setA(RDMEM(addr, T::CC_LD_A_NN_2));
	return T::CC_LD_A_NN;
}

// LD r,n
template <class T> int CPUCore<T>::ld_a_byte()   { R.setA  (RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_b_byte()   { R.setB  (RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_c_byte()   { R.setC  (RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_d_byte()   { R.setD  (RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_e_byte()   { R.setE  (RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_h_byte()   { R.setH  (RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_l_byte()   { R.setL  (RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_ixh_byte() { R.setIXh(RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_ixl_byte() { R.setIXl(RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_iyh_byte() { R.setIYh(RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }
template <class T> int CPUCore<T>::ld_iyl_byte() { R.setIYl(RDMEM_OPCODE(T::CC_LD_R_N_1)); return T::CC_LD_R_N; }

// LD r,(hl)
template <class T> int CPUCore<T>::ld_a_xhl() { R.setA(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL; }
template <class T> int CPUCore<T>::ld_b_xhl() { R.setB(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL; }
template <class T> int CPUCore<T>::ld_c_xhl() { R.setC(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL; }
template <class T> int CPUCore<T>::ld_d_xhl() { R.setD(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL; }
template <class T> int CPUCore<T>::ld_e_xhl() { R.setE(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL; }
template <class T> int CPUCore<T>::ld_h_xhl() { R.setH(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL; }
template <class T> int CPUCore<T>::ld_l_xhl() { R.setL(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL; }

// LD r,(IX+e)
template <class T> inline byte CPUCore<T>::RD_R_XIX()
{
	offset ofst = RDMEM_OPCODE(T::CC_LD_R_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	byte result = RDMEM(memptr, T::CC_LD_R_XIX_2);
	return result;
}
template <class T> int CPUCore<T>::ld_a_xix() { R.setA(RD_R_XIX()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_b_xix() { R.setB(RD_R_XIX()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_c_xix() { R.setC(RD_R_XIX()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_d_xix() { R.setD(RD_R_XIX()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_e_xix() { R.setE(RD_R_XIX()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_h_xix() { R.setH(RD_R_XIX()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_l_xix() { R.setL(RD_R_XIX()); return T::CC_LD_R_XIX; }

// LD r,(IY+e)
template <class T> inline byte CPUCore<T>::RD_R_XIY()
{
	offset ofst = RDMEM_OPCODE(T::CC_LD_R_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	byte result = RDMEM(memptr, T::CC_LD_R_XIX_2);
	return result;
}
template <class T> int CPUCore<T>::ld_a_xiy() { R.setA(RD_R_XIY()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_b_xiy() { R.setB(RD_R_XIY()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_c_xiy() { R.setC(RD_R_XIY()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_d_xiy() { R.setD(RD_R_XIY()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_e_xiy() { R.setE(RD_R_XIY()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_h_xiy() { R.setH(RD_R_XIY()); return T::CC_LD_R_XIX; }
template <class T> int CPUCore<T>::ld_l_xiy() { R.setL(RD_R_XIY()); return T::CC_LD_R_XIX; }

// LD ss,(nn)
template <class T> inline unsigned CPUCore<T>::RD_P_XX(int ee)
{
	unsigned addr = RD_WORD_PC(T::CC_LD_HL_XX_1 + ee);
	memptr = addr + 1; // not 16-bit
	unsigned result = RD_WORD(addr, T::CC_LD_HL_XX_2 + ee);
	return result;
}
template <class T> int CPUCore<T>::ld_hl_xword()  { R.setHL(RD_P_XX(0)); return T::CC_LD_HL_XX; }
template <class T> int CPUCore<T>::ld_ix_xword()  { R.setIX(RD_P_XX(0)); return T::CC_LD_HL_XX; }
template <class T> int CPUCore<T>::ld_iy_xword()  { R.setIY(RD_P_XX(0)); return T::CC_LD_HL_XX; }
template <class T> int CPUCore<T>::ld_bc_xword2() { R.setBC(RD_P_XX(T::EE_ED)); return T::CC_LD_HL_XX + T::EE_ED; }
template <class T> int CPUCore<T>::ld_de_xword2() { R.setDE(RD_P_XX(T::EE_ED)); return T::CC_LD_HL_XX + T::EE_ED; }
template <class T> int CPUCore<T>::ld_hl_xword2() { R.setHL(RD_P_XX(T::EE_ED)); return T::CC_LD_HL_XX + T::EE_ED; }
template <class T> int CPUCore<T>::ld_sp_xword2() { R.setSP(RD_P_XX(T::EE_ED)); return T::CC_LD_HL_XX + T::EE_ED; }

// LD ss,nn
template <class T> int CPUCore<T>::ld_bc_word() { R.setBC(RD_WORD_PC(T::CC_LD_SS_NN_1)); return T::CC_LD_SS_NN; }
template <class T> int CPUCore<T>::ld_de_word() { R.setDE(RD_WORD_PC(T::CC_LD_SS_NN_1)); return T::CC_LD_SS_NN; }
template <class T> int CPUCore<T>::ld_hl_word() { R.setHL(RD_WORD_PC(T::CC_LD_SS_NN_1)); return T::CC_LD_SS_NN; }
template <class T> int CPUCore<T>::ld_ix_word() { R.setIX(RD_WORD_PC(T::CC_LD_SS_NN_1)); return T::CC_LD_SS_NN; }
template <class T> int CPUCore<T>::ld_iy_word() { R.setIY(RD_WORD_PC(T::CC_LD_SS_NN_1)); return T::CC_LD_SS_NN; }
template <class T> int CPUCore<T>::ld_sp_word() { R.setSP(RD_WORD_PC(T::CC_LD_SS_NN_1)); return T::CC_LD_SS_NN; }


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
template <class T> inline int CPUCore<T>::adc_a_a()
{
	unsigned res = 2 * R.getA() + ((R.getF() & C_FLAG) ? 1 : 0);
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       (res & H_FLAG) |
	       (((R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
	return T::CC_CP_R;
}
template <class T> int CPUCore<T>::adc_a_b()   { ADC(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_c()   { ADC(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_d()   { ADC(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_e()   { ADC(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_h()   { ADC(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_l()   { ADC(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_ixl() { ADC(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_ixh() { ADC(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_iyl() { ADC(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_iyh() { ADC(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::adc_a_byte(){ ADC(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::adc_a_xhl() { ADC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::adc_a_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	ADC(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::adc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	ADC(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
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
template <class T> inline int CPUCore<T>::add_a_a()
{
	unsigned res = 2 * R.getA();
	R.setF(ZSXYTable[res & 0xFF] |
	       ((res & 0x100) ? C_FLAG : 0) |
	       (res & H_FLAG) |
	       (((R.getA() ^ res) & 0x80) >> 5)); // V_FLAG
	R.setA(res);
	return T::CC_CP_R;
}
template <class T> int CPUCore<T>::add_a_b()   { ADD(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_c()   { ADD(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_d()   { ADD(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_e()   { ADD(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_h()   { ADD(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_l()   { ADD(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_ixl() { ADD(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_ixh() { ADD(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_iyl() { ADD(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_iyh() { ADD(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::add_a_byte(){ ADD(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::add_a_xhl() { ADD(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::add_a_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	ADD(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::add_a_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	ADD(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// AND r
template <class T> inline void CPUCore<T>::AND(byte reg)
{
	R.setA(R.getA() & reg);
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
}
template <class T> int CPUCore<T>::and_a()
{
	R.setF(ZSPXYTable[R.getA()] | H_FLAG);
	return T::CC_CP_R;
}
template <class T> int CPUCore<T>::and_b()   { AND(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_c()   { AND(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_d()   { AND(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_e()   { AND(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_h()   { AND(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_l()   { AND(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_ixh() { AND(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_ixl() { AND(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_iyh() { AND(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_iyl() { AND(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::and_byte(){ AND(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::and_xhl() { AND(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::and_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	AND(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::and_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	AND(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
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
template <class T> int CPUCore<T>::cp_a()
{
	R.setF(ZS0 | N_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG))); // XY from operand, not from result
	return T::CC_CP_R;
}
template <class T> int CPUCore<T>::cp_b()   { CP(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_c()   { CP(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_d()   { CP(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_e()   { CP(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_h()   { CP(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_l()   { CP(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_ixh() { CP(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_ixl() { CP(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_iyh() { CP(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_iyl() { CP(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::cp_byte(){ CP(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::cp_xhl() { CP(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::cp_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	CP(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::cp_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	CP(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// OR r
template <class T> inline void CPUCore<T>::OR(byte reg)
{
	R.setA(R.getA() | reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> int CPUCore<T>::or_a()   { R.setF(ZSPXYTable[R.getA()]); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_b()   { OR(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_c()   { OR(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_d()   { OR(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_e()   { OR(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_h()   { OR(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_l()   { OR(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_ixh() { OR(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_ixl() { OR(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_iyh() { OR(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_iyl() { OR(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::or_byte(){ OR(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::or_xhl() { OR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::or_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	OR(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::or_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	OR(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
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
template <class T> int CPUCore<T>::sbc_a_a()
{
	R.setAF((R.getF() & C_FLAG) ?
	        (255 * 256 | ZSXY255 | C_FLAG | H_FLAG | N_FLAG) :
	        (  0 * 256 | ZSXY0   |                   N_FLAG));
	return T::CC_CP_R;
}
template <class T> int CPUCore<T>::sbc_a_b()   { SBC(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_c()   { SBC(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_d()   { SBC(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_e()   { SBC(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_h()   { SBC(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_l()   { SBC(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_ixh() { SBC(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_ixl() { SBC(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_iyh() { SBC(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_iyl() { SBC(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sbc_a_byte(){ SBC(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::sbc_a_xhl() { SBC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::sbc_a_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	SBC(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::sbc_a_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	SBC(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
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
template <class T> int CPUCore<T>::sub_a()
{
	R.setAF(0 * 256 | ZSXY0 | N_FLAG);
	return T::CC_CP_R;
}
template <class T> int CPUCore<T>::sub_b()   { SUB(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_c()   { SUB(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_d()   { SUB(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_e()   { SUB(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_h()   { SUB(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_l()   { SUB(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_ixh() { SUB(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_ixl() { SUB(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_iyh() { SUB(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_iyl() { SUB(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::sub_byte(){ SUB(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::sub_xhl() { SUB(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::sub_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	SUB(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::sub_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	SUB(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}

// XOR r
template <class T> inline void CPUCore<T>::XOR(byte reg)
{
	R.setA(R.getA() ^ reg);
	R.setF(ZSPXYTable[R.getA()]);
}
template <class T> int CPUCore<T>::xor_a()   { R.setAF(0 * 256 + ZSPXY0); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_b()   { XOR(R.getB()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_c()   { XOR(R.getC()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_d()   { XOR(R.getD()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_e()   { XOR(R.getE()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_h()   { XOR(R.getH()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_l()   { XOR(R.getL()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_ixh() { XOR(R.getIXh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_ixl() { XOR(R.getIXl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_iyh() { XOR(R.getIYh()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_iyl() { XOR(R.getIYl()); return T::CC_CP_R; }
template <class T> int CPUCore<T>::xor_byte(){ XOR(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N; }
template <class T> int CPUCore<T>::xor_xhl() { XOR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL; }
template <class T> int CPUCore<T>::xor_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	XOR(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
}
template <class T> int CPUCore<T>::xor_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_CP_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	XOR(RDMEM(memptr, T::CC_CP_XIX_2));
	return T::CC_CP_XIX;
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
template <class T> int CPUCore<T>::dec_a()   { R.setA(  DEC(R.getA())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_b()   { R.setB(  DEC(R.getB())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_c()   { R.setC(  DEC(R.getC())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_d()   { R.setD(  DEC(R.getD())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_e()   { R.setE(  DEC(R.getE())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_h()   { R.setH(  DEC(R.getH())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_l()   { R.setL(  DEC(R.getL())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_ixh() { R.setIXh(DEC(R.getIXh())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_ixl() { R.setIXl(DEC(R.getIXl())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_iyh() { R.setIYh(DEC(R.getIYh())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::dec_iyl() { R.setIYl(DEC(R.getIYl())); return T::CC_INC_R; }

template <class T> inline int CPUCore<T>::DEC_X(unsigned x, int ee)
{
	byte val = DEC(RDMEM(x, T::CC_INC_XHL_1 + ee));
	WRMEM(x, val, T::CC_INC_XHL_2 + ee);
	return T::CC_INC_XHL + ee;
}
template <class T> int CPUCore<T>::dec_xhl() { return DEC_X(R.getHL(), 0); }
template <class T> int CPUCore<T>::dec_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	return DEC_X(memptr, T::EE_INC_XIX);
}
template <class T> int CPUCore<T>::dec_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	return DEC_X(memptr, T::EE_INC_XIX);
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
template <class T> int CPUCore<T>::inc_a()   { R.setA(  INC(R.getA())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_b()   { R.setB(  INC(R.getB())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_c()   { R.setC(  INC(R.getC())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_d()   { R.setD(  INC(R.getD())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_e()   { R.setE(  INC(R.getE())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_h()   { R.setH(  INC(R.getH())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_l()   { R.setL(  INC(R.getL())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_ixh() { R.setIXh(INC(R.getIXh())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_ixl() { R.setIXl(INC(R.getIXl())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_iyh() { R.setIYh(INC(R.getIYh())); return T::CC_INC_R; }
template <class T> int CPUCore<T>::inc_iyl() { R.setIYl(INC(R.getIYl())); return T::CC_INC_R; }

template <class T> inline int CPUCore<T>::INC_X(unsigned x, int ee)
{
	byte val = INC(RDMEM(x, T::CC_INC_XHL_1 + ee));
	WRMEM(x, val, T::CC_INC_XHL_2 + ee);
	return T::CC_INC_XHL + ee;
}
template <class T> int CPUCore<T>::inc_xhl() { return INC_X(R.getHL(), 0); }
template <class T> int CPUCore<T>::inc_xix()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIX() + ofst) & 0xFFFF;
	return INC_X(memptr, T::EE_INC_XIX);
}
template <class T> int CPUCore<T>::inc_xiy()
{
	offset ofst = RDMEM_OPCODE(T::CC_INC_XIX_1);
	memptr = (R.getIY() + ofst) & 0xFFFF;
	return INC_X(memptr, T::EE_INC_XIX);
}


// ADC HL,ss
template <class T> inline int CPUCore<T>::ADCW(unsigned reg)
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
	return T::CC_ADC_HL_SS;
}
template <class T> int CPUCore<T>::adc_hl_hl()
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
	return T::CC_ADC_HL_SS;
}
template <class T> int CPUCore<T>::adc_hl_bc() { return ADCW(R.getBC()); }
template <class T> int CPUCore<T>::adc_hl_de() { return ADCW(R.getDE()); }
template <class T> int CPUCore<T>::adc_hl_sp() { return ADCW(R.getSP()); }

// ADD HL/IX/IY,ss
template <class T> inline unsigned CPUCore<T>::ADDW(unsigned reg1, unsigned reg2)
{
	memptr = reg1 + 1; // not 16-bit
	unsigned res = reg1 + reg2;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	       (res >> 16) | // C_FLAG
	       ((res >> 8) & (X_FLAG | Y_FLAG)));
	return res & 0xFFFF;
}
template <class T> inline unsigned CPUCore<T>::ADDW2(unsigned reg)
{
	memptr = reg + 1; // not 16-bit
	unsigned res = 2 * reg;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | V_FLAG)) |
	       (res >> 16) | // C_FLAG
	       ((res >> 8) & (H_FLAG | X_FLAG | Y_FLAG)));
	return res & 0xFFFF;
}
template <class T> int CPUCore<T>::add_hl_bc() {
	R.setHL(ADDW(R.getHL(), R.getBC())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_hl_de() {
	R.setHL(ADDW(R.getHL(), R.getDE())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_hl_hl() {
	R.setHL(ADDW2(R.getHL()));           return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_hl_sp() {
	R.setHL(ADDW(R.getHL(), R.getSP())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_ix_bc() {
	R.setIX(ADDW(R.getIX(), R.getBC())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_ix_de() {
	R.setIX(ADDW(R.getIX(), R.getDE())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_ix_ix() {
	R.setIX(ADDW2(R.getIX()));           return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_ix_sp() {
	R.setIX(ADDW(R.getIX(), R.getSP())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_iy_bc() {
	R.setIY(ADDW(R.getIY(), R.getBC())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_iy_de() {
	R.setIY(ADDW(R.getIY(), R.getDE())); return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_iy_iy() {
	R.setIY(ADDW2(R.getIY()));           return T::CC_ADD_HL_SS;
}
template <class T> int CPUCore<T>::add_iy_sp() {
	R.setIY(ADDW(R.getIY(), R.getSP())); return T::CC_ADD_HL_SS;
}

// SBC HL,ss
template <class T> inline int CPUCore<T>::SBCW(unsigned reg)
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
	return T::CC_ADC_HL_SS;
}
template <class T> int CPUCore<T>::sbc_hl_hl()
{
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
template <class T> int CPUCore<T>::sbc_hl_bc() { return SBCW(R.getBC()); }
template <class T> int CPUCore<T>::sbc_hl_de() { return SBCW(R.getDE()); }
template <class T> int CPUCore<T>::sbc_hl_sp() { return SBCW(R.getSP()); }


// DEC ss
template <class T> int CPUCore<T>::dec_bc() {
	R.setBC(R.getBC() - 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::dec_de() {
	R.setDE(R.getDE() - 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::dec_hl() {
	R.setHL(R.getHL() - 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::dec_ix() {
	R.setIX(R.getIX() - 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::dec_iy() {
	R.setIY(R.getIY() - 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::dec_sp() {
	R.setSP(R.getSP() - 1); return T::CC_INC_SS;
}

// INC ss
template <class T> int CPUCore<T>::inc_bc() {
	R.setBC(R.getBC() + 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::inc_de() {
	R.setDE(R.getDE() + 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::inc_hl() {
	R.setHL(R.getHL() + 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::inc_ix() {
	R.setIX(R.getIX() + 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::inc_iy() {
	R.setIY(R.getIY() + 1); return T::CC_INC_SS;
}
template <class T> int CPUCore<T>::inc_sp() {
	R.setSP(R.getSP() + 1); return T::CC_INC_SS;
}


// BIT n,r
template <class T> inline void CPUCore<T>::BIT(byte b, byte reg)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[reg & (1 << b)] |
	       (reg & (X_FLAG | Y_FLAG)) |
	       H_FLAG);
}
template <class T> int CPUCore<T>::bit_0_a() { BIT(0, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_0_b() { BIT(0, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_0_c() { BIT(0, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_0_d() { BIT(0, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_0_e() { BIT(0, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_0_h() { BIT(0, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_0_l() { BIT(0, R.getL()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_1_a() { BIT(1, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_1_b() { BIT(1, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_1_c() { BIT(1, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_1_d() { BIT(1, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_1_e() { BIT(1, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_1_h() { BIT(1, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_1_l() { BIT(1, R.getL()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_2_a() { BIT(2, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_2_b() { BIT(2, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_2_c() { BIT(2, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_2_d() { BIT(2, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_2_e() { BIT(2, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_2_h() { BIT(2, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_2_l() { BIT(2, R.getL()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_3_a() { BIT(3, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_3_b() { BIT(3, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_3_c() { BIT(3, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_3_d() { BIT(3, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_3_e() { BIT(3, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_3_h() { BIT(3, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_3_l() { BIT(3, R.getL()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_4_a() { BIT(4, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_4_b() { BIT(4, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_4_c() { BIT(4, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_4_d() { BIT(4, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_4_e() { BIT(4, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_4_h() { BIT(4, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_4_l() { BIT(4, R.getL()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_5_a() { BIT(5, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_5_b() { BIT(5, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_5_c() { BIT(5, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_5_d() { BIT(5, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_5_e() { BIT(5, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_5_h() { BIT(5, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_5_l() { BIT(5, R.getL()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_6_a() { BIT(6, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_6_b() { BIT(6, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_6_c() { BIT(6, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_6_d() { BIT(6, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_6_e() { BIT(6, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_6_h() { BIT(6, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_6_l() { BIT(6, R.getL()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_7_a() { BIT(7, R.getA()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_7_b() { BIT(7, R.getB()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_7_c() { BIT(7, R.getC()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_7_d() { BIT(7, R.getD()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_7_e() { BIT(7, R.getE()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_7_h() { BIT(7, R.getH()); return T::CC_BIT_R; }
template <class T> int CPUCore<T>::bit_7_l() { BIT(7, R.getL()); return T::CC_BIT_R; }

template <class T> inline int CPUCore<T>::BIT_HL(byte bit)
{
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(R.getHL(), T::CC_BIT_XHL_1) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	return T::CC_BIT_XHL;
}
template <class T> int CPUCore<T>::bit_0_xhl() { return BIT_HL(0); }
template <class T> int CPUCore<T>::bit_1_xhl() { return BIT_HL(1); }
template <class T> int CPUCore<T>::bit_2_xhl() { return BIT_HL(2); }
template <class T> int CPUCore<T>::bit_3_xhl() { return BIT_HL(3); }
template <class T> int CPUCore<T>::bit_4_xhl() { return BIT_HL(4); }
template <class T> int CPUCore<T>::bit_5_xhl() { return BIT_HL(5); }
template <class T> int CPUCore<T>::bit_6_xhl() { return BIT_HL(6); }
template <class T> int CPUCore<T>::bit_7_xhl() { return BIT_HL(7); }

template <class T> inline int CPUCore<T>::BIT_IX(byte bit, unsigned addr)
{
	memptr = addr;
	R.setF((R.getF() & C_FLAG) |
	       ZSPTable[RDMEM(memptr, T::CC_BIT_XIX_1) & (1 << bit)] |
	       H_FLAG |
	       ((memptr >> 8) & (X_FLAG | Y_FLAG)));
	return T::CC_BIT_XIX;
}
template <class T> int CPUCore<T>::bit_0_xix(unsigned a) { return BIT_IX(0, a); }
template <class T> int CPUCore<T>::bit_1_xix(unsigned a) { return BIT_IX(1, a); }
template <class T> int CPUCore<T>::bit_2_xix(unsigned a) { return BIT_IX(2, a); }
template <class T> int CPUCore<T>::bit_3_xix(unsigned a) { return BIT_IX(3, a); }
template <class T> int CPUCore<T>::bit_4_xix(unsigned a) { return BIT_IX(4, a); }
template <class T> int CPUCore<T>::bit_5_xix(unsigned a) { return BIT_IX(5, a); }
template <class T> int CPUCore<T>::bit_6_xix(unsigned a) { return BIT_IX(6, a); }
template <class T> int CPUCore<T>::bit_7_xix(unsigned a) { return BIT_IX(7, a); }


// RES n,r
template <class T> inline byte CPUCore<T>::RES(unsigned b, byte reg)
{
	return reg & ~(1 << b);
}
template <class T> int CPUCore<T>::res_0_a() { R.setA(RES(0, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_0_b() { R.setB(RES(0, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_0_c() { R.setC(RES(0, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_0_d() { R.setD(RES(0, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_0_e() { R.setE(RES(0, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_0_h() { R.setH(RES(0, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_0_l() { R.setL(RES(0, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_1_a() { R.setA(RES(1, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_1_b() { R.setB(RES(1, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_1_c() { R.setC(RES(1, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_1_d() { R.setD(RES(1, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_1_e() { R.setE(RES(1, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_1_h() { R.setH(RES(1, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_1_l() { R.setL(RES(1, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_2_a() { R.setA(RES(2, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_2_b() { R.setB(RES(2, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_2_c() { R.setC(RES(2, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_2_d() { R.setD(RES(2, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_2_e() { R.setE(RES(2, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_2_h() { R.setH(RES(2, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_2_l() { R.setL(RES(2, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_3_a() { R.setA(RES(3, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_3_b() { R.setB(RES(3, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_3_c() { R.setC(RES(3, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_3_d() { R.setD(RES(3, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_3_e() { R.setE(RES(3, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_3_h() { R.setH(RES(3, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_3_l() { R.setL(RES(3, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_4_a() { R.setA(RES(4, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_4_b() { R.setB(RES(4, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_4_c() { R.setC(RES(4, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_4_d() { R.setD(RES(4, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_4_e() { R.setE(RES(4, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_4_h() { R.setH(RES(4, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_4_l() { R.setL(RES(4, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_5_a() { R.setA(RES(5, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_5_b() { R.setB(RES(5, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_5_c() { R.setC(RES(5, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_5_d() { R.setD(RES(5, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_5_e() { R.setE(RES(5, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_5_h() { R.setH(RES(5, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_5_l() { R.setL(RES(5, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_6_a() { R.setA(RES(6, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_6_b() { R.setB(RES(6, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_6_c() { R.setC(RES(6, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_6_d() { R.setD(RES(6, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_6_e() { R.setE(RES(6, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_6_h() { R.setH(RES(6, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_6_l() { R.setL(RES(6, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_7_a() { R.setA(RES(7, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_7_b() { R.setB(RES(7, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_7_c() { R.setC(RES(7, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_7_d() { R.setD(RES(7, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_7_e() { R.setE(RES(7, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_7_h() { R.setH(RES(7, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::res_7_l() { R.setL(RES(7, R.getL())); return T::CC_SET_R; }

template <class T> inline byte CPUCore<T>::RES_X(unsigned bit, unsigned addr, int ee)
{
	byte res = RES(bit, RDMEM(addr, T::CC_SET_XHL_1 + ee));
	WRMEM(addr, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::RES_X_(unsigned bit, unsigned addr)
{
	memptr = addr;
	return RES_X(bit, addr, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::res_0_xhl()   { RES_X(0, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::res_1_xhl()   { RES_X(1, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::res_2_xhl()   { RES_X(2, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::res_3_xhl()   { RES_X(3, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::res_4_xhl()   { RES_X(4, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::res_5_xhl()   { RES_X(5, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::res_6_xhl()   { RES_X(6, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::res_7_xhl()   { RES_X(7, R.getHL(), 0); return T::CC_SET_XHL; }

template <class T> int CPUCore<T>::res_0_xix(unsigned a)   { RES_X_(0, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix(unsigned a)   { RES_X_(1, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix(unsigned a)   { RES_X_(2, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix(unsigned a)   { RES_X_(3, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix(unsigned a)   { RES_X_(4, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix(unsigned a)   { RES_X_(5, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix(unsigned a)   { RES_X_(6, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix(unsigned a)   { RES_X_(7, a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::res_0_xix_a(unsigned a) { R.setA(RES_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_0_xix_b(unsigned a) { R.setB(RES_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_0_xix_c(unsigned a) { R.setC(RES_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_0_xix_d(unsigned a) { R.setD(RES_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_0_xix_e(unsigned a) { R.setE(RES_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_0_xix_h(unsigned a) { R.setH(RES_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_0_xix_l(unsigned a) { R.setL(RES_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix_a(unsigned a) { R.setA(RES_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix_b(unsigned a) { R.setB(RES_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix_c(unsigned a) { R.setC(RES_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix_d(unsigned a) { R.setD(RES_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix_e(unsigned a) { R.setE(RES_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix_h(unsigned a) { R.setH(RES_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_1_xix_l(unsigned a) { R.setL(RES_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix_a(unsigned a) { R.setA(RES_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix_b(unsigned a) { R.setB(RES_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix_c(unsigned a) { R.setC(RES_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix_d(unsigned a) { R.setD(RES_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix_e(unsigned a) { R.setE(RES_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix_h(unsigned a) { R.setH(RES_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_2_xix_l(unsigned a) { R.setL(RES_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix_a(unsigned a) { R.setA(RES_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix_b(unsigned a) { R.setB(RES_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix_c(unsigned a) { R.setC(RES_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix_d(unsigned a) { R.setD(RES_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix_e(unsigned a) { R.setE(RES_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix_h(unsigned a) { R.setH(RES_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_3_xix_l(unsigned a) { R.setL(RES_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix_a(unsigned a) { R.setA(RES_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix_b(unsigned a) { R.setB(RES_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix_c(unsigned a) { R.setC(RES_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix_d(unsigned a) { R.setD(RES_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix_e(unsigned a) { R.setE(RES_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix_h(unsigned a) { R.setH(RES_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_4_xix_l(unsigned a) { R.setL(RES_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix_a(unsigned a) { R.setA(RES_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix_b(unsigned a) { R.setB(RES_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix_c(unsigned a) { R.setC(RES_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix_d(unsigned a) { R.setD(RES_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix_e(unsigned a) { R.setE(RES_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix_h(unsigned a) { R.setH(RES_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_5_xix_l(unsigned a) { R.setL(RES_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix_a(unsigned a) { R.setA(RES_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix_b(unsigned a) { R.setB(RES_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix_c(unsigned a) { R.setC(RES_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix_d(unsigned a) { R.setD(RES_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix_e(unsigned a) { R.setE(RES_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix_h(unsigned a) { R.setH(RES_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_6_xix_l(unsigned a) { R.setL(RES_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix_a(unsigned a) { R.setA(RES_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix_b(unsigned a) { R.setB(RES_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix_c(unsigned a) { R.setC(RES_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix_d(unsigned a) { R.setD(RES_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix_e(unsigned a) { R.setE(RES_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix_h(unsigned a) { R.setH(RES_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::res_7_xix_l(unsigned a) { R.setL(RES_X_(7, a)); return T::CC_SET_XIX; }


// SET n,r
template <class T> inline byte CPUCore<T>::SET(unsigned b, byte reg)
{
	return reg | (1 << b);
}
template <class T> int CPUCore<T>::set_0_a() { R.setA(SET(0, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_0_b() { R.setB(SET(0, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_0_c() { R.setC(SET(0, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_0_d() { R.setD(SET(0, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_0_e() { R.setE(SET(0, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_0_h() { R.setH(SET(0, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_0_l() { R.setL(SET(0, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_1_a() { R.setA(SET(1, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_1_b() { R.setB(SET(1, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_1_c() { R.setC(SET(1, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_1_d() { R.setD(SET(1, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_1_e() { R.setE(SET(1, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_1_h() { R.setH(SET(1, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_1_l() { R.setL(SET(1, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_2_a() { R.setA(SET(2, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_2_b() { R.setB(SET(2, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_2_c() { R.setC(SET(2, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_2_d() { R.setD(SET(2, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_2_e() { R.setE(SET(2, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_2_h() { R.setH(SET(2, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_2_l() { R.setL(SET(2, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_3_a() { R.setA(SET(3, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_3_b() { R.setB(SET(3, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_3_c() { R.setC(SET(3, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_3_d() { R.setD(SET(3, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_3_e() { R.setE(SET(3, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_3_h() { R.setH(SET(3, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_3_l() { R.setL(SET(3, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_4_a() { R.setA(SET(4, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_4_b() { R.setB(SET(4, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_4_c() { R.setC(SET(4, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_4_d() { R.setD(SET(4, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_4_e() { R.setE(SET(4, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_4_h() { R.setH(SET(4, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_4_l() { R.setL(SET(4, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_5_a() { R.setA(SET(5, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_5_b() { R.setB(SET(5, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_5_c() { R.setC(SET(5, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_5_d() { R.setD(SET(5, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_5_e() { R.setE(SET(5, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_5_h() { R.setH(SET(5, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_5_l() { R.setL(SET(5, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_6_a() { R.setA(SET(6, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_6_b() { R.setB(SET(6, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_6_c() { R.setC(SET(6, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_6_d() { R.setD(SET(6, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_6_e() { R.setE(SET(6, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_6_h() { R.setH(SET(6, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_6_l() { R.setL(SET(6, R.getL())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_7_a() { R.setA(SET(7, R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_7_b() { R.setB(SET(7, R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_7_c() { R.setC(SET(7, R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_7_d() { R.setD(SET(7, R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_7_e() { R.setE(SET(7, R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_7_h() { R.setH(SET(7, R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::set_7_l() { R.setL(SET(7, R.getL())); return T::CC_SET_R; }

template <class T> inline byte CPUCore<T>::SET_X(unsigned bit, unsigned addr, int ee)
{
	byte res = SET(bit, RDMEM(addr, T::CC_SET_XHL_1 + ee));
	WRMEM(addr, res, T::CC_SET_XHL_2 + ee);
	return res;
}
template <class T> inline byte CPUCore<T>::SET_X_(unsigned bit, unsigned addr)
{
	memptr = addr;
	return SET_X(bit, addr, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::set_0_xhl()   { SET_X(0, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::set_1_xhl()   { SET_X(1, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::set_2_xhl()   { SET_X(2, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::set_3_xhl()   { SET_X(3, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::set_4_xhl()   { SET_X(4, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::set_5_xhl()   { SET_X(5, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::set_6_xhl()   { SET_X(6, R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::set_7_xhl()   { SET_X(7, R.getHL(), 0); return T::CC_SET_XHL; }

template <class T> int CPUCore<T>::set_0_xix(unsigned a)   { SET_X_(0, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix(unsigned a)   { SET_X_(1, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix(unsigned a)   { SET_X_(2, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix(unsigned a)   { SET_X_(3, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix(unsigned a)   { SET_X_(4, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix(unsigned a)   { SET_X_(5, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix(unsigned a)   { SET_X_(6, a); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix(unsigned a)   { SET_X_(7, a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::set_0_xix_a(unsigned a) { R.setA(SET_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_0_xix_b(unsigned a) { R.setB(SET_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_0_xix_c(unsigned a) { R.setC(SET_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_0_xix_d(unsigned a) { R.setD(SET_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_0_xix_e(unsigned a) { R.setE(SET_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_0_xix_h(unsigned a) { R.setH(SET_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_0_xix_l(unsigned a) { R.setL(SET_X_(0, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix_a(unsigned a) { R.setA(SET_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix_b(unsigned a) { R.setB(SET_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix_c(unsigned a) { R.setC(SET_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix_d(unsigned a) { R.setD(SET_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix_e(unsigned a) { R.setE(SET_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix_h(unsigned a) { R.setH(SET_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_1_xix_l(unsigned a) { R.setL(SET_X_(1, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix_a(unsigned a) { R.setA(SET_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix_b(unsigned a) { R.setB(SET_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix_c(unsigned a) { R.setC(SET_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix_d(unsigned a) { R.setD(SET_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix_e(unsigned a) { R.setE(SET_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix_h(unsigned a) { R.setH(SET_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_2_xix_l(unsigned a) { R.setL(SET_X_(2, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix_a(unsigned a) { R.setA(SET_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix_b(unsigned a) { R.setB(SET_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix_c(unsigned a) { R.setC(SET_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix_d(unsigned a) { R.setD(SET_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix_e(unsigned a) { R.setE(SET_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix_h(unsigned a) { R.setH(SET_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_3_xix_l(unsigned a) { R.setL(SET_X_(3, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix_a(unsigned a) { R.setA(SET_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix_b(unsigned a) { R.setB(SET_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix_c(unsigned a) { R.setC(SET_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix_d(unsigned a) { R.setD(SET_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix_e(unsigned a) { R.setE(SET_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix_h(unsigned a) { R.setH(SET_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_4_xix_l(unsigned a) { R.setL(SET_X_(4, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix_a(unsigned a) { R.setA(SET_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix_b(unsigned a) { R.setB(SET_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix_c(unsigned a) { R.setC(SET_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix_d(unsigned a) { R.setD(SET_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix_e(unsigned a) { R.setE(SET_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix_h(unsigned a) { R.setH(SET_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_5_xix_l(unsigned a) { R.setL(SET_X_(5, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix_a(unsigned a) { R.setA(SET_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix_b(unsigned a) { R.setB(SET_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix_c(unsigned a) { R.setC(SET_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix_d(unsigned a) { R.setD(SET_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix_e(unsigned a) { R.setE(SET_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix_h(unsigned a) { R.setH(SET_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_6_xix_l(unsigned a) { R.setL(SET_X_(6, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix_a(unsigned a) { R.setA(SET_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix_b(unsigned a) { R.setB(SET_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix_c(unsigned a) { R.setC(SET_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix_d(unsigned a) { R.setD(SET_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix_e(unsigned a) { R.setE(SET_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix_h(unsigned a) { R.setH(SET_X_(7, a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::set_7_xix_l(unsigned a) { R.setL(SET_X_(7, a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::RL_X_(unsigned x)
{
	memptr = x; return RL_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::rl_a() { R.setA(RL(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rl_b() { R.setB(RL(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rl_c() { R.setC(RL(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rl_d() { R.setD(RL(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rl_e() { R.setE(RL(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rl_h() { R.setH(RL(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rl_l() { R.setL(RL(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::rl_xhl()      { RL_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::rl_xix(unsigned a) { RL_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::rl_xix_a(unsigned a) { R.setA(RL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rl_xix_b(unsigned a) { R.setB(RL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rl_xix_c(unsigned a) { R.setC(RL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rl_xix_d(unsigned a) { R.setD(RL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rl_xix_e(unsigned a) { R.setE(RL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rl_xix_h(unsigned a) { R.setH(RL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rl_xix_l(unsigned a) { R.setL(RL_X_(a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::RLC_X_(unsigned x)
{
	memptr = x; return RLC_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::rlc_a() { R.setA(RLC(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rlc_b() { R.setB(RLC(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rlc_c() { R.setC(RLC(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rlc_d() { R.setD(RLC(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rlc_e() { R.setE(RLC(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rlc_h() { R.setH(RLC(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rlc_l() { R.setL(RLC(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::rlc_xhl()      { RLC_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::rlc_xix(unsigned a) { RLC_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::rlc_xix_a(unsigned a) { R.setA(RLC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rlc_xix_b(unsigned a) { R.setB(RLC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rlc_xix_c(unsigned a) { R.setC(RLC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rlc_xix_d(unsigned a) { R.setD(RLC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rlc_xix_e(unsigned a) { R.setE(RLC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rlc_xix_h(unsigned a) { R.setH(RLC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rlc_xix_l(unsigned a) { R.setL(RLC_X_(a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::RR_X_(unsigned x)
{
	memptr = x; return RR_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::rr_a() { R.setA(RR(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rr_b() { R.setB(RR(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rr_c() { R.setC(RR(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rr_d() { R.setD(RR(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rr_e() { R.setE(RR(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rr_h() { R.setH(RR(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rr_l() { R.setL(RR(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::rr_xhl()      { RR_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::rr_xix(unsigned a) { RR_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::rr_xix_a(unsigned a) { R.setA(RR_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rr_xix_b(unsigned a) { R.setB(RR_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rr_xix_c(unsigned a) { R.setC(RR_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rr_xix_d(unsigned a) { R.setD(RR_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rr_xix_e(unsigned a) { R.setE(RR_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rr_xix_h(unsigned a) { R.setH(RR_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rr_xix_l(unsigned a) { R.setL(RR_X_(a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::RRC_X_(unsigned x)
{
	memptr = x; return RRC_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::rrc_a() { R.setA(RRC(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rrc_b() { R.setB(RRC(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rrc_c() { R.setC(RRC(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rrc_d() { R.setD(RRC(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rrc_e() { R.setE(RRC(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rrc_h() { R.setH(RRC(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::rrc_l() { R.setL(RRC(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::rrc_xhl()      { RRC_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::rrc_xix(unsigned a) { RRC_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::rrc_xix_a(unsigned a) { R.setA(RRC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rrc_xix_b(unsigned a) { R.setB(RRC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rrc_xix_c(unsigned a) { R.setC(RRC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rrc_xix_d(unsigned a) { R.setD(RRC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rrc_xix_e(unsigned a) { R.setE(RRC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rrc_xix_h(unsigned a) { R.setH(RRC_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::rrc_xix_l(unsigned a) { R.setL(RRC_X_(a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::SLA_X_(unsigned x)
{
	memptr = x; return SLA_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::sla_a() { R.setA(SLA(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sla_b() { R.setB(SLA(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sla_c() { R.setC(SLA(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sla_d() { R.setD(SLA(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sla_e() { R.setE(SLA(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sla_h() { R.setH(SLA(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sla_l() { R.setL(SLA(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::sla_xhl()      { SLA_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::sla_xix(unsigned a) { SLA_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::sla_xix_a(unsigned a) { R.setA(SLA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sla_xix_b(unsigned a) { R.setB(SLA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sla_xix_c(unsigned a) { R.setC(SLA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sla_xix_d(unsigned a) { R.setD(SLA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sla_xix_e(unsigned a) { R.setE(SLA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sla_xix_h(unsigned a) { R.setH(SLA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sla_xix_l(unsigned a) { R.setL(SLA_X_(a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::SLL_X_(unsigned x)
{
	memptr = x; return SLL_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::sll_a() { R.setA(SLL(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sll_b() { R.setB(SLL(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sll_c() { R.setC(SLL(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sll_d() { R.setD(SLL(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sll_e() { R.setE(SLL(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sll_h() { R.setH(SLL(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sll_l() { R.setL(SLL(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::sll_xhl()      { SLL_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::sll_xix(unsigned a) { SLL_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::sll_xix_a(unsigned a) { R.setA(SLL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sll_xix_b(unsigned a) { R.setB(SLL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sll_xix_c(unsigned a) { R.setC(SLL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sll_xix_d(unsigned a) { R.setD(SLL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sll_xix_e(unsigned a) { R.setE(SLL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sll_xix_h(unsigned a) { R.setH(SLL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sll_xix_l(unsigned a) { R.setL(SLL_X_(a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::SRA_X_(unsigned x)
{
	memptr = x; return SRA_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::sra_a() { R.setA(SRA(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sra_b() { R.setB(SRA(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sra_c() { R.setC(SRA(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sra_d() { R.setD(SRA(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sra_e() { R.setE(SRA(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sra_h() { R.setH(SRA(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::sra_l() { R.setL(SRA(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::sra_xhl()      { SRA_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::sra_xix(unsigned a) { SRA_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::sra_xix_a(unsigned a) { R.setA(SRA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sra_xix_b(unsigned a) { R.setB(SRA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sra_xix_c(unsigned a) { R.setC(SRA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sra_xix_d(unsigned a) { R.setD(SRA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sra_xix_e(unsigned a) { R.setE(SRA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sra_xix_h(unsigned a) { R.setH(SRA_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::sra_xix_l(unsigned a) { R.setL(SRA_X_(a)); return T::CC_SET_XIX; }


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
	return T::CC_SET_XHL + ee;
	return res;
}
template <class T> inline byte CPUCore<T>::SRL_X_(unsigned x)
{
	memptr = x; return SRL_X(x, T::EE_SET_XIX);
}

template <class T> int CPUCore<T>::srl_a() { R.setA(SRL(R.getA())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::srl_b() { R.setB(SRL(R.getB())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::srl_c() { R.setC(SRL(R.getC())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::srl_d() { R.setD(SRL(R.getD())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::srl_e() { R.setE(SRL(R.getE())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::srl_h() { R.setH(SRL(R.getH())); return T::CC_SET_R; }
template <class T> int CPUCore<T>::srl_l() { R.setL(SRL(R.getL())); return T::CC_SET_R; }

template <class T> int CPUCore<T>::srl_xhl()      { SRL_X(R.getHL(), 0); return T::CC_SET_XHL; }
template <class T> int CPUCore<T>::srl_xix(unsigned a) { SRL_X_(a); return T::CC_SET_XIX; }

template <class T> int CPUCore<T>::srl_xix_a(unsigned a) { R.setA(SRL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::srl_xix_b(unsigned a) { R.setB(SRL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::srl_xix_c(unsigned a) { R.setC(SRL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::srl_xix_d(unsigned a) { R.setD(SRL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::srl_xix_e(unsigned a) { R.setE(SRL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::srl_xix_h(unsigned a) { R.setH(SRL_X_(a)); return T::CC_SET_XIX; }
template <class T> int CPUCore<T>::srl_xix_l(unsigned a) { R.setL(SRL_X_(a)); return T::CC_SET_XIX; }


// RLA RLCA RRA RRCA
template <class T> int CPUCore<T>::rla()
{
	byte c = R.getF() & C_FLAG;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x80) ? C_FLAG : 0));
	R.setA((R.getA() << 1) | (c ? 1 : 0));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rlca()
{
	R.setA((R.getA() << 1) | (R.getA() >> 7));
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() & (Y_FLAG | X_FLAG | C_FLAG)));
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rra()
{
	byte c = (R.getF() & C_FLAG) << 7;
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       ((R.getA() & 0x01) ? C_FLAG : 0));
	R.setA((R.getA() >> 1) | c);
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rrca()
{
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       (R.getA() &  C_FLAG));
	R.setA((R.getA() >> 1) | (R.getA() << 7));
	R.setF(R.getF() | (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_RLA;
}


// RLD
template <class T> int CPUCore<T>::rld()
{
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	memptr = R.getHL() + 1; // not 16-bit
	WRMEM(R.getHL(), (val << 4) | (R.getA() & 0x0F), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val >> 4));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
	return T::CC_RLD;
}

// RRD
template <class T> int CPUCore<T>::rrd()
{
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	memptr = R.getHL() + 1; // not 16-bit
	WRMEM(R.getHL(), (val >> 4) | (R.getA() << 4), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val & 0x0F));
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[R.getA()]);
	return T::CC_RLD;
}


// PUSH ss
template <class T> inline void CPUCore<T>::PUSH(unsigned reg, int ee)
{
	R.setSP(R.getSP() - 2);
	WR_WORD_rev(R.getSP(), reg, T::CC_PUSH_1 + ee);
}
template <class T> int CPUCore<T>::push_af() { PUSH(R.getAF(), 0); return T::CC_PUSH; }
template <class T> int CPUCore<T>::push_bc() { PUSH(R.getBC(), 0); return T::CC_PUSH; }
template <class T> int CPUCore<T>::push_de() { PUSH(R.getDE(), 0); return T::CC_PUSH; }
template <class T> int CPUCore<T>::push_hl() { PUSH(R.getHL(), 0); return T::CC_PUSH; }
template <class T> int CPUCore<T>::push_ix() { PUSH(R.getIX(), 0); return T::CC_PUSH; }
template <class T> int CPUCore<T>::push_iy() { PUSH(R.getIY(), 0); return T::CC_PUSH; }


// POP ss
template <class T> inline unsigned CPUCore<T>::POP(int ee)
{
	unsigned addr = R.getSP();
	R.setSP(addr + 2);
	return RD_WORD(addr, T::CC_POP_1 + ee);
}
template <class T> int CPUCore<T>::pop_af() { R.setAF(POP(0)); return T::CC_POP; }
template <class T> int CPUCore<T>::pop_bc() { R.setBC(POP(0)); return T::CC_POP; }
template <class T> int CPUCore<T>::pop_de() { R.setDE(POP(0)); return T::CC_POP; }
template <class T> int CPUCore<T>::pop_hl() { R.setHL(POP(0)); return T::CC_POP; }
template <class T> int CPUCore<T>::pop_ix() { R.setIX(POP(0)); return T::CC_POP; }
template <class T> int CPUCore<T>::pop_iy() { R.setIY(POP(0)); return T::CC_POP; }


// CALL nn / CALL cc,nn
template <class T> inline int CPUCore<T>::CALL()
{
	memptr = RD_WORD_PC(T::CC_CALL_1);
	PUSH(R.getPC(), T::EE_CALL);
	R.setPC(memptr);
	return T::CC_CALL_A;
}
template <class T> inline int CPUCore<T>::SKIP_CALL()
{
	memptr = RD_WORD_PC(T::CC_CALL_1);
	return T::CC_CALL_B;
}
template <class T> int CPUCore<T>::call()    { return        CALL();               }
template <class T> int CPUCore<T>::call_c()  { return C()  ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_m()  { return M()  ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_nc() { return NC() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_nz() { return NZ() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_p()  { return P()  ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_pe() { return PE() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_po() { return PO() ? CALL() : SKIP_CALL(); }
template <class T> int CPUCore<T>::call_z()  { return Z()  ? CALL() : SKIP_CALL(); }


// RST n
template <class T> inline int CPUCore<T>::RST(unsigned x)
{
	PUSH(R.getPC(), 0);
	memptr = x;
	R.setPC(x);
	return T::CC_RST;
}
template <class T> int CPUCore<T>::rst_00() { return RST(0x00); }
template <class T> int CPUCore<T>::rst_08() { return RST(0x08); }
template <class T> int CPUCore<T>::rst_10() { return RST(0x10); }
template <class T> int CPUCore<T>::rst_18() { return RST(0x18); }
template <class T> int CPUCore<T>::rst_20() { return RST(0x20); }
template <class T> int CPUCore<T>::rst_28() { return RST(0x28); }
template <class T> int CPUCore<T>::rst_30() { return RST(0x30); }
template <class T> int CPUCore<T>::rst_38() { return RST(0x38); }


// RET
template <class T> inline int CPUCore<T>::RET(bool cond, int ee)
{
	if (cond) {
		memptr = POP(ee);
		R.setPC(memptr);
		return T::CC_RET_A + ee;
	} else {
		return T::CC_RET_B + ee;
	}
}
template <class T> int CPUCore<T>::ret()    { return RET(true, 0); }
template <class T> int CPUCore<T>::ret_c()  { return RET(C(),  T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_m()  { return RET(M(),  T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_nc() { return RET(NC(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_nz() { return RET(NZ(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_p()  { return RET(P(),  T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_pe() { return RET(PE(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_po() { return RET(PO(), T::EE_RET_C); }
template <class T> int CPUCore<T>::ret_z()  { return RET(Z(),  T::EE_RET_C); }

template <class T> int CPUCore<T>::retn() // also reti
{
	R.setIFF1(R.getIFF2());
	R.setNextIFF1(R.getIFF2());
	setSlowInstructions();
	return RET(true, T::EE_RETN);
}


// JP ss
template <class T> int CPUCore<T>::jp_hl() { R.setPC(R.getHL()); return T::CC_JP_HL; }
template <class T> int CPUCore<T>::jp_ix() { R.setPC(R.getIX()); return T::CC_JP_HL; }
template <class T> int CPUCore<T>::jp_iy() { R.setPC(R.getIY()); return T::CC_JP_HL; }

// JP nn / JP cc,nn
template <class T> inline int CPUCore<T>::JP()
{
	memptr = RD_WORD_PC(T::CC_JP_1);
	R.setPC(memptr);
	return T::CC_JP;
}
template <class T> inline int CPUCore<T>::SKIP_JP()
{
	memptr = RD_WORD_PC(T::CC_JP_1);
	return T::CC_JP;
}
template <class T> int CPUCore<T>::jp()    { return        JP();             }
template <class T> int CPUCore<T>::jp_c()  { return C()  ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_m()  { return M()  ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_nc() { return NC() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_nz() { return NZ() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_p()  { return P()  ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_pe() { return PE() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_po() { return PO() ? JP() : SKIP_JP(); }
template <class T> int CPUCore<T>::jp_z()  { return Z()  ? JP() : SKIP_JP(); }

// JR e
template <class T> inline int CPUCore<T>::JR(int ee)
{
	offset ofst = RDMEM_OPCODE(T::CC_JR_1 + ee);
	R.setPC((R.getPC() + ofst) & 0xFFFF);
	memptr = R.getPC();
	return T::CC_JR_A + ee;
}
template <class T> inline int CPUCore<T>::SKIP_JR(int ee)
{
	RDMEM_OPCODE(T::CC_JR_1 + ee); // ignore return value
	return T::CC_JR_B + ee;
}
template <class T> int CPUCore<T>::jr()    { return        JR(0);              }
template <class T> int CPUCore<T>::jr_c()  { return C()  ? JR(0) : SKIP_JR(0); }
template <class T> int CPUCore<T>::jr_nc() { return NC() ? JR(0) : SKIP_JR(0); }
template <class T> int CPUCore<T>::jr_nz() { return NZ() ? JR(0) : SKIP_JR(0); }
template <class T> int CPUCore<T>::jr_z()  { return Z()  ? JR(0) : SKIP_JR(0); }

// DJNZ e
template <class T> int CPUCore<T>::djnz()
{
	byte b = R.getB();
	R.setB(--b);
	return b ? JR(T::EE_DJNZ) : SKIP_JR(T::EE_DJNZ);
}

// EX (SP),ss
template <class T> inline unsigned CPUCore<T>::EX_SP(unsigned reg)
{
	memptr = RD_WORD(R.getSP(), T::CC_EX_SP_HL_1);
	WR_WORD_rev(R.getSP(), reg, T::CC_EX_SP_HL_2);
	return memptr;
}
template <class T> int CPUCore<T>::ex_xsp_hl() { R.setHL(EX_SP(R.getHL())); return T::CC_EX_SP_HL; }
template <class T> int CPUCore<T>::ex_xsp_ix() { R.setIX(EX_SP(R.getIX())); return T::CC_EX_SP_HL; }
template <class T> int CPUCore<T>::ex_xsp_iy() { R.setIY(EX_SP(R.getIY())); return T::CC_EX_SP_HL; }


// IN r,(c)
template <class T> inline byte CPUCore<T>::IN()
{
	byte res = READ_PORT(R.getBC(), T::CC_IN_R_C_1);
	R.setF((R.getF() & C_FLAG) | ZSPXYTable[res]);
	return res;
}
template <class T> int CPUCore<T>::in_a_c() { R.setA(IN()); return T::CC_IN_R_C; }
template <class T> int CPUCore<T>::in_b_c() { R.setB(IN()); return T::CC_IN_R_C; }
template <class T> int CPUCore<T>::in_c_c() { R.setC(IN()); return T::CC_IN_R_C; }
template <class T> int CPUCore<T>::in_d_c() { R.setD(IN()); return T::CC_IN_R_C; }
template <class T> int CPUCore<T>::in_e_c() { R.setE(IN()); return T::CC_IN_R_C; }
template <class T> int CPUCore<T>::in_h_c() { R.setH(IN()); return T::CC_IN_R_C; }
template <class T> int CPUCore<T>::in_l_c() { R.setL(IN()); return T::CC_IN_R_C; }
template <class T> int CPUCore<T>::in_0_c() { IN(); return T::CC_IN_R_C; } // discard result

// IN a,(n)
template <class T> int CPUCore<T>::in_a_byte()
{
	unsigned y = RDMEM_OPCODE(T::CC_IN_A_N_1) + 256 * R.getA();
	R.setA(READ_PORT(y, T::CC_IN_A_N_2));
	return T::CC_IN_A_N;
}


// OUT (c),r
template <class T> inline int CPUCore<T>::OUT(byte val)
{
	WRITE_PORT(R.getBC(), val, T::CC_OUT_C_R_1);
	return T::CC_OUT_C_R;
}
template <class T> int CPUCore<T>::out_c_a()   { return OUT(R.getA()); }
template <class T> int CPUCore<T>::out_c_b()   { return OUT(R.getB()); }
template <class T> int CPUCore<T>::out_c_c()   { return OUT(R.getC()); }
template <class T> int CPUCore<T>::out_c_d()   { return OUT(R.getD()); }
template <class T> int CPUCore<T>::out_c_e()   { return OUT(R.getE()); }
template <class T> int CPUCore<T>::out_c_h()   { return OUT(R.getH()); }
template <class T> int CPUCore<T>::out_c_l()   { return OUT(R.getL()); }
template <class T> int CPUCore<T>::out_c_0()   { return OUT(0);        }

// OUT (n),a
template <class T> int CPUCore<T>::out_byte_a()
{
	unsigned y = RDMEM_OPCODE(T::CC_OUT_N_A_1) + 256 * R.getA();
	WRITE_PORT(y, R.getA(), T::CC_OUT_N_A_2);
	return T::CC_OUT_N_A;
}


// block CP
template <class T> inline int CPUCore<T>::BLOCK_CP(int increase, bool repeat)
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
template <class T> inline int CPUCore<T>::BLOCK_LD(int increase, bool repeat)
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
template <class T> inline int CPUCore<T>::BLOCK_IN(int increase, bool repeat)
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
template <class T> inline int CPUCore<T>::BLOCK_OUT(int increase, bool repeat)
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
template <class T> int CPUCore<T>::ccf()
{
	R.setF(((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	       ((R.getF() & C_FLAG) << 4) | // H_FLAG
	       (R.getA() & (X_FLAG | Y_FLAG))                  ) ^ C_FLAG);
	return T::CC_CCF;
}
template <class T> int CPUCore<T>::cpl()
{
	R.setA(R.getA() ^ 0xFF);
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG)) |
	       H_FLAG | N_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_CPL;

}
template <class T> int CPUCore<T>::daa()
{
	unsigned i = R.getA();
	i |= (R.getF() & (C_FLAG | N_FLAG)) << 8; // 0x100 0x200
	i |= (R.getF() &  H_FLAG)           << 6; // 0x400
	R.setAF(DAATable[i]);
	return T::CC_DAA;
}
template <class T> int CPUCore<T>::neg()
{
	 byte i = R.getA();
	 R.setA(0);
	 SUB(i);
	return T::CC_NEG;
}
template <class T> int CPUCore<T>::scf()
{
	R.setF((R.getF() & (S_FLAG | Z_FLAG | P_FLAG)) |
	       C_FLAG |
	       (R.getA() & (X_FLAG | Y_FLAG)));
	return T::CC_SCF;
}

template <class T> int CPUCore<T>::ex_af_af()
{
	unsigned t = R.getAF2(); R.setAF2(R.getAF()); R.setAF(t);
	return T::CC_EX;
}
template <class T> int CPUCore<T>::ex_de_hl()
{
	unsigned t = R.getDE(); R.setDE(R.getHL()); R.setHL(t);
	return T::CC_EX;
}
template <class T> int CPUCore<T>::exx()
{
	unsigned t1 = R.getBC2(); R.setBC2(R.getBC()); R.setBC(t1);
	unsigned t2 = R.getDE2(); R.setDE2(R.getDE()); R.setDE(t2);
	unsigned t3 = R.getHL2(); R.setHL2(R.getHL()); R.setHL(t3);
	return T::CC_EX;
}

template <class T> int CPUCore<T>::di()
{
	R.di();
	return T::CC_DI;
}
template <class T> int CPUCore<T>::ei()
{
	R.setIFF1(false);	// no ints after this instruction
	R.setNextIFF1(true);	// but allow them after next instruction
	R.setIFF2(true);
	setSlowInstructions();
	return T::CC_EI;
}
template <class T> int CPUCore<T>::halt()
{
	R.setHALT(true);
	setSlowInstructions();

	if (!(R.getIFF1() || R.getNextIFF1() || R.getIFF2())) {
		motherboard.getMSXCliComm().printWarning(
			"DI; HALT detected, which means a hang. "
			"You can just as well reset the MSX now...\n");
	}
	return T::CC_HALT;
}
template <class T> int CPUCore<T>::im_0() { R.setIM(0); return T::CC_IM; }
template <class T> int CPUCore<T>::im_1() { R.setIM(1); return T::CC_IM; }
template <class T> int CPUCore<T>::im_2() { R.setIM(2); return T::CC_IM; }

// LD A,I/R
template <class T> int CPUCore<T>::ld_a_i()
{
	R.setA(R.getI());
	R.setF((R.getF() & C_FLAG) |
	       ZSXYTable[R.getA()] |
	       (R.getIFF2() ? V_FLAG : 0));
	return T::CC_LD_A_I;
}
template <class T> int CPUCore<T>::ld_a_r()
{
	R.setA(R.getR());
	R.setF((R.getF() & C_FLAG) |
	       ZSXYTable[R.getA()] |
	       (R.getIFF2() ? V_FLAG : 0));
	return T::CC_LD_A_I;
}

// LD I/R,A
template <class T> int CPUCore<T>::ld_i_a()
{
	R.setI(R.getA());
	return T::CC_LD_A_I;
}
template <class T> int CPUCore<T>::ld_r_a()
{
	R.setR(R.getA());
	return T::CC_LD_A_I;
}

// MULUB A,r
template <class T> inline int CPUCore<T>::MULUB(byte reg)
{
	// TODO check flags
	R.setHL(unsigned(R.getA()) * reg);
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (R.getHL() ? 0 : Z_FLAG) |
	       ((R.getHL() & 0x8000) ? C_FLAG : 0));
	return T::CC_MULUB;
}
//template <class T> int CPUCore<T>::mulub_a_xhl()   { } // TODO
//template <class T> int CPUCore<T>::mulub_a_a()     { } // TODO
template <class T> int CPUCore<T>::mulub_a_b()   { return MULUB(R.getB()); }
template <class T> int CPUCore<T>::mulub_a_c()   { return MULUB(R.getC()); }
template <class T> int CPUCore<T>::mulub_a_d()   { return MULUB(R.getD()); }
template <class T> int CPUCore<T>::mulub_a_e()   { return MULUB(R.getE()); }
//template <class T> int CPUCore<T>::mulub_a_h()     { } // TODO
//template <class T> int CPUCore<T>::mulub_a_l()     { } // TODO

// MULUW HL,ss
template <class T> inline int CPUCore<T>::MULUW(unsigned reg)
{
	// TODO check flags
	unsigned res = unsigned(R.getHL()) * reg;
	R.setDE(res >> 16);
	R.setHL(res & 0xffff);
	R.setF((R.getF() & (N_FLAG | H_FLAG)) |
	       (res ? 0 : Z_FLAG) |
	       ((res & 0x80000000) ? C_FLAG : 0));
	return T::CC_MULUW;
}
template <class T> int CPUCore<T>::muluw_hl_bc() { return MULUW(R.getBC()); }
//template <class T> int CPUCore<T>::muluw_hl_de()   { } // TODO
//template <class T> int CPUCore<T>::muluw_hl_hl()   { } // TODO
template <class T> int CPUCore<T>::muluw_hl_sp() { return MULUW(R.getSP()); }


// prefixes
template <class T> ALWAYS_INLINE int CPUCore<T>::nn_cb(unsigned reg)
{
	unsigned tmp = RD_WORD_PC(T::CC_DD_CB);
	offset ofst = tmp & 0xFF;
	unsigned addr = (reg + ofst) & 0xFFFF;
	unsigned opcode = tmp >> 8;
	switch (opcode) {
		case 0x00: return rlc_xix_b(addr);
		case 0x01: return rlc_xix_c(addr);
		case 0x02: return rlc_xix_d(addr);
		case 0x03: return rlc_xix_e(addr);
		case 0x04: return rlc_xix_h(addr);
		case 0x05: return rlc_xix_l(addr);
		case 0x06: return rlc_xix(addr);
		case 0x07: return rlc_xix_a(addr);
		case 0x08: return rrc_xix_b(addr);
		case 0x09: return rrc_xix_c(addr);
		case 0x0a: return rrc_xix_d(addr);
		case 0x0b: return rrc_xix_e(addr);
		case 0x0c: return rrc_xix_h(addr);
		case 0x0d: return rrc_xix_l(addr);
		case 0x0e: return rrc_xix(addr);
		case 0x0f: return rrc_xix_a(addr);
		case 0x10: return rl_xix_b(addr);
		case 0x11: return rl_xix_c(addr);
		case 0x12: return rl_xix_d(addr);
		case 0x13: return rl_xix_e(addr);
		case 0x14: return rl_xix_h(addr);
		case 0x15: return rl_xix_l(addr);
		case 0x16: return rl_xix(addr);
		case 0x17: return rl_xix_a(addr);
		case 0x18: return rr_xix_b(addr);
		case 0x19: return rr_xix_c(addr);
		case 0x1a: return rr_xix_d(addr);
		case 0x1b: return rr_xix_e(addr);
		case 0x1c: return rr_xix_h(addr);
		case 0x1d: return rr_xix_l(addr);
		case 0x1e: return rr_xix(addr);
		case 0x1f: return rr_xix_a(addr);
		case 0x20: return sla_xix_b(addr);
		case 0x21: return sla_xix_c(addr);
		case 0x22: return sla_xix_d(addr);
		case 0x23: return sla_xix_e(addr);
		case 0x24: return sla_xix_h(addr);
		case 0x25: return sla_xix_l(addr);
		case 0x26: return sla_xix(addr);
		case 0x27: return sla_xix_a(addr);
		case 0x28: return sra_xix_b(addr);
		case 0x29: return sra_xix_c(addr);
		case 0x2a: return sra_xix_d(addr);
		case 0x2b: return sra_xix_e(addr);
		case 0x2c: return sra_xix_h(addr);
		case 0x2d: return sra_xix_l(addr);
		case 0x2e: return sra_xix(addr);
		case 0x2f: return sra_xix_a(addr);
		case 0x30: return sll_xix_b(addr);
		case 0x31: return sll_xix_c(addr);
		case 0x32: return sll_xix_d(addr);
		case 0x33: return sll_xix_e(addr);
		case 0x34: return sll_xix_h(addr);
		case 0x35: return sll_xix_l(addr);
		case 0x36: return sll_xix(addr);
		case 0x37: return sll_xix_a(addr);
		case 0x38: return srl_xix_b(addr);
		case 0x39: return srl_xix_c(addr);
		case 0x3a: return srl_xix_d(addr);
		case 0x3b: return srl_xix_e(addr);
		case 0x3c: return srl_xix_h(addr);
		case 0x3d: return srl_xix_l(addr);
		case 0x3e: return srl_xix(addr);
		case 0x3f: return srl_xix_a(addr);

		case 0x40: case 0x41: case 0x42: case 0x43:
		case 0x44: case 0x45: case 0x46: case 0x47:
			return bit_0_xix(addr);
		case 0x48: case 0x49: case 0x4a: case 0x4b:
		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
			return bit_1_xix(addr);
		case 0x50: case 0x51: case 0x52: case 0x53:
		case 0x54: case 0x55: case 0x56: case 0x57:
			return bit_2_xix(addr);
		case 0x58: case 0x59: case 0x5a: case 0x5b:
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:
			return bit_3_xix(addr);
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x66: case 0x67:
			return bit_4_xix(addr);
		case 0x68: case 0x69: case 0x6a: case 0x6b:
		case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			return bit_5_xix(addr);
		case 0x70: case 0x71: case 0x72: case 0x73:
		case 0x74: case 0x75: case 0x76: case 0x77:
			return bit_6_xix(addr);
		case 0x78: case 0x79: case 0x7a: case 0x7b:
		case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			return bit_7_xix(addr);

		case 0x80: return res_0_xix_b(addr);
		case 0x81: return res_0_xix_c(addr);
		case 0x82: return res_0_xix_d(addr);
		case 0x83: return res_0_xix_e(addr);
		case 0x84: return res_0_xix_h(addr);
		case 0x85: return res_0_xix_l(addr);
		case 0x86: return res_0_xix(addr);
		case 0x87: return res_0_xix_a(addr);
		case 0x88: return res_1_xix_b(addr);
		case 0x89: return res_1_xix_c(addr);
		case 0x8a: return res_1_xix_d(addr);
		case 0x8b: return res_1_xix_e(addr);
		case 0x8c: return res_1_xix_h(addr);
		case 0x8d: return res_1_xix_l(addr);
		case 0x8e: return res_1_xix(addr);
		case 0x8f: return res_1_xix_a(addr);
		case 0x90: return res_2_xix_b(addr);
		case 0x91: return res_2_xix_c(addr);
		case 0x92: return res_2_xix_d(addr);
		case 0x93: return res_2_xix_e(addr);
		case 0x94: return res_2_xix_h(addr);
		case 0x95: return res_2_xix_l(addr);
		case 0x96: return res_2_xix(addr);
		case 0x97: return res_2_xix_a(addr);
		case 0x98: return res_3_xix_b(addr);
		case 0x99: return res_3_xix_c(addr);
		case 0x9a: return res_3_xix_d(addr);
		case 0x9b: return res_3_xix_e(addr);
		case 0x9c: return res_3_xix_h(addr);
		case 0x9d: return res_3_xix_l(addr);
		case 0x9e: return res_3_xix(addr);
		case 0x9f: return res_3_xix_a(addr);
		case 0xa0: return res_4_xix_b(addr);
		case 0xa1: return res_4_xix_c(addr);
		case 0xa2: return res_4_xix_d(addr);
		case 0xa3: return res_4_xix_e(addr);
		case 0xa4: return res_4_xix_h(addr);
		case 0xa5: return res_4_xix_l(addr);
		case 0xa6: return res_4_xix(addr);
		case 0xa7: return res_4_xix_a(addr);
		case 0xa8: return res_5_xix_b(addr);
		case 0xa9: return res_5_xix_c(addr);
		case 0xaa: return res_5_xix_d(addr);
		case 0xab: return res_5_xix_e(addr);
		case 0xac: return res_5_xix_h(addr);
		case 0xad: return res_5_xix_l(addr);
		case 0xae: return res_5_xix(addr);
		case 0xaf: return res_5_xix_a(addr);
		case 0xb0: return res_6_xix_b(addr);
		case 0xb1: return res_6_xix_c(addr);
		case 0xb2: return res_6_xix_d(addr);
		case 0xb3: return res_6_xix_e(addr);
		case 0xb4: return res_6_xix_h(addr);
		case 0xb5: return res_6_xix_l(addr);
		case 0xb6: return res_6_xix(addr);
		case 0xb7: return res_6_xix_a(addr);
		case 0xb8: return res_7_xix_b(addr);
		case 0xb9: return res_7_xix_c(addr);
		case 0xba: return res_7_xix_d(addr);
		case 0xbb: return res_7_xix_e(addr);
		case 0xbc: return res_7_xix_h(addr);
		case 0xbd: return res_7_xix_l(addr);
		case 0xbe: return res_7_xix(addr);
		case 0xbf: return res_7_xix_a(addr);

		case 0xc0: return set_0_xix_b(addr);
		case 0xc1: return set_0_xix_c(addr);
		case 0xc2: return set_0_xix_d(addr);
		case 0xc3: return set_0_xix_e(addr);
		case 0xc4: return set_0_xix_h(addr);
		case 0xc5: return set_0_xix_l(addr);
		case 0xc6: return set_0_xix(addr);
		case 0xc7: return set_0_xix_a(addr);
		case 0xc8: return set_1_xix_b(addr);
		case 0xc9: return set_1_xix_c(addr);
		case 0xca: return set_1_xix_d(addr);
		case 0xcb: return set_1_xix_e(addr);
		case 0xcc: return set_1_xix_h(addr);
		case 0xcd: return set_1_xix_l(addr);
		case 0xce: return set_1_xix(addr);
		case 0xcf: return set_1_xix_a(addr);
		case 0xd0: return set_2_xix_b(addr);
		case 0xd1: return set_2_xix_c(addr);
		case 0xd2: return set_2_xix_d(addr);
		case 0xd3: return set_2_xix_e(addr);
		case 0xd4: return set_2_xix_h(addr);
		case 0xd5: return set_2_xix_l(addr);
		case 0xd6: return set_2_xix(addr);
		case 0xd7: return set_2_xix_a(addr);
		case 0xd8: return set_3_xix_b(addr);
		case 0xd9: return set_3_xix_c(addr);
		case 0xda: return set_3_xix_d(addr);
		case 0xdb: return set_3_xix_e(addr);
		case 0xdc: return set_3_xix_h(addr);
		case 0xdd: return set_3_xix_l(addr);
		case 0xde: return set_3_xix(addr);
		case 0xdf: return set_3_xix_a(addr);
		case 0xe0: return set_4_xix_b(addr);
		case 0xe1: return set_4_xix_c(addr);
		case 0xe2: return set_4_xix_d(addr);
		case 0xe3: return set_4_xix_e(addr);
		case 0xe4: return set_4_xix_h(addr);
		case 0xe5: return set_4_xix_l(addr);
		case 0xe6: return set_4_xix(addr);
		case 0xe7: return set_4_xix_a(addr);
		case 0xe8: return set_5_xix_b(addr);
		case 0xe9: return set_5_xix_c(addr);
		case 0xea: return set_5_xix_d(addr);
		case 0xeb: return set_5_xix_e(addr);
		case 0xec: return set_5_xix_h(addr);
		case 0xed: return set_5_xix_l(addr);
		case 0xee: return set_5_xix(addr);
		case 0xef: return set_5_xix_a(addr);
		case 0xf0: return set_6_xix_b(addr);
		case 0xf1: return set_6_xix_c(addr);
		case 0xf2: return set_6_xix_d(addr);
		case 0xf3: return set_6_xix_e(addr);
		case 0xf4: return set_6_xix_h(addr);
		case 0xf5: return set_6_xix_l(addr);
		case 0xf6: return set_6_xix(addr);
		case 0xf7: return set_6_xix_a(addr);
		case 0xf8: return set_7_xix_b(addr);
		case 0xf9: return set_7_xix_c(addr);
		case 0xfa: return set_7_xix_d(addr);
		case 0xfb: return set_7_xix_e(addr);
		case 0xfc: return set_7_xix_h(addr);
		case 0xfd: return set_7_xix_l(addr);
		case 0xfe: return set_7_xix(addr);
		case 0xff: return set_7_xix_a(addr);
	}
	assert(false); return 0;
}
template <class T> int CPUCore<T>::dd_cb() { return nn_cb(R.getIX()); }
template <class T> int CPUCore<T>::fd_cb() { return nn_cb(R.getIY()); }

template <class T> int CPUCore<T>::cb()
{
	byte opcode = RDMEM_OPCODE(T::CC_PREFIX);
	M1Cycle();
	switch (opcode) {
		case 0x00: return rlc_b();
		case 0x01: return rlc_c();
		case 0x02: return rlc_d();
		case 0x03: return rlc_e();
		case 0x04: return rlc_h();
		case 0x05: return rlc_l();
		case 0x06: return rlc_xhl();
		case 0x07: return rlc_a();
		case 0x08: return rrc_b();
		case 0x09: return rrc_c();
		case 0x0a: return rrc_d();
		case 0x0b: return rrc_e();
		case 0x0c: return rrc_h();
		case 0x0d: return rrc_l();
		case 0x0e: return rrc_xhl();
		case 0x0f: return rrc_a();
		case 0x10: return rl_b();
		case 0x11: return rl_c();
		case 0x12: return rl_d();
		case 0x13: return rl_e();
		case 0x14: return rl_h();
		case 0x15: return rl_l();
		case 0x16: return rl_xhl();
		case 0x17: return rl_a();
		case 0x18: return rr_b();
		case 0x19: return rr_c();
		case 0x1a: return rr_d();
		case 0x1b: return rr_e();
		case 0x1c: return rr_h();
		case 0x1d: return rr_l();
		case 0x1e: return rr_xhl();
		case 0x1f: return rr_a();
		case 0x20: return sla_b();
		case 0x21: return sla_c();
		case 0x22: return sla_d();
		case 0x23: return sla_e();
		case 0x24: return sla_h();
		case 0x25: return sla_l();
		case 0x26: return sla_xhl();
		case 0x27: return sla_a();
		case 0x28: return sra_b();
		case 0x29: return sra_c();
		case 0x2a: return sra_d();
		case 0x2b: return sra_e();
		case 0x2c: return sra_h();
		case 0x2d: return sra_l();
		case 0x2e: return sra_xhl();
		case 0x2f: return sra_a();
		case 0x30: return sll_b();
		case 0x31: return sll_c();
		case 0x32: return sll_d();
		case 0x33: return sll_e();
		case 0x34: return sll_h();
		case 0x35: return sll_l();
		case 0x36: return sll_xhl();
		case 0x37: return sll_a();
		case 0x38: return srl_b();
		case 0x39: return srl_c();
		case 0x3a: return srl_d();
		case 0x3b: return srl_e();
		case 0x3c: return srl_h();
		case 0x3d: return srl_l();
		case 0x3e: return srl_xhl();
		case 0x3f: return srl_a();

		case 0x40: return bit_0_b();
		case 0x41: return bit_0_c();
		case 0x42: return bit_0_d();
		case 0x43: return bit_0_e();
		case 0x44: return bit_0_h();
		case 0x45: return bit_0_l();
		case 0x46: return bit_0_xhl();
		case 0x47: return bit_0_a();
		case 0x48: return bit_1_b();
		case 0x49: return bit_1_c();
		case 0x4a: return bit_1_d();
		case 0x4b: return bit_1_e();
		case 0x4c: return bit_1_h();
		case 0x4d: return bit_1_l();
		case 0x4e: return bit_1_xhl();
		case 0x4f: return bit_1_a();
		case 0x50: return bit_2_b();
		case 0x51: return bit_2_c();
		case 0x52: return bit_2_d();
		case 0x53: return bit_2_e();
		case 0x54: return bit_2_h();
		case 0x55: return bit_2_l();
		case 0x56: return bit_2_xhl();
		case 0x57: return bit_2_a();
		case 0x58: return bit_3_b();
		case 0x59: return bit_3_c();
		case 0x5a: return bit_3_d();
		case 0x5b: return bit_3_e();
		case 0x5c: return bit_3_h();
		case 0x5d: return bit_3_l();
		case 0x5e: return bit_3_xhl();
		case 0x5f: return bit_3_a();
		case 0x60: return bit_4_b();
		case 0x61: return bit_4_c();
		case 0x62: return bit_4_d();
		case 0x63: return bit_4_e();
		case 0x64: return bit_4_h();
		case 0x65: return bit_4_l();
		case 0x66: return bit_4_xhl();
		case 0x67: return bit_4_a();
		case 0x68: return bit_5_b();
		case 0x69: return bit_5_c();
		case 0x6a: return bit_5_d();
		case 0x6b: return bit_5_e();
		case 0x6c: return bit_5_h();
		case 0x6d: return bit_5_l();
		case 0x6e: return bit_5_xhl();
		case 0x6f: return bit_5_a();
		case 0x70: return bit_6_b();
		case 0x71: return bit_6_c();
		case 0x72: return bit_6_d();
		case 0x73: return bit_6_e();
		case 0x74: return bit_6_h();
		case 0x75: return bit_6_l();
		case 0x76: return bit_6_xhl();
		case 0x77: return bit_6_a();
		case 0x78: return bit_7_b();
		case 0x79: return bit_7_c();
		case 0x7a: return bit_7_d();
		case 0x7b: return bit_7_e();
		case 0x7c: return bit_7_h();
		case 0x7d: return bit_7_l();
		case 0x7e: return bit_7_xhl();
		case 0x7f: return bit_7_a();

		case 0x80: return res_0_b();
		case 0x81: return res_0_c();
		case 0x82: return res_0_d();
		case 0x83: return res_0_e();
		case 0x84: return res_0_h();
		case 0x85: return res_0_l();
		case 0x86: return res_0_xhl();
		case 0x87: return res_0_a();
		case 0x88: return res_1_b();
		case 0x89: return res_1_c();
		case 0x8a: return res_1_d();
		case 0x8b: return res_1_e();
		case 0x8c: return res_1_h();
		case 0x8d: return res_1_l();
		case 0x8e: return res_1_xhl();
		case 0x8f: return res_1_a();
		case 0x90: return res_2_b();
		case 0x91: return res_2_c();
		case 0x92: return res_2_d();
		case 0x93: return res_2_e();
		case 0x94: return res_2_h();
		case 0x95: return res_2_l();
		case 0x96: return res_2_xhl();
		case 0x97: return res_2_a();
		case 0x98: return res_3_b();
		case 0x99: return res_3_c();
		case 0x9a: return res_3_d();
		case 0x9b: return res_3_e();
		case 0x9c: return res_3_h();
		case 0x9d: return res_3_l();
		case 0x9e: return res_3_xhl();
		case 0x9f: return res_3_a();
		case 0xa0: return res_4_b();
		case 0xa1: return res_4_c();
		case 0xa2: return res_4_d();
		case 0xa3: return res_4_e();
		case 0xa4: return res_4_h();
		case 0xa5: return res_4_l();
		case 0xa6: return res_4_xhl();
		case 0xa7: return res_4_a();
		case 0xa8: return res_5_b();
		case 0xa9: return res_5_c();
		case 0xaa: return res_5_d();
		case 0xab: return res_5_e();
		case 0xac: return res_5_h();
		case 0xad: return res_5_l();
		case 0xae: return res_5_xhl();
		case 0xaf: return res_5_a();
		case 0xb0: return res_6_b();
		case 0xb1: return res_6_c();
		case 0xb2: return res_6_d();
		case 0xb3: return res_6_e();
		case 0xb4: return res_6_h();
		case 0xb5: return res_6_l();
		case 0xb6: return res_6_xhl();
		case 0xb7: return res_6_a();
		case 0xb8: return res_7_b();
		case 0xb9: return res_7_c();
		case 0xba: return res_7_d();
		case 0xbb: return res_7_e();
		case 0xbc: return res_7_h();
		case 0xbd: return res_7_l();
		case 0xbe: return res_7_xhl();
		case 0xbf: return res_7_a();

		case 0xc0: return set_0_b();
		case 0xc1: return set_0_c();
		case 0xc2: return set_0_d();
		case 0xc3: return set_0_e();
		case 0xc4: return set_0_h();
		case 0xc5: return set_0_l();
		case 0xc6: return set_0_xhl();
		case 0xc7: return set_0_a();
		case 0xc8: return set_1_b();
		case 0xc9: return set_1_c();
		case 0xca: return set_1_d();
		case 0xcb: return set_1_e();
		case 0xcc: return set_1_h();
		case 0xcd: return set_1_l();
		case 0xce: return set_1_xhl();
		case 0xcf: return set_1_a();
		case 0xd0: return set_2_b();
		case 0xd1: return set_2_c();
		case 0xd2: return set_2_d();
		case 0xd3: return set_2_e();
		case 0xd4: return set_2_h();
		case 0xd5: return set_2_l();
		case 0xd6: return set_2_xhl();
		case 0xd7: return set_2_a();
		case 0xd8: return set_3_b();
		case 0xd9: return set_3_c();
		case 0xda: return set_3_d();
		case 0xdb: return set_3_e();
		case 0xdc: return set_3_h();
		case 0xdd: return set_3_l();
		case 0xde: return set_3_xhl();
		case 0xdf: return set_3_a();
		case 0xe0: return set_4_b();
		case 0xe1: return set_4_c();
		case 0xe2: return set_4_d();
		case 0xe3: return set_4_e();
		case 0xe4: return set_4_h();
		case 0xe5: return set_4_l();
		case 0xe6: return set_4_xhl();
		case 0xe7: return set_4_a();
		case 0xe8: return set_5_b();
		case 0xe9: return set_5_c();
		case 0xea: return set_5_d();
		case 0xeb: return set_5_e();
		case 0xec: return set_5_h();
		case 0xed: return set_5_l();
		case 0xee: return set_5_xhl();
		case 0xef: return set_5_a();
		case 0xf0: return set_6_b();
		case 0xf1: return set_6_c();
		case 0xf2: return set_6_d();
		case 0xf3: return set_6_e();
		case 0xf4: return set_6_h();
		case 0xf5: return set_6_l();
		case 0xf6: return set_6_xhl();
		case 0xf7: return set_6_a();
		case 0xf8: return set_7_b();
		case 0xf9: return set_7_c();
		case 0xfa: return set_7_d();
		case 0xfb: return set_7_e();
		case 0xfc: return set_7_h();
		case 0xfd: return set_7_l();
		case 0xfe: return set_7_xhl();
		case 0xff: return set_7_a();
	}
	assert(false); return 0;
}

template <class T> int CPUCore<T>::ed()
{
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

		case 0x40: return in_b_c();
		case 0x48: return in_c_c();
		case 0x50: return in_d_c();
		case 0x58: return in_e_c();
		case 0x60: return in_h_c();
		case 0x68: return in_l_c();
		case 0x70: return in_0_c();
		case 0x78: return in_a_c();

		case 0x41: return out_c_b();
		case 0x49: return out_c_c();
		case 0x51: return out_c_d();
		case 0x59: return out_c_e();
		case 0x61: return out_c_h();
		case 0x69: return out_c_l();
		case 0x71: return out_c_0();
		case 0x79: return out_c_a();

		case 0x42: return sbc_hl_bc();
		case 0x52: return sbc_hl_de();
		case 0x62: return sbc_hl_hl();
		case 0x72: return sbc_hl_sp();

		case 0x4a: return adc_hl_bc();
		case 0x5a: return adc_hl_de();
		case 0x6a: return adc_hl_hl();
		case 0x7a: return adc_hl_sp();

		case 0x43: return ld_xword_bc2();
		case 0x53: return ld_xword_de2();
		case 0x63: return ld_xword_hl2();
		case 0x73: return ld_xword_sp2();

		case 0x4b: return ld_bc_xword2();
		case 0x5b: return ld_de_xword2();
		case 0x6b: return ld_hl_xword2();
		case 0x7b: return ld_sp_xword2();

		case 0x47: return ld_i_a();
		case 0x4f: return ld_r_a();
		case 0x57: return ld_a_i();
		case 0x5f: return ld_a_r();

		case 0x67: return rrd();
		case 0x6f: return rld();

		case 0x45: case 0x4d: case 0x55: case 0x5d:
		case 0x65: case 0x6d: case 0x75: case 0x7d:
			return retn();
		case 0x46: case 0x4e: case 0x66: case 0x6e:
			return im_0();
		case 0x56: case 0x76:
			return im_1();
		case 0x5e: case 0x7e:
			return im_2();
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

		case 0xc1: return T::hasMul() ? mulub_a_b()   : nop();
		case 0xc3: return T::hasMul() ? muluw_hl_bc() : nop();
		case 0xc9: return T::hasMul() ? mulub_a_c()   : nop();
		case 0xd1: return T::hasMul() ? mulub_a_d()   : nop();
		case 0xd9: return T::hasMul() ? mulub_a_e()   : nop();
		case 0xf3: return T::hasMul() ? muluw_hl_sp() : nop();
	}
	assert(false); return 0;
}

// TODO dd2
template <class T> int CPUCore<T>::dd()
{
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

		case 0x09: return add_ix_bc();
		case 0x19: return add_ix_de();
		case 0x21: return ld_ix_word();
		case 0x22: return ld_xword_ix();
		case 0x23: return inc_ix();
		case 0x24: return inc_ixh();
		case 0x25: return dec_ixh();
		case 0x26: return ld_ixh_byte();
		case 0x29: return add_ix_ix();
		case 0x2a: return ld_ix_xword();
		case 0x2b: return dec_ix();
		case 0x2c: return inc_ixl();
		case 0x2d: return dec_ixl();
		case 0x2e: return ld_ixl_byte();
		case 0x34: return inc_xix();
		case 0x35: return dec_xix();
		case 0x36: return ld_xix_byte();
		case 0x39: return add_ix_sp();

		case 0x44: return ld_b_ixh();
		case 0x45: return ld_b_ixl();
		case 0x46: return ld_b_xix();
		case 0x4c: return ld_c_ixh();
		case 0x4d: return ld_c_ixl();
		case 0x4e: return ld_c_xix();
		case 0x54: return ld_d_ixh();
		case 0x55: return ld_d_ixl();
		case 0x56: return ld_d_xix();
		case 0x5c: return ld_e_ixh();
		case 0x5d: return ld_e_ixl();
		case 0x5e: return ld_e_xix();
		case 0x60: return ld_ixh_b();
		case 0x61: return ld_ixh_c();
		case 0x62: return ld_ixh_d();
		case 0x63: return ld_ixh_e();
		case 0x65: return ld_ixh_ixl();
		case 0x66: return ld_h_xix();
		case 0x67: return ld_ixh_a();
		case 0x68: return ld_ixl_b();
		case 0x69: return ld_ixl_c();
		case 0x6a: return ld_ixl_d();
		case 0x6b: return ld_ixl_e();
		case 0x6c: return ld_ixl_ixh();
		case 0x6e: return ld_l_xix();
		case 0x6f: return ld_ixl_a();
		case 0x70: return ld_xix_b();
		case 0x71: return ld_xix_c();
		case 0x72: return ld_xix_d();
		case 0x73: return ld_xix_e();
		case 0x74: return ld_xix_h();
		case 0x75: return ld_xix_l();
		case 0x77: return ld_xix_a();
		case 0x7c: return ld_a_ixh();
		case 0x7d: return ld_a_ixl();
		case 0x7e: return ld_a_xix();

		case 0x84: return add_a_ixh();
		case 0x85: return add_a_ixl();
		case 0x86: return add_a_xix();
		case 0x8c: return adc_a_ixh();
		case 0x8d: return adc_a_ixl();
		case 0x8e: return adc_a_xix();
		case 0x94: return sub_ixh();
		case 0x95: return sub_ixl();
		case 0x96: return sub_xix();
		case 0x9c: return sbc_a_ixh();
		case 0x9d: return sbc_a_ixl();
		case 0x9e: return sbc_a_xix();
		case 0xa4: return and_ixh();
		case 0xa5: return and_ixl();
		case 0xa6: return and_xix();
		case 0xac: return xor_ixh();
		case 0xad: return xor_ixl();
		case 0xae: return xor_xix();
		case 0xb4: return or_ixh();
		case 0xb5: return or_ixl();
		case 0xb6: return or_xix();
		case 0xbc: return cp_ixh();
		case 0xbd: return cp_ixl();
		case 0xbe: return cp_xix();

		case 0xcb: return dd_cb();
		case 0xdd: return dd();
		case 0xe1: return pop_ix();
		case 0xe3: return ex_xsp_ix();
		case 0xe5: return push_ix();
		case 0xe9: return jp_ix();
		case 0xf9: return ld_sp_ix();
		case 0xfd: return fd();
	}
	assert(false); return 0;
}

// TODO fd2
template <class T> int CPUCore<T>::fd()
{
	T::add(T::CC_DD);
	byte opcode = RDMEM_OPCODE(T::CC_MAIN);
	M1Cycle();
	switch (opcode) {
		case 0x00: // nop
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
		case 0x64: // ld_iyh_iyh(); == nop
		case 0x6d: // ld_iyl_iyl(); == nop
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

		case 0x09: return add_iy_bc();
		case 0x19: return add_iy_de();
		case 0x21: return ld_iy_word();
		case 0x22: return ld_xword_iy();
		case 0x23: return inc_iy();
		case 0x24: return inc_iyh();
		case 0x25: return dec_iyh();
		case 0x26: return ld_iyh_byte();
		case 0x29: return add_iy_iy();
		case 0x2a: return ld_iy_xword();
		case 0x2b: return dec_iy();
		case 0x2c: return inc_iyl();
		case 0x2d: return dec_iyl();
		case 0x2e: return ld_iyl_byte();
		case 0x34: return inc_xiy();
		case 0x35: return dec_xiy();
		case 0x36: return ld_xiy_byte();
		case 0x39: return add_iy_sp();

		case 0x44: return ld_b_iyh();
		case 0x45: return ld_b_iyl();
		case 0x46: return ld_b_xiy();
		case 0x4c: return ld_c_iyh();
		case 0x4d: return ld_c_iyl();
		case 0x4e: return ld_c_xiy();
		case 0x54: return ld_d_iyh();
		case 0x55: return ld_d_iyl();
		case 0x56: return ld_d_xiy();
		case 0x5c: return ld_e_iyh();
		case 0x5d: return ld_e_iyl();
		case 0x5e: return ld_e_xiy();
		case 0x60: return ld_iyh_b();
		case 0x61: return ld_iyh_c();
		case 0x62: return ld_iyh_d();
		case 0x63: return ld_iyh_e();
		case 0x65: return ld_iyh_iyl();
		case 0x66: return ld_h_xiy();
		case 0x67: return ld_iyh_a();
		case 0x68: return ld_iyl_b();
		case 0x69: return ld_iyl_c();
		case 0x6a: return ld_iyl_d();
		case 0x6b: return ld_iyl_e();
		case 0x6c: return ld_iyl_iyh();
		case 0x6e: return ld_l_xiy();
		case 0x6f: return ld_iyl_a();
		case 0x70: return ld_xiy_b();
		case 0x71: return ld_xiy_c();
		case 0x72: return ld_xiy_d();
		case 0x73: return ld_xiy_e();
		case 0x74: return ld_xiy_h();
		case 0x75: return ld_xiy_l();
		case 0x77: return ld_xiy_a();
		case 0x7c: return ld_a_iyh();
		case 0x7d: return ld_a_iyl();
		case 0x7e: return ld_a_xiy();

		case 0x84: return add_a_iyh();
		case 0x85: return add_a_iyl();
		case 0x86: return add_a_xiy();
		case 0x8c: return adc_a_iyh();
		case 0x8d: return adc_a_iyl();
		case 0x8e: return adc_a_xiy();
		case 0x94: return sub_iyh();
		case 0x95: return sub_iyl();
		case 0x96: return sub_xiy();
		case 0x9c: return sbc_a_iyh();
		case 0x9d: return sbc_a_iyl();
		case 0x9e: return sbc_a_xiy();
		case 0xa4: return and_iyh();
		case 0xa5: return and_iyl();
		case 0xa6: return and_xiy();
		case 0xac: return xor_iyh();
		case 0xad: return xor_iyl();
		case 0xae: return xor_xiy();
		case 0xb4: return or_iyh();
		case 0xb5: return or_iyl();
		case 0xb6: return or_xiy();
		case 0xbc: return cp_iyh();
		case 0xbd: return cp_iyl();
		case 0xbe: return cp_xiy();

		case 0xcb: return fd_cb();
		case 0xdd: return dd();
		case 0xe1: return pop_iy();
		case 0xe3: return ex_xsp_iy();
		case 0xe5: return push_iy();
		case 0xe9: return jp_iy();
		case 0xf9: return ld_sp_iy();
		case 0xfd: return fd();
	}
	assert(false); return 0;
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

} // namespace openmsx

