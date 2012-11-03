// $Id$

// MEMORY EMULATION
// ----------------
//
// Memory access emulation is a very important part of the CPU emulation.
// Because they happen so frequently they really need to be executed as fast as
// possible otherwise they will completely bring down the speed of the CPU
// emulation.
//
// A very fast way to emulate memory accesses is by simply reading/writing to a
// 64kb array (for a 16-bit address space). Unfortunately this doesn't allow
// for memory mapped IO (MMIO). These are memory regions where read/writes
// trigger side effects, so where we need to execute device-specific code on
// read or writes. An alternative that does work with MMIO is for every access
// execute a virtual method call, (this is the approach taken by most current
// MSX emulators). Unfortunately this is also a lot slower.
//
// It is possible to combine the speed of array accesses with the flexibility
// of virtual methods. In openMSX it's implemened as follows: the 64kb address
// space is divided in 256 regions of 256 bytes (called cacheLines in the code
// below). For each such region we store a pointer, if this pointer is nullptr
// then we have to use the slow way (=virtual method call). If it is not nullptr,
// the pointer points to a block of memory that can be directly accessed. In
// some contexts accesses via the pointer are known as backdoor accesses while
// the accesses directly to the device are known as frontdoor accesses.
//
// We keep different pointers for read and write accesses. This allows to also
// implement ROMs efficiently: read is handled as regular RAM, but writes end
// up in some dummy memory region. This region is called 'unmappedWrite' in the
// code. There is also a special region 'unmappedRead', this region is filled
// with 0xFF and can be used to model (parts of) a device that don't react to
// reads (so reads return 0xFF).
//
// Because of bankswitching (the MSX slot select mechanism, but also e.g.
// MegaROM backswitching) the memory map as seen by the Z80 is not static. This
// means that the cacheLine pointers also need to change during runtime. To
// solve this we made the bankswitch code also responsible for invalidating the
// cacheLines of the switched region. These pointers are filled-in again in a
// lazy way: the first read or write to a cache line will first get this
// pointer (see getReadCacheLine() and getWriteCacheLine() in the code below),
// from then on this pointer is used for all further accesses to this region,
// until the cache is invalidated again.
//
//
// INSTRUCTION EMULATION
// ---------------------
//
// UPDATE: the 'threaded interpreter model' is not enabled by default
//         main reason is the huge memory requirement while compiling
//         and that it doesn't work on non-gcc compilers
//
// The current implementation is based on a 'threaded interpreter model'. In
// the text below I'll call the older implementation the 'traditional
// interpreter model'. From a very high level these two models look like this:
//
//     Traditional model:
//         while (!needExit()) {
//             byte opcode = fetch(PC++);
//             switch (opcode) {
//                 case 0x00: nop(); break;
//                 case 0x01: ld_bc_nn(); break;
//                 ...
//             }
//         }
//
//     Threaded model:
//         byte opcode = fetch(PC++);   //
//         goto *(table[opcode]);       // fetch-and-dispatch
//         // note: the goto * syntax is a gcc extension called computed-gotos
//
//         op00: nop();      if (!needExit()) [fetch-and-dispatch];
//         op01: ld_bc_nn(); if (!needExit()) [fetch-and-dispatch];
//         ...
//
// In the first model there is a central place in the code that fetches (the
// first byte of) the instruction and based on this byte jumps to the
// appropriate routine. In the second model, this fetch-and-dispatch logic is
// duplicated at the end of each instruction.
//
// Typically the 'dispatch' part in above paragraph is implemented (either by
// the compiler or manually using computed goto's) via a jump table. Thus on
// assembler level via an indirect jump. For the host CPU it's hard to predict
// the destination address of such an indirect jump, certainly if there's only
// one such jump for all dispatching (the traditional model). If each
// instruction has its own indirect jump instruction (the threaded model), it
// becomes a bit easier, because often one particular z80 instructions is
// followed by a specific other z80 instruction (or one from a small subset).
// For example a z80 'cp' instruction is most likely followed by a 'conditional
// jump' z80 instruction. Modern CPUs are quite sensitive to
// branch-(mis)predictions, so the above optimization helps quite a lot. I
// measured a speedup of more than 10%!
//
// There is another advantage to the threaded model. Because also the
// needExit() test is duplicated for each instruction, it becomes possible to
// tweak it for individual instructions. But first let me explain this
// exit-test in more detail.
//
// These are the main reasons why the emulator should stop emulating CPU
// instructions:
//  1) When other devices than the CPU must be emulated (e.g. video frame
//     rendering). In openMSX this is handled by the Scheduler class and
//     actually we don't exit the CPU loop (anymore) for this. Instead we
//     simply execute the device code as a subroutine. Each time right before
//     we access an IO port or do a frontdoor memory access, there is a check
//     whether we should emulate device code (search for schedule() in the code
//     below).
//  2) To keep the inner CPU loop as fast as possible we don't check for IRQ,
//     NMI or HALT status in this loop. Instead this condition is checked only
//     once at the beginning outside of the loop (if there wasn't a pending IRQ
//     on the first instruction there also won't be one on the second
//     instruction, if all we did was emulating cpu instructions). Now when one
//     of these conditions changes, we must exit the inner loop and re-evaluate
//     them. For example after an EI instruction we must check the IRQ status
//     again.
//  3) Various reasons like:
//      * Z80/R800 switch
//      * executing a Tcl command (could be a cpu-register debug read)
//      * exit the emulator
//  4) 'once-in-a-while': To avoid threading problems and race conditions,
//     several threads in openMSX only 'schedule' work that will later be
//     executed by the main emulation thread. The main thread checks for such
//     task outside of the cpu emulation loop. So once-in-a-while we need to
//     exit the loop. The exact timing doesn't matter here because anyway the
//     relative timing between threads is undefined.
// So for 1) we don't need to do anything (we don't actually exit). For 2) and
// 3) we need the exit the loop as soon as possible (right after the current
// instruction is finished). For 4) it's OK to exit 'eventually' (a few hundred
// z80 instructions late is still OK).
//
// Condition 2) is implemented with the 'slowInstructions' mechanism. Condition
// 3) via exitCPULoopSync() (may only get called by the main emulation thread)
// and condition 4) is implemented via exitCPULoopAsync() (can be called from
// any thread).
//
// Now back to the exit-test optimization: in the threaded model each
// instruction ends with:
//
//     if (needExit()) return
//     byte opcode = fetch(PC++);
//     goto *(table[opcode]);
//
// And if we look in more detail at fetch():
//
//     if (canDoBackdoor(addr)) {
//         doBackdoorAccess(addr);
//     } else {
//         doFrontdoorAccess(addr);
//     }
//
// So there are in fact two checks per instruction. This can be reduced to only
// one check with the following trick:
//
// !!!WRONG!!!
// In the past we optimized this to only check canDoBackdoor() (and make sure
// canDoBackdoor() returned false when needExit() would return true). This
// worked rather well, except for one case: when we exit the CPU loop we also
// check for pending Syncronization points. It is possible such a SyncPoint
// raises the IRQ line. So it is important to check for exit after every
// instruction, otherwise we would enter the IRQ routine a couple of
// instructions too late.

#if (__GNUC__ == 4) && (__GNUC_MINOR__ == 0) \
 && defined(__i386__) && defined(NDEBUG)
#warning Applying workaround for GCC 4.0 internal compiler error.
// When compiling on Mac OS X for x86 in "opt" flavour, GCC crashes.
// The crash does not occur when compiling for PPC.
// The crash can be avoided by enabling asserts ("devel" flavour) or disabling
// optimization completely (-O0). However, with some strategically placed calls
// to abort() we can get the same effect with less impact on the generated
// code. The define below enables those calls.
#define WORK_AROUND_GCC40_SEGFAULT
#include <cstdlib> // for abort()
#include "likely.hh" // for unlikely()
#endif

#include "CPUCore.hh"
#include "MSXCPUInterface.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "CliComm.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "TclCallback.hh"
#include "Dasm.hh"
#include "Z80.hh"
#include "R800.hh"
#include "Thread.hh"
#include "endian.hh"
#include "likely.hh"
#include "inline.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <iomanip>
#include <iostream>
#include <cassert>
#include <cstring>


//
// #define USE_COMPUTED_GOTO
//
// Computed goto's are not enabled by default:
// - Computed goto's are a gcc extension, it's not part of the official c++
//   standard. So this will only work if you use gcc as your compiler (it
//   won't work with visual c++ for example)
// - This is only beneficial on CPUs with branch prediction for indirect jumps
//   and a reasonable amout of cache. For example it is very benefical for a
//   intel core2 cpu (10% faster), but not for a ARM920 (a few percent slower)
// - Compiling src/cpu/CPUCore.cc with computed goto's enabled is very demanding
//   on the compiler. On older gcc versions it requires up to 1.5GB of memory.
//   But even on more recent gcc versions it still requires around 700MB.
//
// Probably the easiest way to enable this, is to pass the -DUSE_COMPUTED_GOTO
// flag to the compiler. This is for example done in the super-opt flavour.
// See build/flavour-super-opt.mk


using std::string;

namespace openmsx {

typedef signed char offset;

// Global variable, because it should be shared between Z80 and R800.
// It must not be shared between the CPUs of different MSX machines, but
// the (logical) lifetime of this variable cannot overlap between execution
// of two MSX machines.
static word start_pc;

// conditions
struct CondC  { bool operator()(byte f) const { return  (f & CPU::C_FLAG) != 0; } };
struct CondNC { bool operator()(byte f) const { return !(f & CPU::C_FLAG); } };
struct CondZ  { bool operator()(byte f) const { return  (f & CPU::Z_FLAG) != 0; } };
struct CondNZ { bool operator()(byte f) const { return !(f & CPU::Z_FLAG); } };
struct CondM  { bool operator()(byte f) const { return  (f & CPU::S_FLAG) != 0; } };
struct CondP  { bool operator()(byte f) const { return !(f & CPU::S_FLAG); } };
struct CondPE { bool operator()(byte f) const { return  (f & CPU::V_FLAG) != 0; } };
struct CondPO { bool operator()(byte f) const { return !(f & CPU::V_FLAG); } };
struct CondTrue { bool operator()(byte) const { return true; } };

// This function only exists as a workaround for a bug in g++-4.2.x, see
//   http://gcc.gnu.org/bugzilla/show_bug.cgi?id=34336
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
		const BooleanSetting& traceSetting_,
		TclCallback& diHaltCallback_, EmuTime::param time)
	: T(time, motherboard_.getScheduler())
	, CPU(T::isR800())
	, motherboard(motherboard_)
	, scheduler(motherboard.getScheduler())
	, interface(nullptr)
	, traceSetting(traceSetting_)
	, diHaltCallback(diHaltCallback_)
	, IRQStatus(motherboard.getDebugger(), name + ".pendingIRQ",
	            "Non-zero if there are pending IRQs (thus CPU would enter "
	            "interrupt routine in EI mode).",
	            0)
	, IRQAccept(motherboard.getDebugger(), name + ".acceptIRQ",
	            "This probe is only useful to set a breakpoint on (the value "
		    "return by read is meaningless). The breakpoint gets triggered "
		    "right after the CPU accepted an IRQ.")
	, freqLocked(createFreqLockedSetting(
		motherboard.getCommandController(), name))
	, freqValue(new IntegerSetting(motherboard.getCommandController(),
	        name + "_freq",
	        "custom " + name + " frequency (only valid when unlocked)",
	        T::CLOCK_FREQ, 1000000, 1000000000))
	, freq(T::CLOCK_FREQ)
	, NMIStatus(0)
	, nmiEdge(false)
	, exitLoop(false)
	, isTurboR(motherboard.isTurboR())
{
	doSetFreq();
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

template <class T> void CPUCore<T>::warp(EmuTime::param time)
{
	assert(T::getTimeFast() <= time);
	T::setTime(time);
}

template <class T> EmuTime::param CPUCore<T>::getCurrentTime() const
{
	return T::getTime();
}

template <class T> void CPUCore<T>::invalidateMemCache(unsigned start, unsigned size)
{
	unsigned first = start / CacheLine::SIZE;
	unsigned num = (size + CacheLine::SIZE - 1) / CacheLine::SIZE;
	memset(&readCacheLine  [first], 0, num * sizeof(byte*)); // nullptr
	memset(&writeCacheLine [first], 0, num * sizeof(byte*)); //
	memset(&readCacheTried [first], 0, num * sizeof(bool));  // FALSE
	memset(&writeCacheTried[first], 0, num * sizeof(bool));  //
}

template <class T> void CPUCore<T>::doReset(EmuTime::param time)
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
	R.clearNextAfter();
	R.copyNextAfter();
	R.setIFF1(false);
	R.setIFF2(false);
	R.setHALT(false);
	R.setExtHALT(false);
	R.setIM(0);
	R.setI(0x00);
	R.setR(0x00);
	T::setMemPtr(0xFFFF);
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
	IRQStatus = IRQStatus + 1;
}

template <class T> void CPUCore<T>::lowerIRQ()
{
	IRQStatus = IRQStatus - 1;
	assert(IRQStatus >= 0);
}

template <class T> void CPUCore<T>::raiseNMI()
{
	// NMIs are currently disabled, they are anyway not used in MSX and
	// not having to check for them allows to emulate slightly faster
	UNREACHABLE;
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

template <class T> bool CPUCore<T>::isM1Cycle(unsigned address) const
{
	// PC was already increased, so decrease again
	return address == ((R.getPC() - 1) & 0xFFFF);
}

template <class T> void CPUCore<T>::wait(EmuTime::param time)
{
	assert(time >= getCurrentTime());
	scheduler.schedule(time);
	T::advanceTime(time);
}

template <class T> void CPUCore<T>::waitCycles(unsigned cycles)
{
	T::add(cycles);
}

template <class T> void CPUCore<T>::setNextSyncPoint(EmuTime::param time)
{
	T::setLimit(time);
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
	const std::vector<TclObject>& tokens,
	TclObject& result) const
{
	word address = (tokens.size() < 3) ? R.getPC() : tokens[2].getInt();
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
		doSetFreq();
	} else if (&setting == freqValue.get()) {
		doSetFreq();
	} else {
		UNREACHABLE;
	}
}

template <class T> void CPUCore<T>::setFreq(unsigned freq_)
{
	freq = freq_;
	doSetFreq();
}

template <class T> void CPUCore<T>::doSetFreq()
{
	if (freqLocked->getValue()) {
		// locked, use value set via setFreq()
		T::setFreq(freq);
	} else {
		// unlocked, use value set by user
		T::setFreq(freqValue->getValue());
	}
}


template <class T> inline byte CPUCore<T>::READ_PORT(unsigned port, unsigned cc)
{
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	byte result = interface->readIO(port, time);
	// note: no forced page-break after IO
	return result;
}

template <class T> inline void CPUCore<T>::WRITE_PORT(unsigned port, byte value, unsigned cc)
{
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	interface->writeIO(port, value, time);
	// note: no forced page-break after IO
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
	if (likely(line != nullptr)) {
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
		   (line != nullptr))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<PRE_PB, POST_PB>(address);
		T::template POST_WORD<       POST_PB>(address);
		return Endian::read_UA_L16(&line[address]);
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
	if (likely(line != nullptr)) {
		// cached, fast path
		T::template PRE_MEM<PRE_PB, POST_PB>(address);
		T::template POST_MEM<       POST_PB>(address);
		line[address] = value;
	} else {
		WRMEMslow<PRE_PB, POST_PB>(address, value, cc); // not inlined
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
		   (line != nullptr))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<true, true>(address);
		T::template POST_WORD<     true>(address);
		Endian::write_UA_L16(&line[address], value);
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
		   (line != nullptr))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<PRE_PB, POST_PB>(address);
		T::template POST_WORD<       POST_PB>(address);
		Endian::write_UA_L16(&line[address], value);
	} else {
		// slow path, not inline
		WR_WORD_rev_slow<PRE_PB, POST_PB>(address, value, cc);
	}
}


// NMI interrupt
template <class T> inline void CPUCore<T>::nmi()
{
	R.incR(1);
	R.setHALT(false);
	R.setIFF1(false);
	PUSH<T::EE_NMI_1>(R.getPC());
	R.setPC(0x0066);
	T::add(T::CC_NMI);
}

// IM0 interrupt
template <class T> inline void CPUCore<T>::irq0()
{
	// TODO current implementation only works for 1-byte instructions
	//      ok for MSX
	assert(interface->readIRQVector() == 0xFF);
	R.incR(1);
	R.setHALT(false);
	R.setIFF1(false);
	R.setIFF2(false);
	PUSH<T::EE_IRQ0_1>(R.getPC());
	R.setPC(0x0038);
	T::setMemPtr(R.getPC());
	T::add(T::CC_IRQ0);
}

// IM1 interrupt
template <class T> inline void CPUCore<T>::irq1()
{
	R.incR(1);
	R.setHALT(false);
	R.setIFF1(false);
	R.setIFF2(false);
	PUSH<T::EE_IRQ1_1>(R.getPC());
	R.setPC(0x0038);
	T::setMemPtr(R.getPC());
	T::add(T::CC_IRQ1);
}

// IM2 interrupt
template <class T> inline void CPUCore<T>::irq2()
{
	R.incR(1);
	R.setHALT(false);
	R.setIFF1(false);
	R.setIFF2(false);
	PUSH<T::EE_IRQ2_1>(R.getPC());
	unsigned x = interface->readIRQVector() | (R.getI() << 8);
	R.setPC(RD_WORD(x, T::CC_IRQ2_2));
	T::setMemPtr(R.getPC());
	T::add(T::CC_IRQ2);
}

template <class T>
void CPUCore<T>::executeInstructions()
{
	assert(R.isNextAfterClear());

#ifdef USE_COMPUTED_GOTO
	// Addresses of all main-opcode routines,
	// Note that 40/49/53/5B/64/6D/7F is replaced by 00 (ld r,r == nop)
	static void* opcodeTable[256] = {
		&&op00, &&op01, &&op02, &&op03, &&op04, &&op05, &&op06, &&op07,
		&&op08, &&op09, &&op0A, &&op0B, &&op0C, &&op0D, &&op0E, &&op0F,
		&&op10, &&op11, &&op12, &&op13, &&op14, &&op15, &&op16, &&op17,
		&&op18, &&op19, &&op1A, &&op1B, &&op1C, &&op1D, &&op1E, &&op1F,
		&&op20, &&op21, &&op22, &&op23, &&op24, &&op25, &&op26, &&op27,
		&&op28, &&op29, &&op2A, &&op2B, &&op2C, &&op2D, &&op2E, &&op2F,
		&&op30, &&op31, &&op32, &&op33, &&op34, &&op35, &&op36, &&op37,
		&&op38, &&op39, &&op3A, &&op3B, &&op3C, &&op3D, &&op3E, &&op3F,
		&&op00, &&op41, &&op42, &&op43, &&op44, &&op45, &&op46, &&op47,
		&&op48, &&op00, &&op4A, &&op4B, &&op4C, &&op4D, &&op4E, &&op4F,
		&&op50, &&op51, &&op00, &&op53, &&op54, &&op55, &&op56, &&op57,
		&&op58, &&op59, &&op5A, &&op00, &&op5C, &&op5D, &&op5E, &&op5F,
		&&op60, &&op61, &&op62, &&op63, &&op00, &&op65, &&op66, &&op67,
		&&op68, &&op69, &&op6A, &&op6B, &&op6C, &&op00, &&op6E, &&op6F,
		&&op70, &&op71, &&op72, &&op73, &&op74, &&op75, &&op76, &&op77,
		&&op78, &&op79, &&op7A, &&op7B, &&op7C, &&op7D, &&op7E, &&op00,
		&&op80, &&op81, &&op82, &&op83, &&op84, &&op85, &&op86, &&op87,
		&&op88, &&op89, &&op8A, &&op8B, &&op8C, &&op8D, &&op8E, &&op8F,
		&&op90, &&op91, &&op92, &&op93, &&op94, &&op95, &&op96, &&op97,
		&&op98, &&op99, &&op9A, &&op9B, &&op9C, &&op9D, &&op9E, &&op9F,
		&&opA0, &&opA1, &&opA2, &&opA3, &&opA4, &&opA5, &&opA6, &&opA7,
		&&opA8, &&opA9, &&opAA, &&opAB, &&opAC, &&opAD, &&opAE, &&opAF,
		&&opB0, &&opB1, &&opB2, &&opB3, &&opB4, &&opB5, &&opB6, &&opB7,
		&&opB8, &&opB9, &&opBA, &&opBB, &&opBC, &&opBD, &&opBE, &&opBF,
		&&opC0, &&opC1, &&opC2, &&opC3, &&opC4, &&opC5, &&opC6, &&opC7,
		&&opC8, &&opC9, &&opCA, &&opCB, &&opCC, &&opCD, &&opCE, &&opCF,
		&&opD0, &&opD1, &&opD2, &&opD3, &&opD4, &&opD5, &&opD6, &&opD7,
		&&opD8, &&opD9, &&opDA, &&opDB, &&opDC, &&opDD, &&opDE, &&opDF,
		&&opE0, &&opE1, &&opE2, &&opE3, &&opE4, &&opE5, &&opE6, &&opE7,
		&&opE8, &&opE9, &&opEA, &&opEB, &&opEC, &&opED, &&opEE, &&opEF,
		&&opF0, &&opF1, &&opF2, &&opF3, &&opF4, &&opF5, &&opF6, &&opF7,
		&&opF8, &&opF9, &&opFA, &&opFB, &&opFC, &&opFD, &&opFE, &&opFF,
	};

// Check T::limitReached(). If it's OK to continue,
// fetch and execute next instruction.
#define NEXT \
	T::add(c); \
	T::R800Refresh(R); \
	if (likely(!T::limitReached())) { \
		R.incR(1); \
		unsigned address = R.getPC(); \
		const byte* line = readCacheLine[address >> CacheLine::BITS]; \
		if (likely(line != nullptr)) { \
			R.setPC(address + 1); \
			T::template PRE_MEM<false, false>(address); \
			T::template POST_MEM<      false>(address); \
			byte op = line[address]; \
			goto *(opcodeTable[op]); \
		} else { \
			goto fetchSlow; \
		} \
	} \
	return;

// After some instructions we must always exit the CPU loop (ei, halt, retn)
#define NEXT_STOP \
	T::add(c); \
	T::R800Refresh(R); \
	assert(T::limitReached()); \
	return;

#define NEXT_EI \
	T::add(c); \
	/* !! NO T::R800Refresh(R); !! */ \
	assert(T::limitReached()); \
	return;

// Define a label (instead of case in a switch statement)
#define CASE(X) op##X:

#else // USE_COMPUTED_GOTO

#define NEXT \
	T::add(c); \
	T::R800Refresh(R); \
	if (likely(!T::limitReached())) { \
		goto start; \
	} \
	return;

#define NEXT_STOP \
	T::add(c); \
	T::R800Refresh(R); \
	assert(T::limitReached()); \
	return;

#define NEXT_EI \
	T::add(c); \
	/* !! NO T::R800Refresh(R); !! */ \
	assert(T::limitReached()); \
	return;

#define CASE(X) case 0x##X:

#endif // USE_COMPUTED_GOTO

#ifndef USE_COMPUTED_GOTO
start:
#endif
	unsigned ixy; // for dd_cb/fd_cb
	byte opcodeMain = RDMEM_OPCODE(T::CC_MAIN);
	R.incR(1);
#ifdef USE_COMPUTED_GOTO
	goto *(opcodeTable[opcodeMain]);

fetchSlow: {
	unsigned address = R.getPC();
	R.setPC(address + 1);
	byte opcodeSlow = RDMEMslow<false, false>(address, T::CC_MAIN);
	goto *(opcodeTable[opcodeSlow]);
}
#endif

#ifndef USE_COMPUTED_GOTO
switchopcode:
	switch (opcodeMain) {
CASE(40) // ld b,b
CASE(49) // ld c,c
CASE(52) // ld d,d
CASE(5B) // ld e,e
CASE(64) // ld h,h
CASE(6D) // ld l,l
CASE(7F) // ld a,a
#endif
CASE(00) { int c = nop(); NEXT; }
CASE(07) { int c = rlca(); NEXT; }
CASE(0F) { int c = rrca(); NEXT; }
CASE(17) { int c = rla();  NEXT; }
CASE(1F) { int c = rra();  NEXT; }
CASE(08) { int c = ex_af_af(); NEXT; }
CASE(27) { int c = daa(); NEXT; }
CASE(2F) { int c = cpl(); NEXT; }
CASE(37) { int c = scf(); NEXT; }
CASE(3F) { int c = ccf(); NEXT; }
CASE(20) { int c = jr(CondNZ()); NEXT; }
CASE(28) { int c = jr(CondZ ()); NEXT; }
CASE(30) { int c = jr(CondNC()); NEXT; }
CASE(38) { int c = jr(CondC ()); NEXT; }
CASE(18) { int c = jr(CondTrue()); NEXT; }
CASE(10) { int c = djnz(); NEXT; }
CASE(32) { int c = ld_xbyte_a(); NEXT; }
CASE(3A) { int c = ld_a_xbyte(); NEXT; }
CASE(22) { int c = ld_xword_SS<HL,0>(); NEXT; }
CASE(2A) { int c = ld_SS_xword<HL,0>(); NEXT; }
CASE(02) { int c = ld_SS_a<BC>(); NEXT; }
CASE(12) { int c = ld_SS_a<DE>(); NEXT; }
CASE(1A) { int c = ld_a_SS<DE>(); NEXT; }
CASE(0A) { int c = ld_a_SS<BC>(); NEXT; }
CASE(03) { int c = inc_SS<BC,0>(); NEXT; }
CASE(13) { int c = inc_SS<DE,0>(); NEXT; }
CASE(23) { int c = inc_SS<HL,0>(); NEXT; }
CASE(33) { int c = inc_SS<SP,0>(); NEXT; }
CASE(0B) { int c = dec_SS<BC,0>(); NEXT; }
CASE(1B) { int c = dec_SS<DE,0>(); NEXT; }
CASE(2B) { int c = dec_SS<HL,0>(); NEXT; }
CASE(3B) { int c = dec_SS<SP,0>(); NEXT; }
CASE(09) { int c = add_SS_TT<HL,BC,0>(); NEXT; }
CASE(19) { int c = add_SS_TT<HL,DE,0>(); NEXT; }
CASE(29) { int c = add_SS_SS<HL   ,0>(); NEXT; }
CASE(39) { int c = add_SS_TT<HL,SP,0>(); NEXT; }
CASE(01) { int c = ld_SS_word<BC,0>(); NEXT; }
CASE(11) { int c = ld_SS_word<DE,0>(); NEXT; }
CASE(21) { int c = ld_SS_word<HL,0>(); NEXT; }
CASE(31) { int c = ld_SS_word<SP,0>(); NEXT; }
CASE(04) { int c = inc_R<B,0>(); NEXT; }
CASE(0C) { int c = inc_R<C,0>(); NEXT; }
CASE(14) { int c = inc_R<D,0>(); NEXT; }
CASE(1C) { int c = inc_R<E,0>(); NEXT; }
CASE(24) { int c = inc_R<H,0>(); NEXT; }
CASE(2C) { int c = inc_R<L,0>(); NEXT; }
CASE(3C) { int c = inc_R<A,0>(); NEXT; }
CASE(34) { int c = inc_xhl();  NEXT; }
CASE(05) { int c = dec_R<B,0>(); NEXT; }
CASE(0D) { int c = dec_R<C,0>(); NEXT; }
CASE(15) { int c = dec_R<D,0>(); NEXT; }
CASE(1D) { int c = dec_R<E,0>(); NEXT; }
CASE(25) { int c = dec_R<H,0>(); NEXT; }
CASE(2D) { int c = dec_R<L,0>(); NEXT; }
CASE(3D) { int c = dec_R<A,0>(); NEXT; }
CASE(35) { int c = dec_xhl();  NEXT; }
CASE(06) { int c = ld_R_byte<B,0>(); NEXT; }
CASE(0E) { int c = ld_R_byte<C,0>(); NEXT; }
CASE(16) { int c = ld_R_byte<D,0>(); NEXT; }
CASE(1E) { int c = ld_R_byte<E,0>(); NEXT; }
CASE(26) { int c = ld_R_byte<H,0>(); NEXT; }
CASE(2E) { int c = ld_R_byte<L,0>(); NEXT; }
CASE(3E) { int c = ld_R_byte<A,0>(); NEXT; }
CASE(36) { int c = ld_xhl_byte();  NEXT; }

CASE(41) { int c = ld_R_R<B,C,0>(); NEXT; }
CASE(42) { int c = ld_R_R<B,D,0>(); NEXT; }
CASE(43) { int c = ld_R_R<B,E,0>(); NEXT; }
CASE(44) { int c = ld_R_R<B,H,0>(); NEXT; }
CASE(45) { int c = ld_R_R<B,L,0>(); NEXT; }
CASE(47) { int c = ld_R_R<B,A,0>(); NEXT; }
CASE(48) { int c = ld_R_R<C,B,0>(); NEXT; }
CASE(4A) { int c = ld_R_R<C,D,0>(); NEXT; }
CASE(4B) { int c = ld_R_R<C,E,0>(); NEXT; }
CASE(4C) { int c = ld_R_R<C,H,0>(); NEXT; }
CASE(4D) { int c = ld_R_R<C,L,0>(); NEXT; }
CASE(4F) { int c = ld_R_R<C,A,0>(); NEXT; }
CASE(50) { int c = ld_R_R<D,B,0>(); NEXT; }
CASE(51) { int c = ld_R_R<D,C,0>(); NEXT; }
CASE(53) { int c = ld_R_R<D,E,0>(); NEXT; }
CASE(54) { int c = ld_R_R<D,H,0>(); NEXT; }
CASE(55) { int c = ld_R_R<D,L,0>(); NEXT; }
CASE(57) { int c = ld_R_R<D,A,0>(); NEXT; }
CASE(58) { int c = ld_R_R<E,B,0>(); NEXT; }
CASE(59) { int c = ld_R_R<E,C,0>(); NEXT; }
CASE(5A) { int c = ld_R_R<E,D,0>(); NEXT; }
CASE(5C) { int c = ld_R_R<E,H,0>(); NEXT; }
CASE(5D) { int c = ld_R_R<E,L,0>(); NEXT; }
CASE(5F) { int c = ld_R_R<E,A,0>(); NEXT; }
CASE(60) { int c = ld_R_R<H,B,0>(); NEXT; }
CASE(61) { int c = ld_R_R<H,C,0>(); NEXT; }
CASE(62) { int c = ld_R_R<H,D,0>(); NEXT; }
CASE(63) { int c = ld_R_R<H,E,0>(); NEXT; }
CASE(65) { int c = ld_R_R<H,L,0>(); NEXT; }
CASE(67) { int c = ld_R_R<H,A,0>(); NEXT; }
CASE(68) { int c = ld_R_R<L,B,0>(); NEXT; }
CASE(69) { int c = ld_R_R<L,C,0>(); NEXT; }
CASE(6A) { int c = ld_R_R<L,D,0>(); NEXT; }
CASE(6B) { int c = ld_R_R<L,E,0>(); NEXT; }
CASE(6C) { int c = ld_R_R<L,H,0>(); NEXT; }
CASE(6F) { int c = ld_R_R<L,A,0>(); NEXT; }
CASE(78) { int c = ld_R_R<A,B,0>(); NEXT; }
CASE(79) { int c = ld_R_R<A,C,0>(); NEXT; }
CASE(7A) { int c = ld_R_R<A,D,0>(); NEXT; }
CASE(7B) { int c = ld_R_R<A,E,0>(); NEXT; }
CASE(7C) { int c = ld_R_R<A,H,0>(); NEXT; }
CASE(7D) { int c = ld_R_R<A,L,0>(); NEXT; }
CASE(70) { int c = ld_xhl_R<B>(); NEXT; }
CASE(71) { int c = ld_xhl_R<C>(); NEXT; }
CASE(72) { int c = ld_xhl_R<D>(); NEXT; }
CASE(73) { int c = ld_xhl_R<E>(); NEXT; }
CASE(74) { int c = ld_xhl_R<H>(); NEXT; }
CASE(75) { int c = ld_xhl_R<L>(); NEXT; }
CASE(77) { int c = ld_xhl_R<A>(); NEXT; }
CASE(46) { int c = ld_R_xhl<B>(); NEXT; }
CASE(4E) { int c = ld_R_xhl<C>(); NEXT; }
CASE(56) { int c = ld_R_xhl<D>(); NEXT; }
CASE(5E) { int c = ld_R_xhl<E>(); NEXT; }
CASE(66) { int c = ld_R_xhl<H>(); NEXT; }
CASE(6E) { int c = ld_R_xhl<L>(); NEXT; }
CASE(7E) { int c = ld_R_xhl<A>(); NEXT; }
CASE(76) { int c = halt(); NEXT_STOP; }

CASE(80) { int c = add_a_R<B,0>(); NEXT; }
CASE(81) { int c = add_a_R<C,0>(); NEXT; }
CASE(82) { int c = add_a_R<D,0>(); NEXT; }
CASE(83) { int c = add_a_R<E,0>(); NEXT; }
CASE(84) { int c = add_a_R<H,0>(); NEXT; }
CASE(85) { int c = add_a_R<L,0>(); NEXT; }
CASE(86) { int c = add_a_xhl();   NEXT; }
CASE(87) { int c = add_a_a();     NEXT; }
CASE(88) { int c = adc_a_R<B,0>(); NEXT; }
CASE(89) { int c = adc_a_R<C,0>(); NEXT; }
CASE(8A) { int c = adc_a_R<D,0>(); NEXT; }
CASE(8B) { int c = adc_a_R<E,0>(); NEXT; }
CASE(8C) { int c = adc_a_R<H,0>(); NEXT; }
CASE(8D) { int c = adc_a_R<L,0>(); NEXT; }
CASE(8E) { int c = adc_a_xhl();   NEXT; }
CASE(8F) { int c = adc_a_a();     NEXT; }
CASE(90) { int c = sub_R<B,0>(); NEXT; }
CASE(91) { int c = sub_R<C,0>(); NEXT; }
CASE(92) { int c = sub_R<D,0>(); NEXT; }
CASE(93) { int c = sub_R<E,0>(); NEXT; }
CASE(94) { int c = sub_R<H,0>(); NEXT; }
CASE(95) { int c = sub_R<L,0>(); NEXT; }
CASE(96) { int c = sub_xhl();   NEXT; }
CASE(97) { int c = sub_a();     NEXT; }
CASE(98) { int c = sbc_a_R<B,0>(); NEXT; }
CASE(99) { int c = sbc_a_R<C,0>(); NEXT; }
CASE(9A) { int c = sbc_a_R<D,0>(); NEXT; }
CASE(9B) { int c = sbc_a_R<E,0>(); NEXT; }
CASE(9C) { int c = sbc_a_R<H,0>(); NEXT; }
CASE(9D) { int c = sbc_a_R<L,0>(); NEXT; }
CASE(9E) { int c = sbc_a_xhl();   NEXT; }
CASE(9F) { int c = sbc_a_a();     NEXT; }
CASE(A0) { int c = and_R<B,0>(); NEXT; }
CASE(A1) { int c = and_R<C,0>(); NEXT; }
CASE(A2) { int c = and_R<D,0>(); NEXT; }
CASE(A3) { int c = and_R<E,0>(); NEXT; }
CASE(A4) { int c = and_R<H,0>(); NEXT; }
CASE(A5) { int c = and_R<L,0>(); NEXT; }
CASE(A6) { int c = and_xhl();   NEXT; }
CASE(A7) { int c = and_a();     NEXT; }
CASE(A8) { int c = xor_R<B,0>(); NEXT; }
CASE(A9) { int c = xor_R<C,0>(); NEXT; }
CASE(AA) { int c = xor_R<D,0>(); NEXT; }
CASE(AB) { int c = xor_R<E,0>(); NEXT; }
CASE(AC) { int c = xor_R<H,0>(); NEXT; }
CASE(AD) { int c = xor_R<L,0>(); NEXT; }
CASE(AE) { int c = xor_xhl();   NEXT; }
CASE(AF) { int c = xor_a();     NEXT; }
CASE(B0) { int c = or_R<B,0>(); NEXT; }
CASE(B1) { int c = or_R<C,0>(); NEXT; }
CASE(B2) { int c = or_R<D,0>(); NEXT; }
CASE(B3) { int c = or_R<E,0>(); NEXT; }
CASE(B4) { int c = or_R<H,0>(); NEXT; }
CASE(B5) { int c = or_R<L,0>(); NEXT; }
CASE(B6) { int c = or_xhl();   NEXT; }
CASE(B7) { int c = or_a();     NEXT; }
CASE(B8) { int c = cp_R<B,0>(); NEXT; }
CASE(B9) { int c = cp_R<C,0>(); NEXT; }
CASE(BA) { int c = cp_R<D,0>(); NEXT; }
CASE(BB) { int c = cp_R<E,0>(); NEXT; }
CASE(BC) { int c = cp_R<H,0>(); NEXT; }
CASE(BD) { int c = cp_R<L,0>(); NEXT; }
CASE(BE) { int c = cp_xhl();   NEXT; }
CASE(BF) { int c = cp_a();     NEXT; }

CASE(D3) { int c = out_byte_a(); NEXT; }
CASE(DB) { int c = in_a_byte();  NEXT; }
CASE(D9) { int c = exx(); NEXT; }
CASE(E3) { int c = ex_xsp_SS<HL,0>(); NEXT; }
CASE(EB) { int c = ex_de_hl(); NEXT; }
CASE(E9) { int c = jp_SS<HL,0>(); NEXT; }
CASE(F9) { int c = ld_sp_SS<HL,0>(); NEXT; }
CASE(F3) { int c = di(); NEXT; }
CASE(FB) { int c = ei(); NEXT_EI; }
CASE(C6) { int c = add_a_byte(); NEXT; }
CASE(CE) { int c = adc_a_byte(); NEXT; }
CASE(D6) { int c = sub_byte();   NEXT; }
CASE(DE) { int c = sbc_a_byte(); NEXT; }
CASE(E6) { int c = and_byte();   NEXT; }
CASE(EE) { int c = xor_byte();   NEXT; }
CASE(F6) { int c = or_byte();    NEXT; }
CASE(FE) { int c = cp_byte();    NEXT; }
CASE(C0) { int c = ret(CondNZ()); NEXT; }
CASE(C8) { int c = ret(CondZ ()); NEXT; }
CASE(D0) { int c = ret(CondNC()); NEXT; }
CASE(D8) { int c = ret(CondC ()); NEXT; }
CASE(E0) { int c = ret(CondPO()); NEXT; }
CASE(E8) { int c = ret(CondPE()); NEXT; }
CASE(F0) { int c = ret(CondP ()); NEXT; }
CASE(F8) { int c = ret(CondM ()); NEXT; }
CASE(C9) { int c = ret();         NEXT; }
CASE(C2) { int c = jp(CondNZ()); NEXT; }
CASE(CA) { int c = jp(CondZ ()); NEXT; }
CASE(D2) { int c = jp(CondNC()); NEXT; }
CASE(DA) { int c = jp(CondC ()); NEXT; }
CASE(E2) { int c = jp(CondPO()); NEXT; }
CASE(EA) { int c = jp(CondPE()); NEXT; }
CASE(F2) { int c = jp(CondP ()); NEXT; }
CASE(FA) { int c = jp(CondM ()); NEXT; }
CASE(C3) { int c = jp(CondTrue()); NEXT; }
CASE(C4) { int c = call(CondNZ()); NEXT; }
CASE(CC) { int c = call(CondZ ()); NEXT; }
CASE(D4) { int c = call(CondNC()); NEXT; }
CASE(DC) { int c = call(CondC ()); NEXT; }
CASE(E4) { int c = call(CondPO()); NEXT; }
CASE(EC) { int c = call(CondPE()); NEXT; }
CASE(F4) { int c = call(CondP ()); NEXT; }
CASE(FC) { int c = call(CondM ()); NEXT; }
CASE(CD) { int c = call(CondTrue()); NEXT; }
CASE(C1) { int c = pop_SS <BC,0>(); NEXT; }
CASE(D1) { int c = pop_SS <DE,0>(); NEXT; }
CASE(E1) { int c = pop_SS <HL,0>(); NEXT; }
CASE(F1) { int c = pop_SS <AF,0>(); NEXT; }
CASE(C5) { int c = push_SS<BC,0>(); NEXT; }
CASE(D5) { int c = push_SS<DE,0>(); NEXT; }
CASE(E5) { int c = push_SS<HL,0>(); NEXT; }
CASE(F5) { int c = push_SS<AF,0>(); NEXT; }
CASE(C7) { int c = rst<0x00>(); NEXT; }
CASE(CF) { int c = rst<0x08>(); NEXT; }
CASE(D7) { int c = rst<0x10>(); NEXT; }
CASE(DF) { int c = rst<0x18>(); NEXT; }
CASE(E7) { int c = rst<0x20>(); NEXT; }
CASE(EF) { int c = rst<0x28>(); NEXT; }
CASE(F7) { int c = rst<0x30>(); NEXT; }
CASE(FF) { int c = rst<0x38>(); NEXT; }
CASE(CB) {
	byte cb_opcode = RDMEM_OPCODE(T::CC_PREFIX);
	R.incR(1);
	switch (cb_opcode) {
		case 0x00: { int c = rlc_R<B>(); NEXT; }
		case 0x01: { int c = rlc_R<C>(); NEXT; }
		case 0x02: { int c = rlc_R<D>(); NEXT; }
		case 0x03: { int c = rlc_R<E>(); NEXT; }
		case 0x04: { int c = rlc_R<H>(); NEXT; }
		case 0x05: { int c = rlc_R<L>(); NEXT; }
		case 0x07: { int c = rlc_R<A>(); NEXT; }
		case 0x06: { int c = rlc_xhl();  NEXT; }
		case 0x08: { int c = rrc_R<B>(); NEXT; }
		case 0x09: { int c = rrc_R<C>(); NEXT; }
		case 0x0a: { int c = rrc_R<D>(); NEXT; }
		case 0x0b: { int c = rrc_R<E>(); NEXT; }
		case 0x0c: { int c = rrc_R<H>(); NEXT; }
		case 0x0d: { int c = rrc_R<L>(); NEXT; }
		case 0x0f: { int c = rrc_R<A>(); NEXT; }
		case 0x0e: { int c = rrc_xhl();  NEXT; }
		case 0x10: { int c = rl_R<B>(); NEXT; }
		case 0x11: { int c = rl_R<C>(); NEXT; }
		case 0x12: { int c = rl_R<D>(); NEXT; }
		case 0x13: { int c = rl_R<E>(); NEXT; }
		case 0x14: { int c = rl_R<H>(); NEXT; }
		case 0x15: { int c = rl_R<L>(); NEXT; }
		case 0x17: { int c = rl_R<A>(); NEXT; }
		case 0x16: { int c = rl_xhl();  NEXT; }
		case 0x18: { int c = rr_R<B>(); NEXT; }
		case 0x19: { int c = rr_R<C>(); NEXT; }
		case 0x1a: { int c = rr_R<D>(); NEXT; }
		case 0x1b: { int c = rr_R<E>(); NEXT; }
		case 0x1c: { int c = rr_R<H>(); NEXT; }
		case 0x1d: { int c = rr_R<L>(); NEXT; }
		case 0x1f: { int c = rr_R<A>(); NEXT; }
		case 0x1e: { int c = rr_xhl();  NEXT; }
		case 0x20: { int c = sla_R<B>(); NEXT; }
		case 0x21: { int c = sla_R<C>(); NEXT; }
		case 0x22: { int c = sla_R<D>(); NEXT; }
		case 0x23: { int c = sla_R<E>(); NEXT; }
		case 0x24: { int c = sla_R<H>(); NEXT; }
		case 0x25: { int c = sla_R<L>(); NEXT; }
		case 0x27: { int c = sla_R<A>(); NEXT; }
		case 0x26: { int c = sla_xhl();  NEXT; }
		case 0x28: { int c = sra_R<B>(); NEXT; }
		case 0x29: { int c = sra_R<C>(); NEXT; }
		case 0x2a: { int c = sra_R<D>(); NEXT; }
		case 0x2b: { int c = sra_R<E>(); NEXT; }
		case 0x2c: { int c = sra_R<H>(); NEXT; }
		case 0x2d: { int c = sra_R<L>(); NEXT; }
		case 0x2f: { int c = sra_R<A>(); NEXT; }
		case 0x2e: { int c = sra_xhl();  NEXT; }
		case 0x30: { int c = T::isR800() ? sla_R<B>() : sll_R<B>(); NEXT; }
		case 0x31: { int c = T::isR800() ? sla_R<C>() : sll_R<C>(); NEXT; }
		case 0x32: { int c = T::isR800() ? sla_R<D>() : sll_R<D>(); NEXT; }
		case 0x33: { int c = T::isR800() ? sla_R<E>() : sll_R<E>(); NEXT; }
		case 0x34: { int c = T::isR800() ? sla_R<H>() : sll_R<H>(); NEXT; }
		case 0x35: { int c = T::isR800() ? sla_R<L>() : sll_R<L>(); NEXT; }
		case 0x37: { int c = T::isR800() ? sla_R<A>() : sll_R<A>(); NEXT; }
		case 0x36: { int c = T::isR800() ? sla_xhl()  : sll_xhl();  NEXT; }
		case 0x38: { int c = srl_R<B>(); NEXT; }
		case 0x39: { int c = srl_R<C>(); NEXT; }
		case 0x3a: { int c = srl_R<D>(); NEXT; }
		case 0x3b: { int c = srl_R<E>(); NEXT; }
		case 0x3c: { int c = srl_R<H>(); NEXT; }
		case 0x3d: { int c = srl_R<L>(); NEXT; }
		case 0x3f: { int c = srl_R<A>(); NEXT; }
		case 0x3e: { int c = srl_xhl();  NEXT; }

		case 0x40: { int c = bit_N_R<0,B>(); NEXT; }
		case 0x41: { int c = bit_N_R<0,C>(); NEXT; }
		case 0x42: { int c = bit_N_R<0,D>(); NEXT; }
		case 0x43: { int c = bit_N_R<0,E>(); NEXT; }
		case 0x44: { int c = bit_N_R<0,H>(); NEXT; }
		case 0x45: { int c = bit_N_R<0,L>(); NEXT; }
		case 0x47: { int c = bit_N_R<0,A>(); NEXT; }
		case 0x48: { int c = bit_N_R<1,B>(); NEXT; }
		case 0x49: { int c = bit_N_R<1,C>(); NEXT; }
		case 0x4a: { int c = bit_N_R<1,D>(); NEXT; }
		case 0x4b: { int c = bit_N_R<1,E>(); NEXT; }
		case 0x4c: { int c = bit_N_R<1,H>(); NEXT; }
		case 0x4d: { int c = bit_N_R<1,L>(); NEXT; }
		case 0x4f: { int c = bit_N_R<1,A>(); NEXT; }
		case 0x50: { int c = bit_N_R<2,B>(); NEXT; }
		case 0x51: { int c = bit_N_R<2,C>(); NEXT; }
		case 0x52: { int c = bit_N_R<2,D>(); NEXT; }
		case 0x53: { int c = bit_N_R<2,E>(); NEXT; }
		case 0x54: { int c = bit_N_R<2,H>(); NEXT; }
		case 0x55: { int c = bit_N_R<2,L>(); NEXT; }
		case 0x57: { int c = bit_N_R<2,A>(); NEXT; }
		case 0x58: { int c = bit_N_R<3,B>(); NEXT; }
		case 0x59: { int c = bit_N_R<3,C>(); NEXT; }
		case 0x5a: { int c = bit_N_R<3,D>(); NEXT; }
		case 0x5b: { int c = bit_N_R<3,E>(); NEXT; }
		case 0x5c: { int c = bit_N_R<3,H>(); NEXT; }
		case 0x5d: { int c = bit_N_R<3,L>(); NEXT; }
		case 0x5f: { int c = bit_N_R<3,A>(); NEXT; }
		case 0x60: { int c = bit_N_R<4,B>(); NEXT; }
		case 0x61: { int c = bit_N_R<4,C>(); NEXT; }
		case 0x62: { int c = bit_N_R<4,D>(); NEXT; }
		case 0x63: { int c = bit_N_R<4,E>(); NEXT; }
		case 0x64: { int c = bit_N_R<4,H>(); NEXT; }
		case 0x65: { int c = bit_N_R<4,L>(); NEXT; }
		case 0x67: { int c = bit_N_R<4,A>(); NEXT; }
		case 0x68: { int c = bit_N_R<5,B>(); NEXT; }
		case 0x69: { int c = bit_N_R<5,C>(); NEXT; }
		case 0x6a: { int c = bit_N_R<5,D>(); NEXT; }
		case 0x6b: { int c = bit_N_R<5,E>(); NEXT; }
		case 0x6c: { int c = bit_N_R<5,H>(); NEXT; }
		case 0x6d: { int c = bit_N_R<5,L>(); NEXT; }
		case 0x6f: { int c = bit_N_R<5,A>(); NEXT; }
		case 0x70: { int c = bit_N_R<6,B>(); NEXT; }
		case 0x71: { int c = bit_N_R<6,C>(); NEXT; }
		case 0x72: { int c = bit_N_R<6,D>(); NEXT; }
		case 0x73: { int c = bit_N_R<6,E>(); NEXT; }
		case 0x74: { int c = bit_N_R<6,H>(); NEXT; }
		case 0x75: { int c = bit_N_R<6,L>(); NEXT; }
		case 0x77: { int c = bit_N_R<6,A>(); NEXT; }
		case 0x78: { int c = bit_N_R<7,B>(); NEXT; }
		case 0x79: { int c = bit_N_R<7,C>(); NEXT; }
		case 0x7a: { int c = bit_N_R<7,D>(); NEXT; }
		case 0x7b: { int c = bit_N_R<7,E>(); NEXT; }
		case 0x7c: { int c = bit_N_R<7,H>(); NEXT; }
		case 0x7d: { int c = bit_N_R<7,L>(); NEXT; }
		case 0x7f: { int c = bit_N_R<7,A>(); NEXT; }
		case 0x46: { int c = bit_N_xhl<0>(); NEXT; }
		case 0x4e: { int c = bit_N_xhl<1>(); NEXT; }
		case 0x56: { int c = bit_N_xhl<2>(); NEXT; }
		case 0x5e: { int c = bit_N_xhl<3>(); NEXT; }
		case 0x66: { int c = bit_N_xhl<4>(); NEXT; }
		case 0x6e: { int c = bit_N_xhl<5>(); NEXT; }
		case 0x76: { int c = bit_N_xhl<6>(); NEXT; }
		case 0x7e: { int c = bit_N_xhl<7>(); NEXT; }

		case 0x80: { int c = res_N_R<0,B>(); NEXT; }
		case 0x81: { int c = res_N_R<0,C>(); NEXT; }
		case 0x82: { int c = res_N_R<0,D>(); NEXT; }
		case 0x83: { int c = res_N_R<0,E>(); NEXT; }
		case 0x84: { int c = res_N_R<0,H>(); NEXT; }
		case 0x85: { int c = res_N_R<0,L>(); NEXT; }
		case 0x87: { int c = res_N_R<0,A>(); NEXT; }
		case 0x88: { int c = res_N_R<1,B>(); NEXT; }
		case 0x89: { int c = res_N_R<1,C>(); NEXT; }
		case 0x8a: { int c = res_N_R<1,D>(); NEXT; }
		case 0x8b: { int c = res_N_R<1,E>(); NEXT; }
		case 0x8c: { int c = res_N_R<1,H>(); NEXT; }
		case 0x8d: { int c = res_N_R<1,L>(); NEXT; }
		case 0x8f: { int c = res_N_R<1,A>(); NEXT; }
		case 0x90: { int c = res_N_R<2,B>(); NEXT; }
		case 0x91: { int c = res_N_R<2,C>(); NEXT; }
		case 0x92: { int c = res_N_R<2,D>(); NEXT; }
		case 0x93: { int c = res_N_R<2,E>(); NEXT; }
		case 0x94: { int c = res_N_R<2,H>(); NEXT; }
		case 0x95: { int c = res_N_R<2,L>(); NEXT; }
		case 0x97: { int c = res_N_R<2,A>(); NEXT; }
		case 0x98: { int c = res_N_R<3,B>(); NEXT; }
		case 0x99: { int c = res_N_R<3,C>(); NEXT; }
		case 0x9a: { int c = res_N_R<3,D>(); NEXT; }
		case 0x9b: { int c = res_N_R<3,E>(); NEXT; }
		case 0x9c: { int c = res_N_R<3,H>(); NEXT; }
		case 0x9d: { int c = res_N_R<3,L>(); NEXT; }
		case 0x9f: { int c = res_N_R<3,A>(); NEXT; }
		case 0xa0: { int c = res_N_R<4,B>(); NEXT; }
		case 0xa1: { int c = res_N_R<4,C>(); NEXT; }
		case 0xa2: { int c = res_N_R<4,D>(); NEXT; }
		case 0xa3: { int c = res_N_R<4,E>(); NEXT; }
		case 0xa4: { int c = res_N_R<4,H>(); NEXT; }
		case 0xa5: { int c = res_N_R<4,L>(); NEXT; }
		case 0xa7: { int c = res_N_R<4,A>(); NEXT; }
		case 0xa8: { int c = res_N_R<5,B>(); NEXT; }
		case 0xa9: { int c = res_N_R<5,C>(); NEXT; }
		case 0xaa: { int c = res_N_R<5,D>(); NEXT; }
		case 0xab: { int c = res_N_R<5,E>(); NEXT; }
		case 0xac: { int c = res_N_R<5,H>(); NEXT; }
		case 0xad: { int c = res_N_R<5,L>(); NEXT; }
		case 0xaf: { int c = res_N_R<5,A>(); NEXT; }
		case 0xb0: { int c = res_N_R<6,B>(); NEXT; }
		case 0xb1: { int c = res_N_R<6,C>(); NEXT; }
		case 0xb2: { int c = res_N_R<6,D>(); NEXT; }
		case 0xb3: { int c = res_N_R<6,E>(); NEXT; }
		case 0xb4: { int c = res_N_R<6,H>(); NEXT; }
		case 0xb5: { int c = res_N_R<6,L>(); NEXT; }
		case 0xb7: { int c = res_N_R<6,A>(); NEXT; }
		case 0xb8: { int c = res_N_R<7,B>(); NEXT; }
		case 0xb9: { int c = res_N_R<7,C>(); NEXT; }
		case 0xba: { int c = res_N_R<7,D>(); NEXT; }
		case 0xbb: { int c = res_N_R<7,E>(); NEXT; }
		case 0xbc: { int c = res_N_R<7,H>(); NEXT; }
		case 0xbd: { int c = res_N_R<7,L>(); NEXT; }
		case 0xbf: { int c = res_N_R<7,A>(); NEXT; }
		case 0x86: { int c = res_N_xhl<0>(); NEXT; }
		case 0x8e: { int c = res_N_xhl<1>(); NEXT; }
		case 0x96: { int c = res_N_xhl<2>(); NEXT; }
		case 0x9e: { int c = res_N_xhl<3>(); NEXT; }
		case 0xa6: { int c = res_N_xhl<4>(); NEXT; }
		case 0xae: { int c = res_N_xhl<5>(); NEXT; }
		case 0xb6: { int c = res_N_xhl<6>(); NEXT; }
		case 0xbe: { int c = res_N_xhl<7>(); NEXT; }

		case 0xc0: { int c = set_N_R<0,B>(); NEXT; }
		case 0xc1: { int c = set_N_R<0,C>(); NEXT; }
		case 0xc2: { int c = set_N_R<0,D>(); NEXT; }
		case 0xc3: { int c = set_N_R<0,E>(); NEXT; }
		case 0xc4: { int c = set_N_R<0,H>(); NEXT; }
		case 0xc5: { int c = set_N_R<0,L>(); NEXT; }
		case 0xc7: { int c = set_N_R<0,A>(); NEXT; }
		case 0xc8: { int c = set_N_R<1,B>(); NEXT; }
		case 0xc9: { int c = set_N_R<1,C>(); NEXT; }
		case 0xca: { int c = set_N_R<1,D>(); NEXT; }
		case 0xcb: { int c = set_N_R<1,E>(); NEXT; }
		case 0xcc: { int c = set_N_R<1,H>(); NEXT; }
		case 0xcd: { int c = set_N_R<1,L>(); NEXT; }
		case 0xcf: { int c = set_N_R<1,A>(); NEXT; }
		case 0xd0: { int c = set_N_R<2,B>(); NEXT; }
		case 0xd1: { int c = set_N_R<2,C>(); NEXT; }
		case 0xd2: { int c = set_N_R<2,D>(); NEXT; }
		case 0xd3: { int c = set_N_R<2,E>(); NEXT; }
		case 0xd4: { int c = set_N_R<2,H>(); NEXT; }
		case 0xd5: { int c = set_N_R<2,L>(); NEXT; }
		case 0xd7: { int c = set_N_R<2,A>(); NEXT; }
		case 0xd8: { int c = set_N_R<3,B>(); NEXT; }
		case 0xd9: { int c = set_N_R<3,C>(); NEXT; }
		case 0xda: { int c = set_N_R<3,D>(); NEXT; }
		case 0xdb: { int c = set_N_R<3,E>(); NEXT; }
		case 0xdc: { int c = set_N_R<3,H>(); NEXT; }
		case 0xdd: { int c = set_N_R<3,L>(); NEXT; }
		case 0xdf: { int c = set_N_R<3,A>(); NEXT; }
		case 0xe0: { int c = set_N_R<4,B>(); NEXT; }
		case 0xe1: { int c = set_N_R<4,C>(); NEXT; }
		case 0xe2: { int c = set_N_R<4,D>(); NEXT; }
		case 0xe3: { int c = set_N_R<4,E>(); NEXT; }
		case 0xe4: { int c = set_N_R<4,H>(); NEXT; }
		case 0xe5: { int c = set_N_R<4,L>(); NEXT; }
		case 0xe7: { int c = set_N_R<4,A>(); NEXT; }
		case 0xe8: { int c = set_N_R<5,B>(); NEXT; }
		case 0xe9: { int c = set_N_R<5,C>(); NEXT; }
		case 0xea: { int c = set_N_R<5,D>(); NEXT; }
		case 0xeb: { int c = set_N_R<5,E>(); NEXT; }
		case 0xec: { int c = set_N_R<5,H>(); NEXT; }
		case 0xed: { int c = set_N_R<5,L>(); NEXT; }
		case 0xef: { int c = set_N_R<5,A>(); NEXT; }
		case 0xf0: { int c = set_N_R<6,B>(); NEXT; }
		case 0xf1: { int c = set_N_R<6,C>(); NEXT; }
		case 0xf2: { int c = set_N_R<6,D>(); NEXT; }
		case 0xf3: { int c = set_N_R<6,E>(); NEXT; }
		case 0xf4: { int c = set_N_R<6,H>(); NEXT; }
		case 0xf5: { int c = set_N_R<6,L>(); NEXT; }
		case 0xf7: { int c = set_N_R<6,A>(); NEXT; }
		case 0xf8: { int c = set_N_R<7,B>(); NEXT; }
		case 0xf9: { int c = set_N_R<7,C>(); NEXT; }
		case 0xfa: { int c = set_N_R<7,D>(); NEXT; }
		case 0xfb: { int c = set_N_R<7,E>(); NEXT; }
		case 0xfc: { int c = set_N_R<7,H>(); NEXT; }
		case 0xfd: { int c = set_N_R<7,L>(); NEXT; }
		case 0xff: { int c = set_N_R<7,A>(); NEXT; }
		case 0xc6: { int c = set_N_xhl<0>(); NEXT; }
		case 0xce: { int c = set_N_xhl<1>(); NEXT; }
		case 0xd6: { int c = set_N_xhl<2>(); NEXT; }
		case 0xde: { int c = set_N_xhl<3>(); NEXT; }
		case 0xe6: { int c = set_N_xhl<4>(); NEXT; }
		case 0xee: { int c = set_N_xhl<5>(); NEXT; }
		case 0xf6: { int c = set_N_xhl<6>(); NEXT; }
		case 0xfe: { int c = set_N_xhl<7>(); NEXT; }
		default: UNREACHABLE; return;
	}
}
CASE(ED) {
	byte ed_opcode = RDMEM_OPCODE(T::CC_PREFIX);
	R.incR(1);
	switch (ed_opcode) {
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
			{ int c = nop(); NEXT; }

		case 0x40: { int c = in_R_c<B>(); NEXT; }
		case 0x48: { int c = in_R_c<C>(); NEXT; }
		case 0x50: { int c = in_R_c<D>(); NEXT; }
		case 0x58: { int c = in_R_c<E>(); NEXT; }
		case 0x60: { int c = in_R_c<H>(); NEXT; }
		case 0x68: { int c = in_R_c<L>(); NEXT; }
		case 0x70: { int c = in_R_c<DUMMY>(); NEXT; }
		case 0x78: { int c = in_R_c<A>(); NEXT; }

		case 0x41: { int c = out_c_R<B>(); NEXT; }
		case 0x49: { int c = out_c_R<C>(); NEXT; }
		case 0x51: { int c = out_c_R<D>(); NEXT; }
		case 0x59: { int c = out_c_R<E>(); NEXT; }
		case 0x61: { int c = out_c_R<H>(); NEXT; }
		case 0x69: { int c = out_c_R<L>(); NEXT; }
		case 0x71: { int c = out_c_0();    NEXT; }
		case 0x79: { int c = out_c_R<A>(); NEXT; }

		case 0x42: { int c = sbc_hl_SS<BC>(); NEXT; }
		case 0x52: { int c = sbc_hl_SS<DE>(); NEXT; }
		case 0x62: { int c = sbc_hl_hl    (); NEXT; }
		case 0x72: { int c = sbc_hl_SS<SP>(); NEXT; }

		case 0x4a: { int c = adc_hl_SS<BC>(); NEXT; }
		case 0x5a: { int c = adc_hl_SS<DE>(); NEXT; }
		case 0x6a: { int c = adc_hl_hl    (); NEXT; }
		case 0x7a: { int c = adc_hl_SS<SP>(); NEXT; }

		case 0x43: { int c = ld_xword_SS_ED<BC>(); NEXT; }
		case 0x53: { int c = ld_xword_SS_ED<DE>(); NEXT; }
		case 0x63: { int c = ld_xword_SS_ED<HL>(); NEXT; }
		case 0x73: { int c = ld_xword_SS_ED<SP>(); NEXT; }

		case 0x4b: { int c = ld_SS_xword_ED<BC>(); NEXT; }
		case 0x5b: { int c = ld_SS_xword_ED<DE>(); NEXT; }
		case 0x6b: { int c = ld_SS_xword_ED<HL>(); NEXT; }
		case 0x7b: { int c = ld_SS_xword_ED<SP>(); NEXT; }

		case 0x47: { int c = ld_i_a(); NEXT; }
		case 0x4f: { int c = ld_r_a(); NEXT; }
		case 0x57: { int c = ld_a_IR<REG_I>(); if (T::isR800()) { NEXT; } else { NEXT_STOP; }}
		case 0x5f: { int c = ld_a_IR<REG_R>(); if (T::isR800()) { NEXT; } else { NEXT_STOP; }}

		case 0x67: { int c = rrd(); NEXT; }
		case 0x6f: { int c = rld(); NEXT; }

		case 0x45: case 0x4d: case 0x55: case 0x5d:
		case 0x65: case 0x6d: case 0x75: case 0x7d:
			{ int c = retn(); NEXT_STOP; }
		case 0x46: case 0x4e: case 0x66: case 0x6e:
			{ int c = im_N<0>(); NEXT; }
		case 0x56: case 0x76:
			{ int c = im_N<1>(); NEXT; }
		case 0x5e: case 0x7e:
			{ int c = im_N<2>(); NEXT; }
		case 0x44: case 0x4c: case 0x54: case 0x5c:
		case 0x64: case 0x6c: case 0x74: case 0x7c:
			{ int c = neg(); NEXT; }

		case 0xa0: { int c = ldi();  NEXT; }
		case 0xa1: { int c = cpi();  NEXT; }
		case 0xa2: { int c = ini();  NEXT; }
		case 0xa3: { int c = outi(); NEXT; }
		case 0xa8: { int c = ldd();  NEXT; }
		case 0xa9: { int c = cpd();  NEXT; }
		case 0xaa: { int c = ind();  NEXT; }
		case 0xab: { int c = outd(); NEXT; }
		case 0xb0: { int c = ldir(); NEXT; }
		case 0xb1: { int c = cpir(); NEXT; }
		case 0xb2: { int c = inir(); NEXT; }
		case 0xb3: { int c = otir(); NEXT; }
		case 0xb8: { int c = lddr(); NEXT; }
		case 0xb9: { int c = cpdr(); NEXT; }
		case 0xba: { int c = indr(); NEXT; }
		case 0xbb: { int c = otdr(); NEXT; }

		case 0xc1: { int c = T::isR800() ? mulub_a_R<B>() : nop(); NEXT; }
		case 0xc9: { int c = T::isR800() ? mulub_a_R<C>() : nop(); NEXT; }
		case 0xd1: { int c = T::isR800() ? mulub_a_R<D>() : nop(); NEXT; }
		case 0xd9: { int c = T::isR800() ? mulub_a_R<E>() : nop(); NEXT; }
		case 0xc3: { int c = T::isR800() ? muluw_hl_SS<BC>() : nop(); NEXT; }
		case 0xf3: { int c = T::isR800() ? muluw_hl_SS<SP>() : nop(); NEXT; }
		default: UNREACHABLE; return;
	}
}
opDD_2:
CASE(DD) {
	byte opcodeDD = RDMEM_OPCODE(T::CC_DD + T::CC_MAIN);
	R.incR(1);
	switch (opcodeDD) {
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
			if (T::isR800()) {
				int c = T::CC_DD + nop(); NEXT;
			} else {
				T::add(T::CC_DD);
				#ifdef USE_COMPUTED_GOTO
				goto *(opcodeTable[opcodeDD]);
				#else
				opcodeMain = opcodeDD;
				goto switchopcode;
				#endif
			}

		case 0x09: { int c = add_SS_TT<IX,BC,T::CC_DD>(); NEXT; }
		case 0x19: { int c = add_SS_TT<IX,DE,T::CC_DD>(); NEXT; }
		case 0x29: { int c = add_SS_SS<IX   ,T::CC_DD>(); NEXT; }
		case 0x39: { int c = add_SS_TT<IX,SP,T::CC_DD>(); NEXT; }
		case 0x21: { int c = ld_SS_word<IX,T::CC_DD>();  NEXT; }
		case 0x22: { int c = ld_xword_SS<IX,T::CC_DD>(); NEXT; }
		case 0x2a: { int c = ld_SS_xword<IX,T::CC_DD>(); NEXT; }
		case 0x23: { int c = inc_SS<IX,T::CC_DD>(); NEXT; }
		case 0x2b: { int c = dec_SS<IX,T::CC_DD>(); NEXT; }
		case 0x24: { int c = inc_R<IXH,T::CC_DD>(); NEXT; }
		case 0x2c: { int c = inc_R<IXL,T::CC_DD>(); NEXT; }
		case 0x25: { int c = dec_R<IXH,T::CC_DD>(); NEXT; }
		case 0x2d: { int c = dec_R<IXL,T::CC_DD>(); NEXT; }
		case 0x26: { int c = ld_R_byte<IXH,T::CC_DD>(); NEXT; }
		case 0x2e: { int c = ld_R_byte<IXL,T::CC_DD>(); NEXT; }
		case 0x34: { int c = inc_xix<IX>(); NEXT; }
		case 0x35: { int c = dec_xix<IX>(); NEXT; }
		case 0x36: { int c = ld_xix_byte<IX>(); NEXT; }

		case 0x44: { int c = ld_R_R<B,IXH,T::CC_DD>(); NEXT; }
		case 0x45: { int c = ld_R_R<B,IXL,T::CC_DD>(); NEXT; }
		case 0x4c: { int c = ld_R_R<C,IXH,T::CC_DD>(); NEXT; }
		case 0x4d: { int c = ld_R_R<C,IXL,T::CC_DD>(); NEXT; }
		case 0x54: { int c = ld_R_R<D,IXH,T::CC_DD>(); NEXT; }
		case 0x55: { int c = ld_R_R<D,IXL,T::CC_DD>(); NEXT; }
		case 0x5c: { int c = ld_R_R<E,IXH,T::CC_DD>(); NEXT; }
		case 0x5d: { int c = ld_R_R<E,IXL,T::CC_DD>(); NEXT; }
		case 0x7c: { int c = ld_R_R<A,IXH,T::CC_DD>(); NEXT; }
		case 0x7d: { int c = ld_R_R<A,IXL,T::CC_DD>(); NEXT; }
		case 0x60: { int c = ld_R_R<IXH,B,T::CC_DD>(); NEXT; }
		case 0x61: { int c = ld_R_R<IXH,C,T::CC_DD>(); NEXT; }
		case 0x62: { int c = ld_R_R<IXH,D,T::CC_DD>(); NEXT; }
		case 0x63: { int c = ld_R_R<IXH,E,T::CC_DD>(); NEXT; }
		case 0x65: { int c = ld_R_R<IXH,IXL,T::CC_DD>(); NEXT; }
		case 0x67: { int c = ld_R_R<IXH,A,T::CC_DD>(); NEXT; }
		case 0x68: { int c = ld_R_R<IXL,B,T::CC_DD>(); NEXT; }
		case 0x69: { int c = ld_R_R<IXL,C,T::CC_DD>(); NEXT; }
		case 0x6a: { int c = ld_R_R<IXL,D,T::CC_DD>(); NEXT; }
		case 0x6b: { int c = ld_R_R<IXL,E,T::CC_DD>(); NEXT; }
		case 0x6c: { int c = ld_R_R<IXL,IXH,T::CC_DD>(); NEXT; }
		case 0x6f: { int c = ld_R_R<IXL,A,T::CC_DD>(); NEXT; }
		case 0x70: { int c = ld_xix_R<IX,B>(); NEXT; }
		case 0x71: { int c = ld_xix_R<IX,C>(); NEXT; }
		case 0x72: { int c = ld_xix_R<IX,D>(); NEXT; }
		case 0x73: { int c = ld_xix_R<IX,E>(); NEXT; }
		case 0x74: { int c = ld_xix_R<IX,H>(); NEXT; }
		case 0x75: { int c = ld_xix_R<IX,L>(); NEXT; }
		case 0x77: { int c = ld_xix_R<IX,A>(); NEXT; }
		case 0x46: { int c = ld_R_xix<B,IX>(); NEXT; }
		case 0x4e: { int c = ld_R_xix<C,IX>(); NEXT; }
		case 0x56: { int c = ld_R_xix<D,IX>(); NEXT; }
		case 0x5e: { int c = ld_R_xix<E,IX>(); NEXT; }
		case 0x66: { int c = ld_R_xix<H,IX>(); NEXT; }
		case 0x6e: { int c = ld_R_xix<L,IX>(); NEXT; }
		case 0x7e: { int c = ld_R_xix<A,IX>(); NEXT; }

		case 0x84: { int c = add_a_R<IXH,T::CC_DD>(); NEXT; }
		case 0x85: { int c = add_a_R<IXL,T::CC_DD>(); NEXT; }
		case 0x86: { int c = add_a_xix<IX>(); NEXT; }
		case 0x8c: { int c = adc_a_R<IXH,T::CC_DD>(); NEXT; }
		case 0x8d: { int c = adc_a_R<IXL,T::CC_DD>(); NEXT; }
		case 0x8e: { int c = adc_a_xix<IX>(); NEXT; }
		case 0x94: { int c = sub_R<IXH,T::CC_DD>(); NEXT; }
		case 0x95: { int c = sub_R<IXL,T::CC_DD>(); NEXT; }
		case 0x96: { int c = sub_xix<IX>(); NEXT; }
		case 0x9c: { int c = sbc_a_R<IXH,T::CC_DD>(); NEXT; }
		case 0x9d: { int c = sbc_a_R<IXL,T::CC_DD>(); NEXT; }
		case 0x9e: { int c = sbc_a_xix<IX>(); NEXT; }
		case 0xa4: { int c = and_R<IXH,T::CC_DD>(); NEXT; }
		case 0xa5: { int c = and_R<IXL,T::CC_DD>(); NEXT; }
		case 0xa6: { int c = and_xix<IX>(); NEXT; }
		case 0xac: { int c = xor_R<IXH,T::CC_DD>(); NEXT; }
		case 0xad: { int c = xor_R<IXL,T::CC_DD>(); NEXT; }
		case 0xae: { int c = xor_xix<IX>(); NEXT; }
		case 0xb4: { int c = or_R<IXH,T::CC_DD>(); NEXT; }
		case 0xb5: { int c = or_R<IXL,T::CC_DD>(); NEXT; }
		case 0xb6: { int c = or_xix<IX>(); NEXT; }
		case 0xbc: { int c = cp_R<IXH,T::CC_DD>(); NEXT; }
		case 0xbd: { int c = cp_R<IXL,T::CC_DD>(); NEXT; }
		case 0xbe: { int c = cp_xix<IX>(); NEXT; }

		case 0xe1: { int c = pop_SS <IX,T::CC_DD>(); NEXT; }
		case 0xe5: { int c = push_SS<IX,T::CC_DD>(); NEXT; }
		case 0xe3: { int c = ex_xsp_SS<IX,T::CC_DD>(); NEXT; }
		case 0xe9: { int c = jp_SS<IX,T::CC_DD>(); NEXT; }
		case 0xf9: { int c = ld_sp_SS<IX,T::CC_DD>(); NEXT; }
		case 0xcb: ixy = R.getIX(); goto xx_cb;
		case 0xdd: T::add(T::CC_DD); goto opDD_2;
		case 0xfd: T::add(T::CC_DD); goto opFD_2;
		default: UNREACHABLE; return;
	}
}
opFD_2:
CASE(FD) {
	byte opcodeFD = RDMEM_OPCODE(T::CC_DD + T::CC_MAIN);
	R.incR(1);
	switch (opcodeFD) {
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
			if (T::isR800()) {
				int c = T::CC_DD + nop(); NEXT;
			} else {
				T::add(T::CC_DD);
				#ifdef USE_COMPUTED_GOTO
				goto *(opcodeTable[opcodeFD]);
				#else
				opcodeMain = opcodeFD;
				goto switchopcode;
				#endif
			}

		case 0x09: { int c = add_SS_TT<IY,BC,T::CC_DD>(); NEXT; }
		case 0x19: { int c = add_SS_TT<IY,DE,T::CC_DD>(); NEXT; }
		case 0x29: { int c = add_SS_SS<IY   ,T::CC_DD>(); NEXT; }
		case 0x39: { int c = add_SS_TT<IY,SP,T::CC_DD>(); NEXT; }
		case 0x21: { int c = ld_SS_word<IY,T::CC_DD>();  NEXT; }
		case 0x22: { int c = ld_xword_SS<IY,T::CC_DD>(); NEXT; }
		case 0x2a: { int c = ld_SS_xword<IY,T::CC_DD>(); NEXT; }
		case 0x23: { int c = inc_SS<IY,T::CC_DD>(); NEXT; }
		case 0x2b: { int c = dec_SS<IY,T::CC_DD>(); NEXT; }
		case 0x24: { int c = inc_R<IYH,T::CC_DD>(); NEXT; }
		case 0x2c: { int c = inc_R<IYL,T::CC_DD>(); NEXT; }
		case 0x25: { int c = dec_R<IYH,T::CC_DD>(); NEXT; }
		case 0x2d: { int c = dec_R<IYL,T::CC_DD>(); NEXT; }
		case 0x26: { int c = ld_R_byte<IYH,T::CC_DD>(); NEXT; }
		case 0x2e: { int c = ld_R_byte<IYL,T::CC_DD>(); NEXT; }
		case 0x34: { int c = inc_xix<IY>(); NEXT; }
		case 0x35: { int c = dec_xix<IY>(); NEXT; }
		case 0x36: { int c = ld_xix_byte<IY>(); NEXT; }

		case 0x44: { int c = ld_R_R<B,IYH,T::CC_DD>(); NEXT; }
		case 0x45: { int c = ld_R_R<B,IYL,T::CC_DD>(); NEXT; }
		case 0x4c: { int c = ld_R_R<C,IYH,T::CC_DD>(); NEXT; }
		case 0x4d: { int c = ld_R_R<C,IYL,T::CC_DD>(); NEXT; }
		case 0x54: { int c = ld_R_R<D,IYH,T::CC_DD>(); NEXT; }
		case 0x55: { int c = ld_R_R<D,IYL,T::CC_DD>(); NEXT; }
		case 0x5c: { int c = ld_R_R<E,IYH,T::CC_DD>(); NEXT; }
		case 0x5d: { int c = ld_R_R<E,IYL,T::CC_DD>(); NEXT; }
		case 0x7c: { int c = ld_R_R<A,IYH,T::CC_DD>(); NEXT; }
		case 0x7d: { int c = ld_R_R<A,IYL,T::CC_DD>(); NEXT; }
		case 0x60: { int c = ld_R_R<IYH,B,T::CC_DD>(); NEXT; }
		case 0x61: { int c = ld_R_R<IYH,C,T::CC_DD>(); NEXT; }
		case 0x62: { int c = ld_R_R<IYH,D,T::CC_DD>(); NEXT; }
		case 0x63: { int c = ld_R_R<IYH,E,T::CC_DD>(); NEXT; }
		case 0x65: { int c = ld_R_R<IYH,IYL,T::CC_DD>(); NEXT; }
		case 0x67: { int c = ld_R_R<IYH,A,T::CC_DD>(); NEXT; }
		case 0x68: { int c = ld_R_R<IYL,B,T::CC_DD>(); NEXT; }
		case 0x69: { int c = ld_R_R<IYL,C,T::CC_DD>(); NEXT; }
		case 0x6a: { int c = ld_R_R<IYL,D,T::CC_DD>(); NEXT; }
		case 0x6b: { int c = ld_R_R<IYL,E,T::CC_DD>(); NEXT; }
		case 0x6c: { int c = ld_R_R<IYL,IYH,T::CC_DD>(); NEXT; }
		case 0x6f: { int c = ld_R_R<IYL,A,T::CC_DD>(); NEXT; }
		case 0x70: { int c = ld_xix_R<IY,B>(); NEXT; }
		case 0x71: { int c = ld_xix_R<IY,C>(); NEXT; }
		case 0x72: { int c = ld_xix_R<IY,D>(); NEXT; }
		case 0x73: { int c = ld_xix_R<IY,E>(); NEXT; }
		case 0x74: { int c = ld_xix_R<IY,H>(); NEXT; }
		case 0x75: { int c = ld_xix_R<IY,L>(); NEXT; }
		case 0x77: { int c = ld_xix_R<IY,A>(); NEXT; }
		case 0x46: { int c = ld_R_xix<B,IY>(); NEXT; }
		case 0x4e: { int c = ld_R_xix<C,IY>(); NEXT; }
		case 0x56: { int c = ld_R_xix<D,IY>(); NEXT; }
		case 0x5e: { int c = ld_R_xix<E,IY>(); NEXT; }
		case 0x66: { int c = ld_R_xix<H,IY>(); NEXT; }
		case 0x6e: { int c = ld_R_xix<L,IY>(); NEXT; }
		case 0x7e: { int c = ld_R_xix<A,IY>(); NEXT; }

		case 0x84: { int c = add_a_R<IYH,T::CC_DD>(); NEXT; }
		case 0x85: { int c = add_a_R<IYL,T::CC_DD>(); NEXT; }
		case 0x86: { int c = add_a_xix<IY>(); NEXT; }
		case 0x8c: { int c = adc_a_R<IYH,T::CC_DD>(); NEXT; }
		case 0x8d: { int c = adc_a_R<IYL,T::CC_DD>(); NEXT; }
		case 0x8e: { int c = adc_a_xix<IY>(); NEXT; }
		case 0x94: { int c = sub_R<IYH,T::CC_DD>(); NEXT; }
		case 0x95: { int c = sub_R<IYL,T::CC_DD>(); NEXT; }
		case 0x96: { int c = sub_xix<IY>(); NEXT; }
		case 0x9c: { int c = sbc_a_R<IYH,T::CC_DD>(); NEXT; }
		case 0x9d: { int c = sbc_a_R<IYL,T::CC_DD>(); NEXT; }
		case 0x9e: { int c = sbc_a_xix<IY>(); NEXT; }
		case 0xa4: { int c = and_R<IYH,T::CC_DD>(); NEXT; }
		case 0xa5: { int c = and_R<IYL,T::CC_DD>(); NEXT; }
		case 0xa6: { int c = and_xix<IY>(); NEXT; }
		case 0xac: { int c = xor_R<IYH,T::CC_DD>(); NEXT; }
		case 0xad: { int c = xor_R<IYL,T::CC_DD>(); NEXT; }
		case 0xae: { int c = xor_xix<IY>(); NEXT; }
		case 0xb4: { int c = or_R<IYH,T::CC_DD>(); NEXT; }
		case 0xb5: { int c = or_R<IYL,T::CC_DD>(); NEXT; }
		case 0xb6: { int c = or_xix<IY>(); NEXT; }
		case 0xbc: { int c = cp_R<IYH,T::CC_DD>(); NEXT; }
		case 0xbd: { int c = cp_R<IYL,T::CC_DD>(); NEXT; }
		case 0xbe: { int c = cp_xix<IY>(); NEXT; }

		case 0xe1: { int c = pop_SS <IY,T::CC_DD>(); NEXT; }
		case 0xe5: { int c = push_SS<IY,T::CC_DD>(); NEXT; }
		case 0xe3: { int c = ex_xsp_SS<IY,T::CC_DD>(); NEXT; }
		case 0xe9: { int c = jp_SS<IY,T::CC_DD>(); NEXT; }
		case 0xf9: { int c = ld_sp_SS<IY,T::CC_DD>(); NEXT; }
		case 0xcb: ixy = R.getIY(); goto xx_cb;
		case 0xdd: T::add(T::CC_DD); goto opDD_2;
		case 0xfd: T::add(T::CC_DD); goto opFD_2;
		default: UNREACHABLE; return;
	}
}
#ifndef USE_COMPUTED_GOTO
	default: UNREACHABLE; return;
}
#endif

xx_cb: {
		unsigned tmp = RD_WORD_PC(T::CC_DD + T::CC_DD_CB);
		offset ofst = tmp & 0xFF;
		unsigned addr = (ixy + ofst) & 0xFFFF;
		byte xxcb_opcode = tmp >> 8;
		switch (xxcb_opcode) {
			case 0x00: { int c = rlc_xix_R<B>(addr); NEXT; }
			case 0x01: { int c = rlc_xix_R<C>(addr); NEXT; }
			case 0x02: { int c = rlc_xix_R<D>(addr); NEXT; }
			case 0x03: { int c = rlc_xix_R<E>(addr); NEXT; }
			case 0x04: { int c = rlc_xix_R<H>(addr); NEXT; }
			case 0x05: { int c = rlc_xix_R<L>(addr); NEXT; }
			case 0x06: { int c = rlc_xix_R<DUMMY>(addr); NEXT; }
			case 0x07: { int c = rlc_xix_R<A>(addr); NEXT; }
			case 0x08: { int c = rrc_xix_R<B>(addr); NEXT; }
			case 0x09: { int c = rrc_xix_R<C>(addr); NEXT; }
			case 0x0a: { int c = rrc_xix_R<D>(addr); NEXT; }
			case 0x0b: { int c = rrc_xix_R<E>(addr); NEXT; }
			case 0x0c: { int c = rrc_xix_R<H>(addr); NEXT; }
			case 0x0d: { int c = rrc_xix_R<L>(addr); NEXT; }
			case 0x0e: { int c = rrc_xix_R<DUMMY>(addr); NEXT; }
			case 0x0f: { int c = rrc_xix_R<A>(addr); NEXT; }
			case 0x10: { int c = rl_xix_R<B>(addr); NEXT; }
			case 0x11: { int c = rl_xix_R<C>(addr); NEXT; }
			case 0x12: { int c = rl_xix_R<D>(addr); NEXT; }
			case 0x13: { int c = rl_xix_R<E>(addr); NEXT; }
			case 0x14: { int c = rl_xix_R<H>(addr); NEXT; }
			case 0x15: { int c = rl_xix_R<L>(addr); NEXT; }
			case 0x16: { int c = rl_xix_R<DUMMY>(addr); NEXT; }
			case 0x17: { int c = rl_xix_R<A>(addr); NEXT; }
			case 0x18: { int c = rr_xix_R<B>(addr); NEXT; }
			case 0x19: { int c = rr_xix_R<C>(addr); NEXT; }
			case 0x1a: { int c = rr_xix_R<D>(addr); NEXT; }
			case 0x1b: { int c = rr_xix_R<E>(addr); NEXT; }
			case 0x1c: { int c = rr_xix_R<H>(addr); NEXT; }
			case 0x1d: { int c = rr_xix_R<L>(addr); NEXT; }
			case 0x1e: { int c = rr_xix_R<DUMMY>(addr); NEXT; }
			case 0x1f: { int c = rr_xix_R<A>(addr); NEXT; }
			case 0x20: { int c = sla_xix_R<B>(addr); NEXT; }
			case 0x21: { int c = sla_xix_R<C>(addr); NEXT; }
			case 0x22: { int c = sla_xix_R<D>(addr); NEXT; }
			case 0x23: { int c = sla_xix_R<E>(addr); NEXT; }
			case 0x24: { int c = sla_xix_R<H>(addr); NEXT; }
			case 0x25: { int c = sla_xix_R<L>(addr); NEXT; }
			case 0x26: { int c = sla_xix_R<DUMMY>(addr); NEXT; }
			case 0x27: { int c = sla_xix_R<A>(addr); NEXT; }
			case 0x28: { int c = sra_xix_R<B>(addr); NEXT; }
			case 0x29: { int c = sra_xix_R<C>(addr); NEXT; }
			case 0x2a: { int c = sra_xix_R<D>(addr); NEXT; }
			case 0x2b: { int c = sra_xix_R<E>(addr); NEXT; }
			case 0x2c: { int c = sra_xix_R<H>(addr); NEXT; }
			case 0x2d: { int c = sra_xix_R<L>(addr); NEXT; }
			case 0x2e: { int c = sra_xix_R<DUMMY>(addr); NEXT; }
			case 0x2f: { int c = sra_xix_R<A>(addr); NEXT; }
			case 0x30: { int c = T::isR800() ? sll2() : sll_xix_R<B>(addr); NEXT; }
			case 0x31: { int c = T::isR800() ? sll2() : sll_xix_R<C>(addr); NEXT; }
			case 0x32: { int c = T::isR800() ? sll2() : sll_xix_R<D>(addr); NEXT; }
			case 0x33: { int c = T::isR800() ? sll2() : sll_xix_R<E>(addr); NEXT; }
			case 0x34: { int c = T::isR800() ? sll2() : sll_xix_R<H>(addr); NEXT; }
			case 0x35: { int c = T::isR800() ? sll2() : sll_xix_R<L>(addr); NEXT; }
			case 0x36: { int c = T::isR800() ? sll2() : sll_xix_R<DUMMY>(addr); NEXT; }
			case 0x37: { int c = T::isR800() ? sll2() : sll_xix_R<A>(addr); NEXT; }
			case 0x38: { int c = srl_xix_R<B>(addr); NEXT; }
			case 0x39: { int c = srl_xix_R<C>(addr); NEXT; }
			case 0x3a: { int c = srl_xix_R<D>(addr); NEXT; }
			case 0x3b: { int c = srl_xix_R<E>(addr); NEXT; }
			case 0x3c: { int c = srl_xix_R<H>(addr); NEXT; }
			case 0x3d: { int c = srl_xix_R<L>(addr); NEXT; }
			case 0x3e: { int c = srl_xix_R<DUMMY>(addr); NEXT; }
			case 0x3f: { int c = srl_xix_R<A>(addr); NEXT; }

			case 0x40: case 0x41: case 0x42: case 0x43:
			case 0x44: case 0x45: case 0x46: case 0x47:
				{ int c = bit_N_xix<0>(addr); NEXT; }
			case 0x48: case 0x49: case 0x4a: case 0x4b:
			case 0x4c: case 0x4d: case 0x4e: case 0x4f:
				{ int c = bit_N_xix<1>(addr); NEXT; }
			case 0x50: case 0x51: case 0x52: case 0x53:
			case 0x54: case 0x55: case 0x56: case 0x57:
				{ int c = bit_N_xix<2>(addr); NEXT; }
			case 0x58: case 0x59: case 0x5a: case 0x5b:
			case 0x5c: case 0x5d: case 0x5e: case 0x5f:
				{ int c = bit_N_xix<3>(addr); NEXT; }
			case 0x60: case 0x61: case 0x62: case 0x63:
			case 0x64: case 0x65: case 0x66: case 0x67:
				{ int c = bit_N_xix<4>(addr); NEXT; }
			case 0x68: case 0x69: case 0x6a: case 0x6b:
			case 0x6c: case 0x6d: case 0x6e: case 0x6f:
				{ int c = bit_N_xix<5>(addr); NEXT; }
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
				{ int c = bit_N_xix<6>(addr); NEXT; }
			case 0x78: case 0x79: case 0x7a: case 0x7b:
			case 0x7c: case 0x7d: case 0x7e: case 0x7f:
				{ int c = bit_N_xix<7>(addr); NEXT; }

			case 0x80: { int c = res_N_xix_R<0,B>(addr); NEXT; }
			case 0x81: { int c = res_N_xix_R<0,C>(addr); NEXT; }
			case 0x82: { int c = res_N_xix_R<0,D>(addr); NEXT; }
			case 0x83: { int c = res_N_xix_R<0,E>(addr); NEXT; }
			case 0x84: { int c = res_N_xix_R<0,H>(addr); NEXT; }
			case 0x85: { int c = res_N_xix_R<0,L>(addr); NEXT; }
			case 0x87: { int c = res_N_xix_R<0,A>(addr); NEXT; }
			case 0x88: { int c = res_N_xix_R<1,B>(addr); NEXT; }
			case 0x89: { int c = res_N_xix_R<1,C>(addr); NEXT; }
			case 0x8a: { int c = res_N_xix_R<1,D>(addr); NEXT; }
			case 0x8b: { int c = res_N_xix_R<1,E>(addr); NEXT; }
			case 0x8c: { int c = res_N_xix_R<1,H>(addr); NEXT; }
			case 0x8d: { int c = res_N_xix_R<1,L>(addr); NEXT; }
			case 0x8f: { int c = res_N_xix_R<1,A>(addr); NEXT; }
			case 0x90: { int c = res_N_xix_R<2,B>(addr); NEXT; }
			case 0x91: { int c = res_N_xix_R<2,C>(addr); NEXT; }
			case 0x92: { int c = res_N_xix_R<2,D>(addr); NEXT; }
			case 0x93: { int c = res_N_xix_R<2,E>(addr); NEXT; }
			case 0x94: { int c = res_N_xix_R<2,H>(addr); NEXT; }
			case 0x95: { int c = res_N_xix_R<2,L>(addr); NEXT; }
			case 0x97: { int c = res_N_xix_R<2,A>(addr); NEXT; }
			case 0x98: { int c = res_N_xix_R<3,B>(addr); NEXT; }
			case 0x99: { int c = res_N_xix_R<3,C>(addr); NEXT; }
			case 0x9a: { int c = res_N_xix_R<3,D>(addr); NEXT; }
			case 0x9b: { int c = res_N_xix_R<3,E>(addr); NEXT; }
			case 0x9c: { int c = res_N_xix_R<3,H>(addr); NEXT; }
			case 0x9d: { int c = res_N_xix_R<3,L>(addr); NEXT; }
			case 0x9f: { int c = res_N_xix_R<3,A>(addr); NEXT; }
			case 0xa0: { int c = res_N_xix_R<4,B>(addr); NEXT; }
			case 0xa1: { int c = res_N_xix_R<4,C>(addr); NEXT; }
			case 0xa2: { int c = res_N_xix_R<4,D>(addr); NEXT; }
			case 0xa3: { int c = res_N_xix_R<4,E>(addr); NEXT; }
			case 0xa4: { int c = res_N_xix_R<4,H>(addr); NEXT; }
			case 0xa5: { int c = res_N_xix_R<4,L>(addr); NEXT; }
			case 0xa7: { int c = res_N_xix_R<4,A>(addr); NEXT; }
			case 0xa8: { int c = res_N_xix_R<5,B>(addr); NEXT; }
			case 0xa9: { int c = res_N_xix_R<5,C>(addr); NEXT; }
			case 0xaa: { int c = res_N_xix_R<5,D>(addr); NEXT; }
			case 0xab: { int c = res_N_xix_R<5,E>(addr); NEXT; }
			case 0xac: { int c = res_N_xix_R<5,H>(addr); NEXT; }
			case 0xad: { int c = res_N_xix_R<5,L>(addr); NEXT; }
			case 0xaf: { int c = res_N_xix_R<5,A>(addr); NEXT; }
			case 0xb0: { int c = res_N_xix_R<6,B>(addr); NEXT; }
			case 0xb1: { int c = res_N_xix_R<6,C>(addr); NEXT; }
			case 0xb2: { int c = res_N_xix_R<6,D>(addr); NEXT; }
			case 0xb3: { int c = res_N_xix_R<6,E>(addr); NEXT; }
			case 0xb4: { int c = res_N_xix_R<6,H>(addr); NEXT; }
			case 0xb5: { int c = res_N_xix_R<6,L>(addr); NEXT; }
			case 0xb7: { int c = res_N_xix_R<6,A>(addr); NEXT; }
			case 0xb8: { int c = res_N_xix_R<7,B>(addr); NEXT; }
			case 0xb9: { int c = res_N_xix_R<7,C>(addr); NEXT; }
			case 0xba: { int c = res_N_xix_R<7,D>(addr); NEXT; }
			case 0xbb: { int c = res_N_xix_R<7,E>(addr); NEXT; }
			case 0xbc: { int c = res_N_xix_R<7,H>(addr); NEXT; }
			case 0xbd: { int c = res_N_xix_R<7,L>(addr); NEXT; }
			case 0xbf: { int c = res_N_xix_R<7,A>(addr); NEXT; }
			case 0x86: { int c = res_N_xix_R<0,DUMMY>(addr); NEXT; }
			case 0x8e: { int c = res_N_xix_R<1,DUMMY>(addr); NEXT; }
			case 0x96: { int c = res_N_xix_R<2,DUMMY>(addr); NEXT; }
			case 0x9e: { int c = res_N_xix_R<3,DUMMY>(addr); NEXT; }
			case 0xa6: { int c = res_N_xix_R<4,DUMMY>(addr); NEXT; }
			case 0xae: { int c = res_N_xix_R<5,DUMMY>(addr); NEXT; }
			case 0xb6: { int c = res_N_xix_R<6,DUMMY>(addr); NEXT; }
			case 0xbe: { int c = res_N_xix_R<7,DUMMY>(addr); NEXT; }

			case 0xc0: { int c = set_N_xix_R<0,B>(addr); NEXT; }
			case 0xc1: { int c = set_N_xix_R<0,C>(addr); NEXT; }
			case 0xc2: { int c = set_N_xix_R<0,D>(addr); NEXT; }
			case 0xc3: { int c = set_N_xix_R<0,E>(addr); NEXT; }
			case 0xc4: { int c = set_N_xix_R<0,H>(addr); NEXT; }
			case 0xc5: { int c = set_N_xix_R<0,L>(addr); NEXT; }
			case 0xc7: { int c = set_N_xix_R<0,A>(addr); NEXT; }
			case 0xc8: { int c = set_N_xix_R<1,B>(addr); NEXT; }
			case 0xc9: { int c = set_N_xix_R<1,C>(addr); NEXT; }
			case 0xca: { int c = set_N_xix_R<1,D>(addr); NEXT; }
			case 0xcb: { int c = set_N_xix_R<1,E>(addr); NEXT; }
			case 0xcc: { int c = set_N_xix_R<1,H>(addr); NEXT; }
			case 0xcd: { int c = set_N_xix_R<1,L>(addr); NEXT; }
			case 0xcf: { int c = set_N_xix_R<1,A>(addr); NEXT; }
			case 0xd0: { int c = set_N_xix_R<2,B>(addr); NEXT; }
			case 0xd1: { int c = set_N_xix_R<2,C>(addr); NEXT; }
			case 0xd2: { int c = set_N_xix_R<2,D>(addr); NEXT; }
			case 0xd3: { int c = set_N_xix_R<2,E>(addr); NEXT; }
			case 0xd4: { int c = set_N_xix_R<2,H>(addr); NEXT; }
			case 0xd5: { int c = set_N_xix_R<2,L>(addr); NEXT; }
			case 0xd7: { int c = set_N_xix_R<2,A>(addr); NEXT; }
			case 0xd8: { int c = set_N_xix_R<3,B>(addr); NEXT; }
			case 0xd9: { int c = set_N_xix_R<3,C>(addr); NEXT; }
			case 0xda: { int c = set_N_xix_R<3,D>(addr); NEXT; }
			case 0xdb: { int c = set_N_xix_R<3,E>(addr); NEXT; }
			case 0xdc: { int c = set_N_xix_R<3,H>(addr); NEXT; }
			case 0xdd: { int c = set_N_xix_R<3,L>(addr); NEXT; }
			case 0xdf: { int c = set_N_xix_R<3,A>(addr); NEXT; }
			case 0xe0: { int c = set_N_xix_R<4,B>(addr); NEXT; }
			case 0xe1: { int c = set_N_xix_R<4,C>(addr); NEXT; }
			case 0xe2: { int c = set_N_xix_R<4,D>(addr); NEXT; }
			case 0xe3: { int c = set_N_xix_R<4,E>(addr); NEXT; }
			case 0xe4: { int c = set_N_xix_R<4,H>(addr); NEXT; }
			case 0xe5: { int c = set_N_xix_R<4,L>(addr); NEXT; }
			case 0xe7: { int c = set_N_xix_R<4,A>(addr); NEXT; }
			case 0xe8: { int c = set_N_xix_R<5,B>(addr); NEXT; }
			case 0xe9: { int c = set_N_xix_R<5,C>(addr); NEXT; }
			case 0xea: { int c = set_N_xix_R<5,D>(addr); NEXT; }
			case 0xeb: { int c = set_N_xix_R<5,E>(addr); NEXT; }
			case 0xec: { int c = set_N_xix_R<5,H>(addr); NEXT; }
			case 0xed: { int c = set_N_xix_R<5,L>(addr); NEXT; }
			case 0xef: { int c = set_N_xix_R<5,A>(addr); NEXT; }
			case 0xf0: { int c = set_N_xix_R<6,B>(addr); NEXT; }
			case 0xf1: { int c = set_N_xix_R<6,C>(addr); NEXT; }
			case 0xf2: { int c = set_N_xix_R<6,D>(addr); NEXT; }
			case 0xf3: { int c = set_N_xix_R<6,E>(addr); NEXT; }
			case 0xf4: { int c = set_N_xix_R<6,H>(addr); NEXT; }
			case 0xf5: { int c = set_N_xix_R<6,L>(addr); NEXT; }
			case 0xf7: { int c = set_N_xix_R<6,A>(addr); NEXT; }
			case 0xf8: { int c = set_N_xix_R<7,B>(addr); NEXT; }
			case 0xf9: { int c = set_N_xix_R<7,C>(addr); NEXT; }
			case 0xfa: { int c = set_N_xix_R<7,D>(addr); NEXT; }
			case 0xfb: { int c = set_N_xix_R<7,E>(addr); NEXT; }
			case 0xfc: { int c = set_N_xix_R<7,H>(addr); NEXT; }
			case 0xfd: { int c = set_N_xix_R<7,L>(addr); NEXT; }
			case 0xff: { int c = set_N_xix_R<7,A>(addr); NEXT; }
			case 0xc6: { int c = set_N_xix_R<0,DUMMY>(addr); NEXT; }
			case 0xce: { int c = set_N_xix_R<1,DUMMY>(addr); NEXT; }
			case 0xd6: { int c = set_N_xix_R<2,DUMMY>(addr); NEXT; }
			case 0xde: { int c = set_N_xix_R<3,DUMMY>(addr); NEXT; }
			case 0xe6: { int c = set_N_xix_R<4,DUMMY>(addr); NEXT; }
			case 0xee: { int c = set_N_xix_R<5,DUMMY>(addr); NEXT; }
			case 0xf6: { int c = set_N_xix_R<6,DUMMY>(addr); NEXT; }
			case 0xfe: { int c = set_N_xix_R<7,DUMMY>(addr); NEXT; }
			default: UNREACHABLE;
		}
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

template <class T> void CPUCore<T>::executeSlow()
{
	if (unlikely(false && nmiEdge)) {
		// Note: NMIs are disabled, see also raiseNMI()
		nmiEdge = false;
		nmi(); // NMI occured
	} else if (unlikely(IRQStatus && R.getIFF1() && !R.getAfterEI())) {
		// normal interrupt
		if (unlikely(R.getAfterLDAI())) {
			// HACK!!!
			// The 'ld a,i' or 'ld a,r' instruction copies the IFF2
			// bit to the V flag. Though when the Z80 accepts an
			// IRQ directly after this instruction, the V flag is 0
			// (instead of the expected value 1). This can probably
			// be explained if you look at the pipeline of the Z80.
			// But for speed reasons we implement it here as a
			// fix-up (a hack) in the IRQ routine. This behaviour
			// is actually a bug in the Z80.
			// Thanks to n_n for reporting this behaviour. I think
			// this was discovered by GuyveR800. Also thanks to
			// n_n for writing a test program that demonstrates
			// this quirk.
			// I also wrote a test program that demonstrates this
			// behaviour is the same whether 'ld a,i' is preceded
			// by a 'ei' instruction or not (so it's not caused by
			// the 'delayed IRQ acceptance of ei').
			assert(R.getF() & V_FLAG);
			R.setF(R.getF() & ~V_FLAG);
		}
		IRQAccept.signal();
		switch (R.getIM()) {
			case 0: irq0();
				break;
			case 1: irq1();
				break;
			case 2: irq2();
				break;
			default:
				UNREACHABLE;
		}
	} else if (unlikely(R.getHALT())) {
		// in halt mode
		R.incR(T::advanceHalt(T::haltStates(), scheduler.getNext()));
		setSlowInstructions();
	} else {
		assert(R.isSameAfter());
		R.clearNextAfter();
		cpuTracePre();
		assert(T::limitReached()); // we want only one instruction
		executeInstructions();
		cpuTracePost();
		R.copyNextAfter();
	}
}

template <class T> void CPUCore<T>::execute(bool fastForward)
{
	// In fast-forward mode, breakpoints, watchpoints or debug condtions
	// won't trigger. It is possible we already are in break mode, but
	// break is ignored in fast-forward mode.
	assert(fastForward || !interface->isBreaked());
	if (fastForward) {
		interface->setFastForward(true);
	}
	execute2(fastForward);
	interface->setFastForward(false);
}

template <class T> void CPUCore<T>::execute2(bool fastForward)
{
	// note: Don't use getTimeFast() here, because 'once in a while' we
	//       need to CPUClock::sync() to avoid overflow.
	//       Should be done at least once per second (approx). So only
	//       once in this method is enough.
	scheduler.schedule(T::getTime());
	setSlowInstructions();

	if (!fastForward && (interface->isContinue() || interface->isStep())) {
		// at least one instruction
		interface->setContinue(false);
		executeSlow();
		scheduler.schedule(T::getTimeFast());
		--slowInstructions;
		if (interface->isStep()) {
			interface->setStep(false);
			interface->doBreak();
			return;
		}
	}

	// Note: we call scheduler _after_ executing the instruction and before
	// deciding between executeFast() and executeSlow() (because a
	// SyncPoint could set an IRQ and then we must choose executeSlow())
	if (fastForward ||
	    (!interface->anyBreakPoints() && !traceSetting.getValue())) {
		// fast path, no breakpoints, no tracing
		while (!needExitCPULoop()) {
			if (slowInstructions) {
				--slowInstructions;
				executeSlow();
				scheduler.schedule(T::getTimeFast());
			} else {
				while (slowInstructions == 0) {
					T::enableLimit(true); // does CPUClock::sync()
					if (likely(!T::limitReached())) {
						// multiple instructions
						assert(R.isSameAfter());
						executeInstructions();
						assert(R.isSameAfter());
					}
					scheduler.schedule(T::getTimeFast());
					if (needExitCPULoop()) return;
				}
			}
		}
	} else {
		while (!needExitCPULoop()) {
			if (interface->checkBreakPoints(R.getPC())) {
				assert(interface->isBreaked());
				break;
			}
			if (slowInstructions == 0) {
				cpuTracePre();
				assert(T::limitReached()); // only one instruction
				assert(R.isSameAfter());
				executeInstructions();
				assert(R.isSameAfter());
				cpuTracePost();
			} else {
				--slowInstructions;
				executeSlow();
			}
			// Don't use getTimeFast() here, we need a call to
			// CPUClock::sync() 'once in a while'. (During a
			// reverse fast-forward this wasn't always the case).
			scheduler.schedule(T::getTime());
		}
	}
}


// LD r,r
template <class T> template<CPU::Reg8 DST, CPU::Reg8 SRC, int EE> int CPUCore<T>::ld_R_R() {
	R.set8<DST>(R.get8<SRC>()); return T::CC_LD_R_R + EE;
}

// LD SP,ss
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::ld_sp_SS() {
	R.setSP(R.get16<REG>()); return T::CC_LD_SP_HL + EE;
}

// LD (ss),a
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_SS_a() {
	T::setMemPtr((R.getA() << 8) | ((R.get16<REG>() + 1) & 0xFF));
	WRMEM(R.get16<REG>(), R.getA(), T::CC_LD_SS_A_1);
	return T::CC_LD_SS_A;
}

// LD (HL),r
template <class T> template<CPU::Reg8 SRC> int CPUCore<T>::ld_xhl_R() {
	WRMEM(R.getHL(), R.get8<SRC>(), T::CC_LD_HL_R_1);
	return T::CC_LD_HL_R;
}

// LD (IXY+e),r
template <class T> template<CPU::Reg16 IXY, CPU::Reg8 SRC> int CPUCore<T>::ld_xix_R() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_LD_XIX_R_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	WRMEM(addr, R.get8<SRC>(), T::CC_DD + T::CC_LD_XIX_R_2);
	return T::CC_DD + T::CC_LD_XIX_R;
}

// LD (HL),n
template <class T> int CPUCore<T>::ld_xhl_byte() {
	byte val = RDMEM_OPCODE(T::CC_LD_HL_N_1);
	WRMEM(R.getHL(), val, T::CC_LD_HL_N_2);
	return T::CC_LD_HL_N;
}

// LD (IXY+e),n
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::ld_xix_byte() {
	unsigned tmp = RD_WORD_PC(T::CC_DD + T::CC_LD_XIX_N_1);
	offset ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	WRMEM(addr, val, T::CC_DD + T::CC_LD_XIX_N_2);
	return T::CC_DD + T::CC_LD_XIX_N;
}

// LD (nn),A
template <class T> int CPUCore<T>::ld_xbyte_a() {
	unsigned x = RD_WORD_PC(T::CC_LD_NN_A_1);
	T::setMemPtr((R.getA() << 8) | ((x + 1) & 0xFF));
	WRMEM(x, R.getA(), T::CC_LD_NN_A_2);
	return T::CC_LD_NN_A;
}

// LD (nn),ss
template <class T> template<int EE> inline int CPUCore<T>::WR_NN_Y(unsigned reg) {
	unsigned addr = RD_WORD_PC(T::CC_LD_XX_HL_1 + EE);
	T::setMemPtr(addr + 1);
	WR_WORD(addr, reg, T::CC_LD_XX_HL_2 + EE);
	return T::CC_LD_XX_HL + EE;
}
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::ld_xword_SS() {
	return WR_NN_Y<EE      >(R.get16<REG>());
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_xword_SS_ED() {
	return WR_NN_Y<T::EE_ED>(R.get16<REG>());
}

// LD A,(ss)
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_a_SS() {
	T::setMemPtr(R.get16<REG>() + 1);
	R.setA(RDMEM(R.get16<REG>(), T::CC_LD_A_SS_1));
	return T::CC_LD_A_SS;
}

// LD A,(nn)
template <class T> int CPUCore<T>::ld_a_xbyte() {
	unsigned addr = RD_WORD_PC(T::CC_LD_A_NN_1);
	T::setMemPtr(addr + 1);
	R.setA(RDMEM(addr, T::CC_LD_A_NN_2));
	return T::CC_LD_A_NN;
}

// LD r,n
template <class T> template<CPU::Reg8 DST, int EE> int CPUCore<T>::ld_R_byte() {
	R.set8<DST>(RDMEM_OPCODE(T::CC_LD_R_N_1 + EE)); return T::CC_LD_R_N + EE;
}

// LD r,(hl)
template <class T> template<CPU::Reg8 DST> int CPUCore<T>::ld_R_xhl() {
	R.set8<DST>(RDMEM(R.getHL(), T::CC_LD_R_HL_1)); return T::CC_LD_R_HL;
}

// LD r,(IXY+e)
template <class T> template<CPU::Reg8 DST, CPU::Reg16 IXY> int CPUCore<T>::ld_R_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_LD_R_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	R.set8<DST>(RDMEM(addr, T::CC_DD + T::CC_LD_R_XIX_2));
	return T::CC_DD + T::CC_LD_R_XIX;
}

// LD ss,(nn)
template <class T> template<int EE> inline unsigned CPUCore<T>::RD_P_XX() {
	unsigned addr = RD_WORD_PC(T::CC_LD_HL_XX_1 + EE);
	T::setMemPtr(addr + 1);
	unsigned result = RD_WORD(addr, T::CC_LD_HL_XX_2 + EE);
	return result;
}
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::ld_SS_xword() {
	R.set16<REG>(RD_P_XX<EE>());       return T::CC_LD_HL_XX + EE;
}
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::ld_SS_xword_ED() {
	R.set16<REG>(RD_P_XX<T::EE_ED>()); return T::CC_LD_HL_XX + T::EE_ED;
}

// LD ss,nn
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::ld_SS_word() {
	R.set16<REG>(RD_WORD_PC(T::CC_LD_SS_NN_1 + EE)); return T::CC_LD_SS_NN + EE;
}


// ADC A,r
template <class T> inline void CPUCore<T>::ADC(byte reg) {
	unsigned res = R.getA() + reg + ((R.getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         ((R.getA() ^ res ^ reg) & H_FLAG) |
	         (((R.getA() ^ res) & (reg ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= ZSTable[res & 0xFF];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSXYTable[res & 0xFF];
	}
	R.setF(f);
	R.setA(res);
}
template <class T> inline int CPUCore<T>::adc_a_a() {
	unsigned res = 2 * R.getA() + ((R.getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         (res & H_FLAG) |
	         (((R.getA() ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= ZSTable[res & 0xFF];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSXYTable[res & 0xFF];
	}
	R.setF(f);
	R.setA(res);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::adc_a_R() {
	ADC(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::adc_a_byte() {
	ADC(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::adc_a_xhl() {
	ADC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::adc_a_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	ADC(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}

// ADD A,r
template <class T> inline void CPUCore<T>::ADD(byte reg) {
	unsigned res = R.getA() + reg;
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         ((R.getA() ^ res ^ reg) & H_FLAG) |
	         (((R.getA() ^ res) & (reg ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= ZSTable[res & 0xFF];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSXYTable[res & 0xFF];
	}
	R.setF(f);
	R.setA(res);
}
template <class T> inline int CPUCore<T>::add_a_a() {
	unsigned res = 2 * R.getA();
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         (res & H_FLAG) |
	         (((R.getA() ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= ZSTable[res & 0xFF];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSXYTable[res & 0xFF];
	}
	R.setF(f);
	R.setA(res);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::add_a_R() {
	ADD(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::add_a_byte() {
	ADD(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::add_a_xhl() {
	ADD(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::add_a_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	ADD(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}

// AND r
template <class T> inline void CPUCore<T>::AND(byte reg) {
	R.setA(R.getA() & reg);
	byte f = 0;
	if (T::isR800()) {
		f |= ZSPHTable[R.getA()];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[R.getA()] | H_FLAG;
	}
	R.setF(f);
}
template <class T> int CPUCore<T>::and_a() {
	byte f = 0;
	if (T::isR800()) {
		f |= ZSPHTable[R.getA()];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[R.getA()] | H_FLAG;
	}
	R.setF(f);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::and_R() {
	AND(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::and_byte() {
	AND(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::and_xhl() {
	AND(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::and_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	AND(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}

// CP r
template <class T> inline void CPUCore<T>::CP(byte reg) {
	unsigned q = R.getA() - reg;
	byte f = ZSTable[q & 0xFF] |
	         ((q & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((R.getA() ^ q ^ reg) & H_FLAG) |
	         (((reg ^ R.getA()) & (R.getA() ^ q) & 0x80) >> 5); // V_FLAG
	if (T::isR800()) {
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= reg & (X_FLAG | Y_FLAG); // XY from operand, not from result
	}
	R.setF(f);
}
template <class T> int CPUCore<T>::cp_a() {
	byte f = ZS0 | N_FLAG;
	if (T::isR800()) {
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= R.getA() & (X_FLAG | Y_FLAG); // XY from operand, not from result
	}
	R.setF(f);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::cp_R() {
	CP(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::cp_byte() {
	CP(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::cp_xhl() {
	CP(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::cp_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	CP(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}

// OR r
template <class T> inline void CPUCore<T>::OR(byte reg) {
	R.setA(R.getA() | reg);
	byte f = 0;
	if (T::isR800()) {
		f |= ZSPTable[R.getA()];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[R.getA()];
	}
	R.setF(f);
}
template <class T> int CPUCore<T>::or_a() {
	byte f = 0;
	if (T::isR800()) {
		f |= ZSPTable[R.getA()];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[R.getA()];
	}
	R.setF(f);
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::or_R() {
	OR(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::or_byte() {
	OR(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::or_xhl() {
	OR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::or_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	OR(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}

// SBC A,r
template <class T> inline void CPUCore<T>::SBC(byte reg) {
	unsigned res = R.getA() - reg - ((R.getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((R.getA() ^ res ^ reg) & H_FLAG) |
	         (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) >> 5); // V_FLAG
	if (T::isR800()) {
		f |= ZSTable[res & 0xFF];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSXYTable[res & 0xFF];
	}
	R.setF(f);
	R.setA(res);
}
template <class T> int CPUCore<T>::sbc_a_a() {
	if (T::isR800()) {
		word t = (R.getF() & C_FLAG)
		       ? (255 * 256 | ZS255 | C_FLAG | H_FLAG | N_FLAG)
		       : (  0 * 256 | ZS0   |                   N_FLAG);
		R.setAF(t | (R.getF() & (X_FLAG | Y_FLAG)));
	} else {
		R.setAF((R.getF() & C_FLAG) ?
			(255 * 256 | ZSXY255 | C_FLAG | H_FLAG | N_FLAG) :
			(  0 * 256 | ZSXY0   |                   N_FLAG));
	}
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::sbc_a_R() {
	SBC(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::sbc_a_byte() {
	SBC(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::sbc_a_xhl() {
	SBC(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::sbc_a_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	SBC(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}

// SUB r
template <class T> inline void CPUCore<T>::SUB(byte reg) {
	unsigned res = R.getA() - reg;
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((R.getA() ^ res ^ reg) & H_FLAG) |
	         (((reg ^ R.getA()) & (R.getA() ^ res) & 0x80) >> 5); // V_FLAG
	if (T::isR800()) {
		f |= ZSTable[res & 0xFF];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSXYTable[res & 0xFF];
	}
	R.setF(f);
	R.setA(res);
}
template <class T> int CPUCore<T>::sub_a() {
	if (T::isR800()) {
		word t = 0 * 256 | ZS0 | N_FLAG;
		R.setAF(t | (R.getF() & (X_FLAG | Y_FLAG)));
	} else {
		R.setAF(0 * 256 | ZSXY0 | N_FLAG);
	}
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::sub_R() {
	SUB(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::sub_byte() {
	SUB(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::sub_xhl() {
	SUB(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::sub_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	SUB(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}

// XOR r
template <class T> inline void CPUCore<T>::XOR(byte reg) {
	R.setA(R.getA() ^ reg);
	byte f = 0;
	if (T::isR800()) {
		f |= ZSPTable[R.getA()];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[R.getA()];
	}
	R.setF(f);
}
template <class T> int CPUCore<T>::xor_a() {
	if (T::isR800()) {
		word t = 0 * 256 + ZSP0;
		R.setAF(t | (R.getF() & (X_FLAG | Y_FLAG)));
	} else {
		R.setAF(0 * 256 + ZSPXY0);
	}
	return T::CC_CP_R;
}
template <class T> template<CPU::Reg8 SRC, int EE> int CPUCore<T>::xor_R() {
	XOR(R.get8<SRC>()); return T::CC_CP_R + EE;
}
template <class T> int CPUCore<T>::xor_byte() {
	XOR(RDMEM_OPCODE(T::CC_CP_N_1)); return T::CC_CP_N;
}
template <class T> int CPUCore<T>::xor_xhl() {
	XOR(RDMEM(R.getHL(), T::CC_CP_XHL_1)); return T::CC_CP_XHL;
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::xor_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	XOR(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return T::CC_DD + T::CC_CP_XIX;
}


// DEC r
template <class T> inline byte CPUCore<T>::DEC(byte reg) {
	byte res = reg - 1;
	byte f = ((reg & ~res & 0x80) >> 5) | // V_FLAG
	         (((res & 0x0F) + 1) & H_FLAG) |
	         N_FLAG;
	if (T::isR800()) {
		f |= R.getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= ZSTable[res];
	} else {
		f |= R.getF() & C_FLAG;
		f |= ZSXYTable[res];
	}
	R.setF(f);
	return res;
}
template <class T> template<CPU::Reg8 REG, int EE> int CPUCore<T>::dec_R() {
	R.set8<REG>(DEC(R.get8<REG>())); return T::CC_INC_R + EE;
}
template <class T> template<int EE> inline int CPUCore<T>::DEC_X(unsigned x) {
	byte val = DEC(RDMEM(x, T::CC_INC_XHL_1 + EE));
	WRMEM(x, val, T::CC_INC_XHL_2 + EE);
	return T::CC_INC_XHL + EE;
}
template <class T> int CPUCore<T>::dec_xhl() {
	return DEC_X<0>(R.getHL());
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::dec_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_INC_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	return DEC_X<T::CC_DD + T::EE_INC_XIX>(addr);
}

// INC r
template <class T> inline byte CPUCore<T>::INC(byte reg) {
	reg++;
	byte f = ((reg & -reg & 0x80) >> 5) | // V_FLAG
	         (((reg & 0x0F) - 1) & H_FLAG) |
		 0; // N_FLAG
	if (T::isR800()) {
		f |= R.getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= ZSTable[reg];
	} else {
		f |= R.getF() & C_FLAG;
		f |= ZSXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<CPU::Reg8 REG, int EE> int CPUCore<T>::inc_R() {
	R.set8<REG>(INC(R.get8<REG>())); return T::CC_INC_R + EE;
}
template <class T> template <int EE> inline int CPUCore<T>::INC_X(unsigned x) {
	byte val = INC(RDMEM(x, T::CC_INC_XHL_1 + EE));
	WRMEM(x, val, T::CC_INC_XHL_2 + EE);
	return T::CC_INC_XHL + EE;
}
template <class T> int CPUCore<T>::inc_xhl() {
	return INC_X<0>(R.getHL());
}
template <class T> template<CPU::Reg16 IXY> int CPUCore<T>::inc_xix() {
	offset ofst = RDMEM_OPCODE(T::CC_DD + T::CC_INC_XIX_1);
	unsigned addr = (R.get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	return INC_X<T::CC_DD + T::EE_INC_XIX>(addr);
}


// ADC HL,ss
template <class T> template<CPU::Reg16 REG> inline int CPUCore<T>::adc_hl_SS() {
	unsigned reg = R.get16<REG>();
	T::setMemPtr(R.getHL() + 1);
	unsigned res = R.getHL() + reg + ((R.getF() & C_FLAG) ? 1 : 0);
	byte f = (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= R.getF() & (X_FLAG | Y_FLAG);
	}
	if (res & 0xFFFF) {
		f |= ((R.getHL() ^ res ^ reg) >> 8) & H_FLAG;
		f |= 0; // Z_FLAG
		f |= ((R.getHL() ^ res) & (reg ^ res) & 0x8000) >> 13; // V_FLAG
		if (T::isR800()) {
			f |= (res >> 8) & S_FLAG;
		} else {
			f |= (res >> 8) & (S_FLAG | X_FLAG | Y_FLAG);
		}
	} else {
		f |= ((R.getHL() ^ reg) >> 8) & H_FLAG;
		f |= Z_FLAG;
		f |= (R.getHL() & reg & 0x8000) >> 13; // V_FLAG
		f |= 0; // S_FLAG (X_FLAG Y_FLAG)
	}
	R.setF(f);
	R.setHL(res);
	return T::CC_ADC_HL_SS;
}
template <class T> int CPUCore<T>::adc_hl_hl() {
	T::setMemPtr(R.getHL() + 1);
	unsigned res = 2 * R.getHL() + ((R.getF() & C_FLAG) ? 1 : 0);
	byte f = (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= R.getF() & (X_FLAG | Y_FLAG);
	}
	if (res & 0xFFFF) {
		f |= 0; // Z_FLAG
		f |= ((R.getHL() ^ res) & 0x8000) >> 13; // V_FLAG
		if (T::isR800()) {
			f |= (res >> 8) & (H_FLAG | S_FLAG);
		} else {
			f |= (res >> 8) & (H_FLAG | S_FLAG | X_FLAG | Y_FLAG);
		}
	} else {
		f |= Z_FLAG;
		f |= (R.getHL() & 0x8000) >> 13; // V_FLAG
		f |= 0; // H_FLAG S_FLAG (X_FLAG Y_FLAG)
	}
	R.setF(f);
	R.setHL(res);
	return T::CC_ADC_HL_SS;
}

// ADD HL/IX/IY,ss
template <class T> template<CPU::Reg16 REG1, CPU::Reg16 REG2, int EE> int CPUCore<T>::add_SS_TT() {
	unsigned reg1 = R.get16<REG1>();
	unsigned reg2 = R.get16<REG2>();
	T::setMemPtr(reg1 + 1);
	unsigned res = reg1 + reg2;
	byte f = (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	         (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | V_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | V_FLAG);
		f |= (res >> 8) & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	R.set16<REG1>(res & 0xFFFF);
	return T::CC_ADD_HL_SS + EE;
}
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::add_SS_SS() {
	unsigned reg = R.get16<REG>();
	T::setMemPtr(reg + 1);
	unsigned res = 2 * reg;
	byte f = (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | V_FLAG | X_FLAG | Y_FLAG);
		f |= (res >> 8) & H_FLAG;
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | V_FLAG);
		f |= (res >> 8) & (H_FLAG | X_FLAG | Y_FLAG);
	}
	R.setF(f);
	R.set16<REG>(res & 0xFFFF);
	return T::CC_ADD_HL_SS + EE;
}

// SBC HL,ss
template <class T> template<CPU::Reg16 REG> inline int CPUCore<T>::sbc_hl_SS() {
	unsigned reg = R.get16<REG>();
	T::setMemPtr(R.getHL() + 1);
	unsigned res = R.getHL() - reg - ((R.getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x10000) ? C_FLAG : 0) |
		 N_FLAG;
	if (T::isR800()) {
		f |= R.getF() & (X_FLAG | Y_FLAG);
	}
	if (res & 0xFFFF) {
		f |= ((R.getHL() ^ res ^ reg) >> 8) & H_FLAG;
		f |= 0; // Z_FLAG
		f |= ((reg ^ R.getHL()) & (R.getHL() ^ res) & 0x8000) >> 13; // V_FLAG
		if (T::isR800()) {
			f |= (res >> 8) & S_FLAG;
		} else {
			f |= (res >> 8) & (S_FLAG | X_FLAG | Y_FLAG);
		}
	} else {
		f |= ((R.getHL() ^ reg) >> 8) & H_FLAG;
		f |= Z_FLAG;
		f |= ((reg ^ R.getHL()) & R.getHL() & 0x8000) >> 13; // V_FLAG
		f |= 0; // S_FLAG (X_FLAG Y_FLAG)
	}
	R.setF(f);
	R.setHL(res);
	return T::CC_ADC_HL_SS;
}
template <class T> int CPUCore<T>::sbc_hl_hl() {
	T::setMemPtr(R.getHL() + 1);
	byte f = T::isR800() ? (R.getF() & (X_FLAG | Y_FLAG)) : 0;
	if (R.getF() & C_FLAG) {
		f |= C_FLAG | H_FLAG | S_FLAG | N_FLAG;
		if (!T::isR800()) {
			f |= X_FLAG | Y_FLAG;
		}
		R.setHL(0xFFFF);
	} else {
		f |= Z_FLAG | N_FLAG;
		R.setHL(0);
	}
	R.setF(f);
	return T::CC_ADC_HL_SS;
}

// DEC ss
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::dec_SS() {
	R.set16<REG>(R.get16<REG>() - 1); return T::CC_INC_SS + EE;
}

// INC ss
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::inc_SS() {
	R.set16<REG>(R.get16<REG>() + 1); return T::CC_INC_SS + EE;
}


// BIT n,r
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::bit_N_R() {
	byte reg = R.get8<REG>();
	byte f = 0; // N_FLAG
	if (T::isR800()) {
		// this is very different from Z80 (not only XY flags)
		f |= R.getF() & (S_FLAG | V_FLAG | C_FLAG | X_FLAG | Y_FLAG);
		f |= H_FLAG;
		f |= (reg & (1 << N)) ? 0 : Z_FLAG;
	} else {
		f |= ZSPHTable[reg & (1 << N)];
		f |= R.getF() & C_FLAG;
		f |= reg & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	return T::CC_BIT_R;
}
template <class T> template<unsigned N> inline int CPUCore<T>::bit_N_xhl() {
	byte m = RDMEM(R.getHL(), T::CC_BIT_XHL_1) & (1 << N);
	byte f = 0; // N_FLAG
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | V_FLAG | C_FLAG | X_FLAG | Y_FLAG);
		f |= H_FLAG;
		f |= m ? 0 : Z_FLAG;
	} else {
		f |= ZSPHTable[m];
		f |= R.getF() & C_FLAG;
		f |= (T::getMemPtr() >> 8) & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	return T::CC_BIT_XHL;
}
template <class T> template<unsigned N> inline int CPUCore<T>::bit_N_xix(unsigned addr) {
	T::setMemPtr(addr);
	byte m = RDMEM(addr, T::CC_DD + T::CC_BIT_XIX_1) & (1 << N);
	byte f = 0; // N_FLAG
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | V_FLAG | C_FLAG | X_FLAG | Y_FLAG);
		f |= H_FLAG;
		f |= m ? 0 : Z_FLAG;
	} else {
		f |= ZSPHTable[m];
		f |= R.getF() & C_FLAG;
		f |= (addr >> 8) & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	return T::CC_DD + T::CC_BIT_XIX;
}

// RES n,r
static inline byte RES(unsigned b, byte reg) {
	return reg & ~(1 << b);
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::res_N_R() {
	R.set8<REG>(RES(N, R.get8<REG>())); return T::CC_SET_R;
}
template <class T> template<int EE> inline byte CPUCore<T>::RES_X(unsigned bit, unsigned addr) {
	byte res = RES(bit, RDMEM(addr, T::CC_SET_XHL_1 + EE));
	WRMEM(addr, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<unsigned N> int CPUCore<T>::res_N_xhl() {
	RES_X<0>(N, R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::res_N_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(RES_X<T::CC_DD + T::EE_SET_XIX>(N, a));
	return T::CC_DD + T::CC_SET_XIX;
}

// SET n,r
static inline byte SET(unsigned b, byte reg) {
	return reg | (1 << b);
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::set_N_R() {
	R.set8<REG>(SET(N, R.get8<REG>())); return T::CC_SET_R;
}
template <class T> template<int EE> inline byte CPUCore<T>::SET_X(unsigned bit, unsigned addr) {
	byte res = SET(bit, RDMEM(addr, T::CC_SET_XHL_1 + EE));
	WRMEM(addr, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<unsigned N> int CPUCore<T>::set_N_xhl() {
	SET_X<0>(N, R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<unsigned N, CPU::Reg8 REG> int CPUCore<T>::set_N_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(SET_X<T::CC_DD + T::EE_SET_XIX>(N, a));
	return T::CC_DD + T::CC_SET_XIX;
}

// RL r
template <class T> inline byte CPUCore<T>::RL(byte reg) {
	byte c = reg >> 7;
	reg = (reg << 1) | ((R.getF() & C_FLAG) ? 0x01 : 0);
	byte f = c ? C_FLAG : 0;
	if (T::isR800()) {
		f |= ZSPTable[reg];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::RL_X(unsigned x) {
	byte res = RL(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rl_R() {
	R.set8<REG>(RL(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rl_xhl() {
	RL_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rl_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(RL_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}

// RLC r
template <class T> inline byte CPUCore<T>::RLC(byte reg) {
	byte c = reg >> 7;
	reg = (reg << 1) | c;
	byte f = c ? C_FLAG : 0;
	if (T::isR800()) {
		f |= ZSPTable[reg];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::RLC_X(unsigned x) {
	byte res = RLC(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rlc_R() {
	R.set8<REG>(RLC(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rlc_xhl() {
	RLC_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rlc_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(RLC_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}

// RR r
template <class T> inline byte CPUCore<T>::RR(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | ((R.getF() & C_FLAG) << 7);
	byte f = c ? C_FLAG : 0;
	if (T::isR800()) {
		f |= ZSPTable[reg];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::RR_X(unsigned x) {
	byte res = RR(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rr_R() {
	R.set8<REG>(RR(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rr_xhl() {
	RR_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rr_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(RR_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}

// RRC r
template <class T> inline byte CPUCore<T>::RRC(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | (c << 7);
	byte f = c ? C_FLAG : 0;
	if (T::isR800()) {
		f |= ZSPTable[reg];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::RRC_X(unsigned x) {
	byte res = RRC(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rrc_R() {
	R.set8<REG>(RRC(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::rrc_xhl() {
	RRC_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::rrc_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(RRC_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}

// SLA r
template <class T> inline byte CPUCore<T>::SLA(byte reg) {
	byte c = reg >> 7;
	reg <<= 1;
	byte f = c ? C_FLAG : 0;
	if (T::isR800()) {
		f |= ZSPTable[reg];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::SLA_X(unsigned x) {
	byte res = SLA(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sla_R() {
	R.set8<REG>(SLA(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::sla_xhl() {
	SLA_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sla_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(SLA_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}

// SLL r
template <class T> inline byte CPUCore<T>::SLL(byte reg) {
	assert(!T::isR800()); // this instruction is Z80-only
	byte c = reg >> 7;
	reg = (reg << 1) | 1;
	byte f = c ? C_FLAG : 0;
	f |= ZSPXYTable[reg];
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::SLL_X(unsigned x) {
	byte res = SLL(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sll_R() {
	R.set8<REG>(SLL(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::sll_xhl() {
	SLL_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sll_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(SLL_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}
template <class T> int CPUCore<T>::sll2() {
	assert(T::isR800()); // this instruction is R800-only
	byte f = (R.getF() & (X_FLAG | Y_FLAG)) |
	         (R.getA() >> 7) | // C_FLAG
	         0; // all other flags zero
	R.setF(f);
	return T::CC_DD + T::CC_SET_XIX; // TODO
}

// SRA r
template <class T> inline byte CPUCore<T>::SRA(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | (reg & 0x80);
	byte f = c ? C_FLAG : 0;
	if (T::isR800()) {
		f |= ZSPTable[reg];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::SRA_X(unsigned x) {
	byte res = SRA(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sra_R() {
	R.set8<REG>(SRA(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::sra_xhl() {
	SRA_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::sra_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(SRA_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}

// SRL R
template <class T> inline byte CPUCore<T>::SRL(byte reg) {
	byte c = reg & 1;
	reg >>= 1;
	byte f = c ? C_FLAG : 0;
	if (T::isR800()) {
		f |= ZSPTable[reg];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSPXYTable[reg];
	}
	R.setF(f);
	return reg;
}
template <class T> template<int EE> inline byte CPUCore<T>::SRL_X(unsigned x) {
	byte res = SRL(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::srl_R() {
	R.set8<REG>(SRL(R.get8<REG>())); return T::CC_SET_R;
}
template <class T> int CPUCore<T>::srl_xhl() {
	SRL_X<0>(R.getHL()); return T::CC_SET_XHL;
}
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::srl_xix_R(unsigned a) {
	T::setMemPtr(a);
	R.set8<REG>(SRL_X<T::CC_DD + T::EE_SET_XIX>(a));
	return T::CC_DD + T::CC_SET_XIX;
}

// RLA RLCA RRA RRCA
template <class T> int CPUCore<T>::rla() {
	byte c = R.getF() & C_FLAG;
	byte f = (R.getA() & 0x80) ? C_FLAG : 0;
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG);
	}
	R.setA((R.getA() << 1) | (c ? 1 : 0));
	if (!T::isR800()) {
		f |= R.getA() & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rlca() {
	R.setA((R.getA() << 1) | (R.getA() >> 7));
	byte f = 0;
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
		f |= R.getA() & C_FLAG;
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG);
		f |= R.getA() & (Y_FLAG | X_FLAG | C_FLAG);
	}
	R.setF(f);
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rra() {
	byte c = (R.getF() & C_FLAG) << 7;
	byte f = (R.getA() & 0x01) ? C_FLAG : 0;
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG);
	}
	R.setA((R.getA() >> 1) | c);
	if (!T::isR800()) {
		f |= R.getA() & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	return T::CC_RLA;
}
template <class T> int CPUCore<T>::rrca() {
	byte f = R.getA() & C_FLAG;
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG);
	}
	R.setA((R.getA() >> 1) | (R.getA() << 7));
	if (!T::isR800()) {
		f |= R.getA() & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	return T::CC_RLA;
}


// RLD
template <class T> int CPUCore<T>::rld() {
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	T::setMemPtr(R.getHL() + 1);
	WRMEM(R.getHL(), (val << 4) | (R.getA() & 0x0F), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val >> 4));
	byte f = 0;
	if (T::isR800()) {
		f |= R.getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= ZSPTable[R.getA()];
	} else {
		f |= R.getF() & C_FLAG;
		f |= ZSPXYTable[R.getA()];
	}
	R.setF(f);
	return T::CC_RLD;
}

// RRD
template <class T> int CPUCore<T>::rrd() {
	byte val = RDMEM(R.getHL(), T::CC_RLD_1);
	T::setMemPtr(R.getHL() + 1);
	WRMEM(R.getHL(), (val >> 4) | (R.getA() << 4), T::CC_RLD_2);
	R.setA((R.getA() & 0xF0) | (val & 0x0F));
	byte f = 0;
	if (T::isR800()) {
		f |= R.getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= ZSPTable[R.getA()];
	} else {
		f |= R.getF() & C_FLAG;
		f |= ZSPXYTable[R.getA()];
	}
	R.setF(f);
	return T::CC_RLD;
}


// PUSH ss
template <class T> template<int EE> inline void CPUCore<T>::PUSH(unsigned reg) {
	R.setSP(R.getSP() - 2);
	WR_WORD_rev<true, true>(R.getSP(), reg, T::CC_PUSH_1 + EE);
}
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::push_SS() {
	PUSH<EE>(R.get16<REG>()); return T::CC_PUSH + EE;
}

// POP ss
template <class T> template <int EE> inline unsigned CPUCore<T>::POP() {
	unsigned addr = R.getSP();
	R.setSP(addr + 2);
	return RD_WORD(addr, T::CC_POP_1 + EE);
}
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::pop_SS() {
	R.set16<REG>(POP<EE>()); return T::CC_POP + EE;
}


// CALL nn / CALL cc,nn
template <class T> template<typename COND> int CPUCore<T>::call(COND cond) {
	unsigned addr = RD_WORD_PC(T::CC_CALL_1);
	T::setMemPtr(addr);
	if (cond(R.getF())) {
		PUSH<T::EE_CALL>(R.getPC());
		R.setPC(addr);
		return T::CC_CALL_A;
	} else {
		return T::CC_CALL_B;
	}
}


// RST n
template <class T> template<unsigned ADDR> int CPUCore<T>::rst() {
	PUSH<0>(R.getPC());
	T::setMemPtr(ADDR);
	R.setPC(ADDR);
	return T::CC_RST;
}


// RET
template <class T> template<int EE, typename COND> inline int CPUCore<T>::RET(COND cond) {
	if (cond(R.getF())) {
		unsigned addr = POP<EE>();
		T::setMemPtr(addr);
		R.setPC(addr);
		return T::CC_RET_A + EE;
	} else {
		return T::CC_RET_B + EE;
	}
}
template <class T> template<typename COND> int CPUCore<T>::ret(COND cond) {
	return RET<T::EE_RET_C>(cond);
}
template <class T> int CPUCore<T>::ret() {
	return RET<0>(CondTrue());
}
template <class T> int CPUCore<T>::retn() { // also reti
	R.setIFF1(R.getIFF2());
	setSlowInstructions();
	return RET<T::EE_RETN>(CondTrue());
}


// JP ss
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::jp_SS() {
	R.setPC(R.get16<REG>()); T::R800ForcePageBreak(); return T::CC_JP_HL + EE;
}

// JP nn / JP cc,nn
template <class T> template<typename COND> int CPUCore<T>::jp(COND cond) {
	unsigned addr = RD_WORD_PC(T::CC_JP_1);
	T::setMemPtr(addr);
	if (cond(R.getF())) {
		R.setPC(addr);
		T::R800ForcePageBreak();
		return T::CC_JP_A;
	} else {
		return T::CC_JP_B;
	}
}

// JR e
template <class T> template<typename COND> int CPUCore<T>::jr(COND cond) {
	offset ofst = RDMEM_OPCODE(T::CC_JR_1);
	if (cond(R.getF())) {
		if ((R.getPC() & 0xFF) == 0) {
			// On R800, when this instruction is located in the
			// last two byte of a page (a page is a 256-byte
			// (aligned) memory block) and even if we jump back,
			// thus fetching the next opcode byte does not cause a
			// page-break, there still is one cycle overhead. It's
			// as-if there is a page-break.
			//
			// This could be explained by some (very limited)
			// pipeline behaviour in R800: it seems that the
			// decision to cause a page-break on the next
			// instruction is already made before the jump
			// destination address for the current instruction is
			// calculated (though a destination address in another
			// page is also a reason for a page-break).
			//
			// It's likely all instructions behave like this, but I
			// think we can get away with only explicitly emulating
			// this behaviour in the djnz and the jr (conditional
			// or not) instructions: all other instructions that
			// cause the PC to change in a non-incremental way do
			// already force a pagebreak for another reason, so
			// this effect is masked. Examples of such instructions
			// are: JP, RET, CALL, RST, all repeated block
			// instructions, accepting an IRQ, (are there more
			// instructions are events that change PC?)
			//
			// See doc/r800-djnz.txt for more details.
			T::R800ForcePageBreak();
		}
		R.setPC((R.getPC() + ofst) & 0xFFFF);
		T::setMemPtr(R.getPC());
		return T::CC_JR_A;
	} else {
		return T::CC_JR_B;
	}
}

// DJNZ e
template <class T> int CPUCore<T>::djnz() {
	byte b = R.getB() - 1;
	R.setB(b);
	offset ofst = RDMEM_OPCODE(T::CC_JR_1 + T::EE_DJNZ);
	if (b) {
		if ((R.getPC() & 0xFF) == 0) {
			// See comment in jr()
			T::R800ForcePageBreak();
		}
		R.setPC((R.getPC() + ofst) & 0xFFFF);
		T::setMemPtr(R.getPC());
		return T::CC_JR_A + T::EE_DJNZ;
	} else {
		return T::CC_JR_B + T::EE_DJNZ;
	}
}

// EX (SP),ss
template <class T> template<CPU::Reg16 REG, int EE> int CPUCore<T>::ex_xsp_SS() {
	unsigned res = RD_WORD_impl<true, false>(R.getSP(), T::CC_EX_SP_HL_1 + EE);
	T::setMemPtr(res);
	WR_WORD_rev<false, true>(R.getSP(), R.get16<REG>(), T::CC_EX_SP_HL_2 + EE);
	R.set16<REG>(res);
	return T::CC_EX_SP_HL + EE;
}

// IN r,(c)
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::in_R_c() {
	if (T::isR800()) T::waitForEvenCycle(T::CC_IN_R_C_1);
	T::setMemPtr(R.getBC() + 1);
	byte res = READ_PORT(R.getBC(), T::CC_IN_R_C_1);
	byte f = 0;
	if (T::isR800()) {
		f |= R.getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= ZSPTable[res];
	} else {
		f |= R.getF() & C_FLAG;
		f |= ZSPXYTable[res];
	}
	R.setF(f);
	R.set8<REG>(res);
	return T::CC_IN_R_C;
}

// IN a,(n)
template <class T> int CPUCore<T>::in_a_byte() {
	unsigned y = RDMEM_OPCODE(T::CC_IN_A_N_1) + 256 * R.getA();
	T::setMemPtr(y + 1);
	if (T::isR800()) T::waitForEvenCycle(T::CC_IN_A_N_2);
	R.setA(READ_PORT(y, T::CC_IN_A_N_2));
	return T::CC_IN_A_N;
}

// OUT (c),r
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::out_c_R() {
	if (T::isR800()) T::waitForEvenCycle(T::CC_OUT_C_R_1);
	T::setMemPtr(R.getBC() + 1);
	WRITE_PORT(R.getBC(), R.get8<REG>(), T::CC_OUT_C_R_1);
	return T::CC_OUT_C_R;
}
template <class T> int CPUCore<T>::out_c_0() {
	// TODO not on R800
	if (T::isR800()) T::waitForEvenCycle(T::CC_OUT_C_R_1);
	T::setMemPtr(R.getBC() + 1);
	byte out_c_x = isTurboR ? 255 : 0;
	WRITE_PORT(R.getBC(), out_c_x, T::CC_OUT_C_R_1);
	return T::CC_OUT_C_R;
}

// OUT (n),a
template <class T> int CPUCore<T>::out_byte_a() {
	byte port = RDMEM_OPCODE(T::CC_OUT_N_A_1);
	unsigned y = (R.getA() << 8) |   port;
	T::setMemPtr((R.getA() << 8) | ((port + 1) & 255));
	if (T::isR800()) T::waitForEvenCycle(T::CC_OUT_N_A_2);
	WRITE_PORT(y, R.getA(), T::CC_OUT_N_A_2);
	return T::CC_OUT_N_A;
}


// block CP
template <class T> inline int CPUCore<T>::BLOCK_CP(int increase, bool repeat) {
	T::setMemPtr(T::getMemPtr() + increase);
	byte val = RDMEM(R.getHL(), T::CC_CPI_1);
	byte res = R.getA() - val;
	R.setHL(R.getHL() + increase);
	R.setBC(R.getBC() - 1);
	byte f = ((R.getA() ^ val ^ res) & H_FLAG) |
	         ZSTable[res] |
	         N_FLAG |
	         (R.getBC() ? V_FLAG : 0);
	if (T::isR800()) {
		f |= R.getF() & (C_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= R.getF() & C_FLAG;
		unsigned k = res - ((f & H_FLAG) >> 4);
		f |= (k << 4) & Y_FLAG; // bit 1 -> flag 5
		f |= k & X_FLAG;        // bit 3 -> flag 3
	}
	R.setF(f);
	if (repeat && R.getBC() && res) {
		R.setPC(R.getPC() - 2);
		T::setMemPtr(R.getPC() + 1);
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
	byte f = R.getBC() ? V_FLAG : 0;
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | C_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | C_FLAG);
		f |= ((R.getA() + val) << 4) & Y_FLAG; // bit 1 -> flag 5
		f |= (R.getA() + val) & X_FLAG;        // bit 3 -> flag 3
	}
	R.setF(f);
	if (repeat && R.getBC()) {
		R.setPC(R.getPC() - 2);
		T::setMemPtr(R.getPC() + 1);
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
	// TODO R800 flags
	if (T::isR800()) T::waitForEvenCycle(T::CC_INI_1);
	T::setMemPtr(R.getBC() + increase);
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
	// TODO R800 flags
	byte val = RDMEM(R.getHL(), T::CC_OUTI_1);
	R.setHL(R.getHL() + increase);
	if (T::isR800()) T::waitForEvenCycle(T::CC_OUTI_2);
	WRITE_PORT(R.getBC(), val, T::CC_OUTI_2);
	R.setBC(R.getBC() - 0x100); // decr after use
	T::setMemPtr(R.getBC() + increase);
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
	byte f = 0;
	if (T::isR800()) {
		// H flag is different from Z80 (and as always XY flags as well)
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG | X_FLAG | Y_FLAG | H_FLAG);
	} else {
		f |= (R.getF() & C_FLAG) << 4; // H_FLAG
		// only set X(Y) flag (don't reset if already set)
		if (isTurboR) {
			// Y flag is not changed on a turboR-Z80
			f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG | Y_FLAG);
			f |= (R.getF() | R.getA()) & X_FLAG;
		} else {
			f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG);
			f |= (R.getF() | R.getA()) & (X_FLAG | Y_FLAG);
		}
	}
	f ^= C_FLAG;
	R.setF(f);
	return T::CC_CCF;
}
template <class T> int CPUCore<T>::cpl() {
	R.setA(R.getA() ^ 0xFF);
	byte f = H_FLAG | N_FLAG;
	if (T::isR800()) {
		f |= R.getF();
	} else {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG);
		f |= R.getA() & (X_FLAG | Y_FLAG);
	}
	R.setF(f);
	return T::CC_CPL;
}
template <class T> int CPUCore<T>::daa() {
	byte a = R.getA();
	byte f = R.getF();
	byte adjust = 0;
	if ((f & H_FLAG) || ((R.getA() & 0xf) > 9)) adjust += 6;
	if ((f & C_FLAG) || (R.getA() > 0x99)) adjust += 0x60;
	if (f & N_FLAG) a -= adjust; else a += adjust;
	if (T::isR800()) {
		f &= C_FLAG | N_FLAG | X_FLAG | Y_FLAG;
		f |= ZSPTable[a];
	} else {
		f &= C_FLAG | N_FLAG;
		f |= ZSPXYTable[a];
	}
	f |= (R.getA() > 0x99) | ((R.getA() ^ a) & H_FLAG);
	R.setA(a);
	R.setF(f);
	return T::CC_DAA;
}
template <class T> int CPUCore<T>::neg() {
	// alternative: LUT   word negTable[256]
	unsigned a = R.getA();
	unsigned res = -signed(a);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((res ^ a) & H_FLAG) |
	         ((a & res & 0x80) >> 5); // V_FLAG
	if (T::isR800()) {
		f |= ZSTable[res & 0xFF];
		f |= R.getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= ZSXYTable[res & 0xFF];
	}
	R.setF(f);
	R.setA(res);
	return T::CC_NEG;
}
template <class T> int CPUCore<T>::scf() {
	byte f = C_FLAG;
	if (T::isR800()) {
		f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		// only set X(Y) flag (don't reset if already set)
		if (isTurboR) {
			// Y flag is not changed on a turboR-Z80
			f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG | Y_FLAG);
			f |= (R.getF() | R.getA()) & X_FLAG;
		} else {
			f |= R.getF() & (S_FLAG | Z_FLAG | P_FLAG);
			f |= (R.getF() | R.getA()) & (X_FLAG | Y_FLAG);
		}
	}
	R.setF(f);
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
	R.setIFF1(false);
	R.setIFF2(false);
	return T::CC_DI;
}
template <class T> int CPUCore<T>::ei() {
	R.setIFF1(true);
	R.setIFF2(true);
	R.setAfterEI(); // no ints directly after this instr
	setSlowInstructions();
	return T::CC_EI;
}
template <class T> int CPUCore<T>::halt() {
	R.setHALT(true);
	setSlowInstructions();

	if (!(R.getIFF1() || R.getIFF2())) {
		diHaltCallback.execute();
	}
	return T::CC_HALT;
}
template <class T> template<unsigned N> int CPUCore<T>::im_N() {
	R.setIM(N); return T::CC_IM;
}

// LD A,I/R
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::ld_a_IR() {
	R.setA(R.get8<REG>());
	byte f = R.getIFF2() ? V_FLAG : 0;
	if (T::isR800()) {
		f |= R.getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= ZSTable[R.getA()];
	} else {
		f |= R.getF() & C_FLAG;
		f |= ZSXYTable[R.getA()];
		// see comment in the IRQ acceptance part of executeSlow().
		R.setAfterLDAI(); // only Z80 (not R800) has this quirk
		setSlowInstructions();
	}
	R.setF(f);
	return T::CC_LD_A_I;
}

// LD I/R,A
template <class T> int CPUCore<T>::ld_r_a() {
	// This code sequence:
	//   XOR A  /  LD R,A   / LD A,R
	// gives A=2 for Z80, but A=1 for R800. The difference can possibly be
	// explained by a difference in the relative time between writing the
	// new value to the R register and increasing the R register per M1
	// cycle. Here we implemented the R800 behaviour by storing 'A-1' into
	// R, that's good enough for now.
	byte val = R.getA();
	if (T::isR800()) val -= 1;
	R.setR(val);
	return T::CC_LD_A_I;
}
template <class T> int CPUCore<T>::ld_i_a() {
	R.setI(R.getA());
	return T::CC_LD_A_I;
}

// MULUB A,r
template <class T> template<CPU::Reg8 REG> int CPUCore<T>::mulub_a_R() {
	assert(T::isR800()); // this instruction is R800-only
	// Verified on real R800:
	//   YHXN flags are unchanged
	//   SV   flags are reset
	//   Z    flag is set when result is zero
	//   C    flag is set when result doesn't fit in 8-bit
	R.setHL(unsigned(R.getA()) * R.get8<REG>());
	R.setF((R.getF() & (N_FLAG | H_FLAG | X_FLAG | Y_FLAG)) |
	       0 | // S_FLAG V_FLAG
	       (R.getHL() ? 0 : Z_FLAG) |
	       ((R.getHL() & 0xFF00) ? C_FLAG : 0));
	return T::CC_MULUB;
}

// MULUW HL,ss
template <class T> template<CPU::Reg16 REG> int CPUCore<T>::muluw_hl_SS() {
	assert(T::isR800()); // this instruction is R800-only
	// Verified on real R800:
	//   YHXN flags are unchanged
	//   SV   flags are reset
	//   Z    flag is set when result is zero
	//   C    flag is set when result doesn't fit in 16-bit
	unsigned res = unsigned(R.getHL()) * R.get16<REG>();
	R.setDE(res >> 16);
	R.setHL(res & 0xffff);
	R.setF((R.getF() & (N_FLAG | H_FLAG | X_FLAG | Y_FLAG)) |
	       0 | // S_FLAG V_FLAG
	       (res ? 0 : Z_FLAG) |
	       ((res & 0xFFFF0000) ? C_FLAG : 0));
	return T::CC_MULUW;
}


// versions:
//  1 -> initial version
//  2 -> moved memptr from here to Z80TYPE (and not to R800TYPE)
//  3 -> timing of the emulation changed (no changes in serialization)
template<class T> template<typename Archive>
void CPUCore<T>::serialize(Archive& ar, unsigned version)
{
	T::serialize(ar, version);
	ar.serialize("regs", R);
	if (ar.versionBelow(version, 2)) {
		unsigned memptr = 0; // dummy value (avoid warning)
		ar.serialize("memptr", memptr);
		T::setMemPtr(memptr);
	}

	if (ar.isLoader()) {
		invalidateMemCache(0x0000, 0x10000);
	}

	// don't serialize
	//    IRQStatus
	//    NMIStatus, nmiEdge
	//    slowInstructions
	//    exitLoop

	if (T::isR800() && ar.versionBelow(version, 3)) {
		motherboard.getMSXCliComm().printWarning(
			"Loading an old savestate: the timing of the R800 "
			"emulation has changed. This may cause synchronization "
			"problems in replay.");
	}
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

INSTANTIATE_SERIALIZE_METHODS(CPUCore<Z80TYPE>);
INSTANTIATE_SERIALIZE_METHODS(CPUCore<R800TYPE>);

} // namespace openmsx

