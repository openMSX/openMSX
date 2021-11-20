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
// of virtual methods. In openMSX it's implemented as follows: the 64kb address
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
// 3) we need to exit the loop as soon as possible (right after the current
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

#include "CPUCore.hh"
#include "MSXCPUInterface.hh"
#include "Scheduler.hh"
#include "MSXMotherBoard.hh"
#include "CliComm.hh"
#include "TclCallback.hh"
#include "Dasm.hh"
#include "Z80.hh"
#include "R800.hh"
#include "Thread.hh"
#include "endian.hh"
#include "likely.hh"
#include "inline.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <iostream>
#include <type_traits>
#include <cassert>


//
// #define USE_COMPUTED_GOTO
//
// Computed goto's are not enabled by default:
// - Computed goto's are a gcc extension, it's not part of the official c++
//   standard. So this will only work if you use gcc as your compiler (it
//   won't work with visual c++ for example)
// - This is only beneficial on CPUs with branch prediction for indirect jumps
//   and a reasonable amount of cache. For example it is very benefical for a
//   intel core2 cpu (10% faster), but not for a ARM920 (a few percent slower)
// - Compiling src/cpu/CPUCore.cc with computed goto's enabled is very demanding
//   on the compiler. On older gcc versions it requires up to 1.5GB of memory.
//   But even on more recent gcc versions it still requires around 700MB.
//
// Probably the easiest way to enable this, is to pass the -DUSE_COMPUTED_GOTO
// flag to the compiler. This is for example done in the super-opt flavour.
// See build/flavour-super-opt.mk

namespace openmsx {

enum Reg8  : int { A, F, B, C, D, E, H, L, IXH, IXL, IYH, IYL, REG_I, REG_R, DUMMY };
enum Reg16 : int { AF, BC, DE, HL, IX, IY, SP };

// flag positions
constexpr byte S_FLAG = 0x80;
constexpr byte Z_FLAG = 0x40;
constexpr byte Y_FLAG = 0x20;
constexpr byte H_FLAG = 0x10;
constexpr byte X_FLAG = 0x08;
constexpr byte V_FLAG = 0x04;
constexpr byte P_FLAG = V_FLAG;
constexpr byte N_FLAG = 0x02;
constexpr byte C_FLAG = 0x01;

// flag-register lookup tables
struct Table {
	byte ZS   [256];
	byte ZSXY [256];
	byte ZSP  [256];
	byte ZSPXY[256];
	byte ZSPH [256];
};

constexpr byte ZS0     = Z_FLAG;
constexpr byte ZSXY0   = Z_FLAG;
constexpr byte ZSP0    = Z_FLAG | V_FLAG;
constexpr byte ZSPXY0  = Z_FLAG | V_FLAG;
constexpr byte ZS255   = S_FLAG;
constexpr byte ZSXY255 = S_FLAG | X_FLAG | Y_FLAG;

static constexpr Table initTables()
{
	Table table = {};

	for (auto i : xrange(256)) {
		byte zFlag = (i == 0) ? Z_FLAG : 0;
		byte sFlag = i & S_FLAG;
		byte xFlag = i & X_FLAG;
		byte yFlag = i & Y_FLAG;
		byte vFlag = V_FLAG;
		for (int v = 128; v != 0; v >>= 1) {
			if (i & v) vFlag ^= V_FLAG;
		}
		table.ZS   [i] = zFlag | sFlag;
		table.ZSXY [i] = zFlag | sFlag | xFlag | yFlag;
		table.ZSP  [i] = zFlag | sFlag |                 vFlag;
		table.ZSPXY[i] = zFlag | sFlag | xFlag | yFlag | vFlag;
		table.ZSPH [i] = zFlag | sFlag |                 vFlag | H_FLAG;
	}
	assert(table.ZS   [  0] == ZS0);
	assert(table.ZSXY [  0] == ZSXY0);
	assert(table.ZSP  [  0] == ZSP0);
	assert(table.ZSPXY[  0] == ZSPXY0);
	assert(table.ZS   [255] == ZS255);
	assert(table.ZSXY [255] == ZSXY255);

	return table;
}

constexpr Table table = initTables();

// Global variable, because it should be shared between Z80 and R800.
// It must not be shared between the CPUs of different MSX machines, but
// the (logical) lifetime of this variable cannot overlap between execution
// of two MSX machines.
static word start_pc;

// conditions
struct CondC  { bool operator()(byte f) const { return  (f & C_FLAG) != 0; } };
struct CondNC { bool operator()(byte f) const { return !(f & C_FLAG); } };
struct CondZ  { bool operator()(byte f) const { return  (f & Z_FLAG) != 0; } };
struct CondNZ { bool operator()(byte f) const { return !(f & Z_FLAG); } };
struct CondM  { bool operator()(byte f) const { return  (f & S_FLAG) != 0; } };
struct CondP  { bool operator()(byte f) const { return !(f & S_FLAG); } };
struct CondPE { bool operator()(byte f) const { return  (f & V_FLAG) != 0; } };
struct CondPO { bool operator()(byte f) const { return !(f & V_FLAG); } };
struct CondTrue { bool operator()(byte /*f*/) const { return true; } };

template<typename T> CPUCore<T>::CPUCore(
		MSXMotherBoard& motherboard_, const std::string& name,
		const BooleanSetting& traceSetting_,
		TclCallback& diHaltCallback_, EmuTime::param time)
	: CPURegs(T::IS_R800)
	, T(time, motherboard_.getScheduler())
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
	, freqLocked(
		motherboard.getCommandController(), tmpStrCat(name, "_freq_locked"),
	        "real (locked) or custom (unlocked) CPU frequency",
	        true)
	, freqValue(
		motherboard.getCommandController(), tmpStrCat(name, "_freq"),
		"custom CPU frequency (only valid when unlocked)",
		T::CLOCK_FREQ, 1000000, 1000000000)
	, freq(T::CLOCK_FREQ)
	, NMIStatus(0)
	, nmiEdge(false)
	, exitLoop(false)
	, tracingEnabled(traceSetting.getBoolean())
	, isTurboR(motherboard.isTurboR())
{
	static_assert(!std::is_polymorphic_v<CPUCore<T>>,
		"keep CPUCore non-virtual to keep PC at offset 0");
	doSetFreq();
	doReset(time);
}

template<typename T> void CPUCore<T>::warp(EmuTime::param time)
{
	assert(T::getTimeFast() <= time);
	T::setTime(time);
}

template<typename T> EmuTime::param CPUCore<T>::getCurrentTime() const
{
	return T::getTime();
}

template<typename T> void CPUCore<T>::doReset(EmuTime::param time)
{
	// AF and SP are 0xFFFF
	// PC, R, IFF1, IFF2, HALT and IM are 0x0
	// all others are random
	setAF(0xFFFF);
	setBC(0xFFFF);
	setDE(0xFFFF);
	setHL(0xFFFF);
	setIX(0xFFFF);
	setIY(0xFFFF);
	setPC(0x0000);
	setSP(0xFFFF);
	setAF2(0xFFFF);
	setBC2(0xFFFF);
	setDE2(0xFFFF);
	setHL2(0xFFFF);
	setIFF1(false);
	setIFF2(false);
	setHALT(false);
	setExtHALT(false);
	setIM(0);
	setI(0x00);
	setR(0x00);
	T::setMemPtr(0xFFFF);
	clearPrevious();

	// We expect this assert to be valid
	//   assert(T::getTimeFast() <= time); // time shouldn't go backwards
	// But it's disabled for the following reason:
	//   'motion' (IRC nickname) managed to create a replay file that
	//   contains a reset command that falls in the middle of a Z80
	//   instruction. Replayed commands go via the Scheduler, and are
	//   (typically) executed right after a complete CPU instruction. So
	//   the CPU is (slightly) ahead in time of the about to be executed
	//   reset command.
	//   Normally this situation should never occur: console commands,
	//   hotkeys, commands over clicomm, ... are all handled via the global
	//   event mechanism. Such global events are scheduled between CPU
	//   instructions, so also in a replay they should fall between CPU
	//   instructions.
	//   However if for some reason the timing of the emulation changed
	//   (improved emulation accuracy or a bug so that emulation isn't
	//   deterministic or the replay file was edited, ...), then the above
	//   reasoning no longer holds and the assert can trigger.
	// We need to be robust against loading older replays (when emulation
	// timing has changed). So in that respect disabling the assert is
	// good. Though in the example above (motion's replay) it's not clear
	// whether the assert is really triggered by mixing an old replay
	// with a newer openMSX version. In any case so far we haven't been
	// able to reproduce this assert by recording and replaying using a
	// single openMSX version.
	T::setTime(time);

	assert(NMIStatus == 0); // other devices must reset their NMI source
	assert(IRQStatus == 0); // other devices must reset their IRQ source
}

// I believe the following two methods are thread safe even without any
// locking. The worst that can happen is that we occasionally needlessly
// exit the CPU loop, but that's harmless
//  TODO thread issues are always tricky, can someone confirm this really
//       is thread safe
template<typename T> void CPUCore<T>::exitCPULoopAsync()
{
	// can get called from non-main threads
	exitLoop = true;
}
template<typename T> void CPUCore<T>::exitCPULoopSync()
{
	assert(Thread::isMainThread());
	exitLoop = true;
	T::disableLimit();
}
template<typename T> inline bool CPUCore<T>::needExitCPULoop()
{
	// always executed in main thread
	if (unlikely(exitLoop)) {
		// Note: The test-and-set is _not_ atomic! But that's fine.
		//   An atomic implementation is trivial (see below), but
		//   this version (at least on x86) avoids the more expensive
		//   instructions on the likely path.
		exitLoop = false;
		return true;
	}
	return false;

	// Alternative implementation:
	//  atomically set to false and return the old value
	//return exitLoop.exchange(false);
}

template<typename T> void CPUCore<T>::setSlowInstructions()
{
	slowInstructions = 2;
	T::disableLimit();
}

template<typename T> void CPUCore<T>::raiseIRQ()
{
	assert(IRQStatus >= 0);
	if (IRQStatus == 0) {
		setSlowInstructions();
	}
	IRQStatus = IRQStatus + 1;
}

template<typename T> void CPUCore<T>::lowerIRQ()
{
	IRQStatus = IRQStatus - 1;
	assert(IRQStatus >= 0);
}

template<typename T> void CPUCore<T>::raiseNMI()
{
	assert(NMIStatus >= 0);
	if (NMIStatus == 0) {
		nmiEdge = true;
		setSlowInstructions();
	}
	NMIStatus++;
}

template<typename T> void CPUCore<T>::lowerNMI()
{
	NMIStatus--;
	assert(NMIStatus >= 0);
}

template<typename T> bool CPUCore<T>::isM1Cycle(unsigned address) const
{
	// This method should only be called from within a MSXDevice::readMem()
	// method. It can be used to check whether the current read action has
	// the M1 pin active. The 'address' parameter that is give to readMem()
	// should be passed (unchanged) to this method.
	//
	// This simple implementation works because the rest of the CPUCore
	// code is careful to only update the PC register on M1 cycles. In
	// practice that means that the PC is (only) updated at the very end of
	// every instruction, even if is a multi-byte instruction. Or for
	// prefix-instructions the PC is also updated after the prefix is
	// fetched (because such instructions activate M1 twice).
	return address == getPC();
}

template<typename T> void CPUCore<T>::wait(EmuTime::param time)
{
	assert(time >= getCurrentTime());
	scheduler.schedule(time);
	T::advanceTime(time);
}

template<typename T> EmuTime CPUCore<T>::waitCycles(EmuTime::param time, unsigned cycles)
{
	T::add(cycles);
	EmuTime time2 = T::calcTime(time, cycles);
	// note: time2 is not necessarily equal to T::getTime() because of the
	// way how WRITE_PORT() is implemented.
	scheduler.schedule(time2);
	return time2;
}

template<typename T> void CPUCore<T>::setNextSyncPoint(EmuTime::param time)
{
	T::setLimit(time);
}


static constexpr char toHex(byte x)
{
	return (x < 10) ? (x + '0') : (x - 10 + 'A');
}
static constexpr void toHex(byte x, char* buf)
{
	buf[0] = toHex(x / 16);
	buf[1] = toHex(x & 15);
}

template<typename T> void CPUCore<T>::disasmCommand(
	Interpreter& interp, span<const TclObject> tokens, TclObject& result) const
{
	word address = (tokens.size() < 3) ? getPC() : tokens[2].getInt(interp);
	byte outBuf[4];
	std::string dasmOutput;
	unsigned len = dasm(*interface, address, outBuf, dasmOutput,
	               T::getTimeFast());
	result.addListElement(dasmOutput);
	char tmp[3]; tmp[2] = 0;
	for (auto i : xrange(len)) {
		toHex(outBuf[i], tmp);
		result.addListElement(tmp);
	}
}

template<typename T> void CPUCore<T>::update(const Setting& setting) noexcept
{
	if (&setting == &freqLocked) {
		doSetFreq();
	} else if (&setting == &freqValue) {
		doSetFreq();
	} else if (&setting == &traceSetting) {
		tracingEnabled = traceSetting.getBoolean();
	}
}

template<typename T> void CPUCore<T>::setFreq(unsigned freq_)
{
	freq = freq_;
	doSetFreq();
}

template<typename T> void CPUCore<T>::doSetFreq()
{
	if (freqLocked.getBoolean()) {
		// locked, use value set via setFreq()
		T::setFreq(freq);
	} else {
		// unlocked, use value set by user
		T::setFreq(freqValue.getInt());
	}
}


template<typename T> inline byte CPUCore<T>::READ_PORT(unsigned port, unsigned cc)
{
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	byte result = interface->readIO(port, time);
	// note: no forced page-break after IO
	return result;
}

template<typename T> inline void CPUCore<T>::WRITE_PORT(unsigned port, byte value, unsigned cc)
{
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	interface->writeIO(port, value, time);
	// note: no forced page-break after IO
}

template<typename T> template<bool PRE_PB, bool POST_PB>
NEVER_INLINE byte CPUCore<T>::RDMEMslow(unsigned address, unsigned cc)
{
	interface->tick(CacheLineCounters::NonCachedRead);
	// not cached
	unsigned high = address >> CacheLine::BITS;
        if (readCacheLine[high] == nullptr) {
		// try to cache now (not a valid entry, and not yet tried)
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
	readCacheLine[high] = reinterpret_cast<const byte*>(1);
	T::template PRE_MEM<PRE_PB, POST_PB>(address);
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	byte result = interface->readMem(address, time);
	T::template POST_MEM<POST_PB>(address);
	return result;
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE byte CPUCore<T>::RDMEM_impl2(unsigned address, unsigned cc)
{
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(uintptr_t(line) > 1)) {
		// cached, fast path
		T::template PRE_MEM<PRE_PB, POST_PB>(address);
		T::template POST_MEM<       POST_PB>(address);
		return line[address];
	} else {
		return RDMEMslow<PRE_PB, POST_PB>(address, cc); // not inlined
	}
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE byte CPUCore<T>::RDMEM_impl(unsigned address, unsigned cc)
{
	constexpr bool PRE  = T::template Normalize<PRE_PB >::value;
	constexpr bool POST = T::template Normalize<POST_PB>::value;
	return RDMEM_impl2<PRE, POST>(address, cc);
}
template<typename T> template<unsigned PC_OFFSET> ALWAYS_INLINE byte CPUCore<T>::RDMEM_OPCODE(unsigned cc)
{
	// Real Z80 would update the PC register now. In this implementation
	// we've chosen to instead update PC only once at the end of the
	// instruction. (Of course we made sure this difference is not
	// noticeable by the program).
	//
	// See the comments in isM1Cycle() for the motivation for this
	// deviation. Apart from that functional aspect it also turns out to be
	// faster to only update PC once per instruction instead of after each
	// fetch.
	unsigned address = (getPC() + PC_OFFSET) & 0xFFFF;
	return RDMEM_impl<false, false>(address, cc);
}
template<typename T> ALWAYS_INLINE byte CPUCore<T>::RDMEM(unsigned address, unsigned cc)
{
	return RDMEM_impl<true, true>(address, cc);
}

template<typename T> template<bool PRE_PB, bool POST_PB>
NEVER_INLINE unsigned CPUCore<T>::RD_WORD_slow(unsigned address, unsigned cc)
{
	unsigned res = RDMEM_impl<PRE_PB,  false>(address, cc);
	res         += RDMEM_impl<false, POST_PB>((address + 1) & 0xFFFF, cc + T::CC_RDMEM) << 8;
	return res;
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD_impl2(unsigned address, unsigned cc)
{
	const byte* line = readCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) && (uintptr_t(line) > 1))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<PRE_PB, POST_PB>(address);
		T::template POST_WORD<       POST_PB>(address);
		return Endian::read_UA_L16(&line[address]);
	} else {
		// slow path, not inline
		return RD_WORD_slow<PRE_PB, POST_PB>(address, cc);
	}
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD_impl(unsigned address, unsigned cc)
{
	constexpr bool PRE  = T::template Normalize<PRE_PB >::value;
	constexpr bool POST = T::template Normalize<POST_PB>::value;
	return RD_WORD_impl2<PRE, POST>(address, cc);
}
template<typename T> template<unsigned PC_OFFSET> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD_PC(unsigned cc)
{
	unsigned addr = (getPC() + PC_OFFSET) & 0xFFFF;
	return RD_WORD_impl<false, false>(addr, cc);
}
template<typename T> ALWAYS_INLINE unsigned CPUCore<T>::RD_WORD(
	unsigned address, unsigned cc)
{
	return RD_WORD_impl<true, true>(address, cc);
}

template<typename T> template<bool PRE_PB, bool POST_PB>
NEVER_INLINE void CPUCore<T>::WRMEMslow(unsigned address, byte value, unsigned cc)
{
	interface->tick(CacheLineCounters::NonCachedWrite);
	// not cached
	unsigned high = address >> CacheLine::BITS;
	if (writeCacheLine[high] == nullptr) {
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
	writeCacheLine[high] = reinterpret_cast<byte*>(1);
	T::template PRE_MEM<PRE_PB, POST_PB>(address);
	EmuTime time = T::getTimeFast(cc);
	scheduler.schedule(time);
	interface->writeMem(address, value, time);
	T::template POST_MEM<POST_PB>(address);
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE void CPUCore<T>::WRMEM_impl2(
	unsigned address, byte value, unsigned cc)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(uintptr_t(line) > 1)) {
		// cached, fast path
		T::template PRE_MEM<PRE_PB, POST_PB>(address);
		T::template POST_MEM<       POST_PB>(address);
		line[address] = value;
	} else {
		WRMEMslow<PRE_PB, POST_PB>(address, value, cc); // not inlined
	}
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE void CPUCore<T>::WRMEM_impl(
	unsigned address, byte value, unsigned cc)
{
	constexpr bool PRE  = T::template Normalize<PRE_PB >::value;
	constexpr bool POST = T::template Normalize<POST_PB>::value;
	WRMEM_impl2<PRE, POST>(address, value, cc);
}
template<typename T> ALWAYS_INLINE void CPUCore<T>::WRMEM(
	unsigned address, byte value, unsigned cc)
{
	WRMEM_impl<true, true>(address, value, cc);
}

template<typename T> NEVER_INLINE void CPUCore<T>::WR_WORD_slow(
	unsigned address, unsigned value, unsigned cc)
{
	WRMEM_impl<true, false>( address,               value & 255, cc);
	WRMEM_impl<false, true>((address + 1) & 0xFFFF, value >> 8,  cc + T::CC_WRMEM);
}
template<typename T> ALWAYS_INLINE void CPUCore<T>::WR_WORD(
	unsigned address, unsigned value, unsigned cc)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) && (uintptr_t(line) > 1))) {
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
template<typename T> template<bool PRE_PB, bool POST_PB>
NEVER_INLINE void CPUCore<T>::WR_WORD_rev_slow(
	unsigned address, unsigned value, unsigned cc)
{
	WRMEM_impl<PRE_PB,  false>((address + 1) & 0xFFFF, value >> 8,  cc);
	WRMEM_impl<false, POST_PB>( address,               value & 255, cc + T::CC_WRMEM);
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE void CPUCore<T>::WR_WORD_rev2(
	unsigned address, unsigned value, unsigned cc)
{
	byte* line = writeCacheLine[address >> CacheLine::BITS];
	if (likely(((address & CacheLine::LOW) != CacheLine::LOW) && (uintptr_t(line) > 1))) {
		// fast path: cached and two bytes in same cache line
		T::template PRE_WORD<PRE_PB, POST_PB>(address);
		T::template POST_WORD<       POST_PB>(address);
		Endian::write_UA_L16(&line[address], value);
	} else {
		// slow path, not inline
		WR_WORD_rev_slow<PRE_PB, POST_PB>(address, value, cc);
	}
}
template<typename T> template<bool PRE_PB, bool POST_PB>
ALWAYS_INLINE void CPUCore<T>::WR_WORD_rev(
	unsigned address, unsigned value, unsigned cc)
{
	constexpr bool PRE  = T::template Normalize<PRE_PB >::value;
	constexpr bool POST = T::template Normalize<POST_PB>::value;
	WR_WORD_rev2<PRE, POST>(address, value, cc);
}


// NMI interrupt
template<typename T> inline void CPUCore<T>::nmi()
{
	incR(1);
	setHALT(false);
	setIFF1(false);
	PUSH<T::EE_NMI_1>(getPC());
	setPC(0x0066);
	T::add(T::CC_NMI);
}

// IM0 interrupt
template<typename T> inline void CPUCore<T>::irq0()
{
	// TODO current implementation only works for 1-byte instructions
	//      ok for MSX
	assert(interface->readIRQVector() == 0xFF);
	incR(1);
	setHALT(false);
	setIFF1(false);
	setIFF2(false);
	PUSH<T::EE_IRQ0_1>(getPC());
	setPC(0x0038);
	T::setMemPtr(getPC());
	T::add(T::CC_IRQ0);
}

// IM1 interrupt
template<typename T> inline void CPUCore<T>::irq1()
{
	incR(1);
	setHALT(false);
	setIFF1(false);
	setIFF2(false);
	PUSH<T::EE_IRQ1_1>(getPC());
	setPC(0x0038);
	T::setMemPtr(getPC());
	T::add(T::CC_IRQ1);
}

// IM2 interrupt
template<typename T> inline void CPUCore<T>::irq2()
{
	incR(1);
	setHALT(false);
	setIFF1(false);
	setIFF2(false);
	PUSH<T::EE_IRQ2_1>(getPC());
	unsigned x = interface->readIRQVector() | (getI() << 8);
	setPC(RD_WORD(x, T::CC_IRQ2_2));
	T::setMemPtr(getPC());
	T::add(T::CC_IRQ2);
}

template<typename T>
void CPUCore<T>::executeInstructions()
{
	checkNoCurrentFlags();
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
	setPC(getPC() + ii.length); \
	T::add(ii.cycles); \
	T::R800Refresh(*this); \
	if (likely(!T::limitReached())) { \
		incR(1); \
		unsigned address = getPC(); \
		const byte* line = readCacheLine[address >> CacheLine::BITS]; \
		if (likely(uintptr_t(line) > 1)) { \
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
	setPC(getPC() + ii.length); \
	T::add(ii.cycles); \
	T::R800Refresh(*this); \
	assert(T::limitReached()); \
	return;

#define NEXT_EI \
	setPC(getPC() + ii.length); \
	T::add(ii.cycles); \
	/* !! NO T::R800Refresh(*this); !! */ \
	assert(T::limitReached()); \
	return;

// Define a label (instead of case in a switch statement)
#define CASE(X) op##X:

#else // USE_COMPUTED_GOTO

#define NEXT \
	setPC(getPC() + ii.length); \
	T::add(ii.cycles); \
	T::R800Refresh(*this); \
	if (likely(!T::limitReached())) { \
		goto start; \
	} \
	return;

#define NEXT_STOP \
	setPC(getPC() + ii.length); \
	T::add(ii.cycles); \
	T::R800Refresh(*this); \
	assert(T::limitReached()); \
	return;

#define NEXT_EI \
	setPC(getPC() + ii.length); \
	T::add(ii.cycles); \
	/* !! NO T::R800Refresh(*this); !! */ \
	assert(T::limitReached()); \
	return;

#define CASE(X) case 0x##X:

#endif // USE_COMPUTED_GOTO

#ifndef USE_COMPUTED_GOTO
start:
#endif
	unsigned ixy; // for dd_cb/fd_cb
	byte opcodeMain = RDMEM_OPCODE<0>(T::CC_MAIN);
	incR(1);
#ifdef USE_COMPUTED_GOTO
	goto *(opcodeTable[opcodeMain]);

fetchSlow: {
	unsigned address = getPC();
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
CASE(00) { II ii = nop(); NEXT; }
CASE(07) { II ii = rlca(); NEXT; }
CASE(0F) { II ii = rrca(); NEXT; }
CASE(17) { II ii = rla();  NEXT; }
CASE(1F) { II ii = rra();  NEXT; }
CASE(08) { II ii = ex_af_af(); NEXT; }
CASE(27) { II ii = daa(); NEXT; }
CASE(2F) { II ii = cpl(); NEXT; }
CASE(37) { II ii = scf(); NEXT; }
CASE(3F) { II ii = ccf(); NEXT; }
CASE(20) { II ii = jr(CondNZ()); NEXT; }
CASE(28) { II ii = jr(CondZ ()); NEXT; }
CASE(30) { II ii = jr(CondNC()); NEXT; }
CASE(38) { II ii = jr(CondC ()); NEXT; }
CASE(18) { II ii = jr(CondTrue()); NEXT; }
CASE(10) { II ii = djnz(); NEXT; }
CASE(32) { II ii = ld_xbyte_a(); NEXT; }
CASE(3A) { II ii = ld_a_xbyte(); NEXT; }
CASE(22) { II ii = ld_xword_SS<HL,0>(); NEXT; }
CASE(2A) { II ii = ld_SS_xword<HL,0>(); NEXT; }
CASE(02) { II ii = ld_SS_a<BC>(); NEXT; }
CASE(12) { II ii = ld_SS_a<DE>(); NEXT; }
CASE(1A) { II ii = ld_a_SS<DE>(); NEXT; }
CASE(0A) { II ii = ld_a_SS<BC>(); NEXT; }
CASE(03) { II ii = inc_SS<BC,0>(); NEXT; }
CASE(13) { II ii = inc_SS<DE,0>(); NEXT; }
CASE(23) { II ii = inc_SS<HL,0>(); NEXT; }
CASE(33) { II ii = inc_SS<SP,0>(); NEXT; }
CASE(0B) { II ii = dec_SS<BC,0>(); NEXT; }
CASE(1B) { II ii = dec_SS<DE,0>(); NEXT; }
CASE(2B) { II ii = dec_SS<HL,0>(); NEXT; }
CASE(3B) { II ii = dec_SS<SP,0>(); NEXT; }
CASE(09) { II ii = add_SS_TT<HL,BC,0>(); NEXT; }
CASE(19) { II ii = add_SS_TT<HL,DE,0>(); NEXT; }
CASE(29) { II ii = add_SS_SS<HL   ,0>(); NEXT; }
CASE(39) { II ii = add_SS_TT<HL,SP,0>(); NEXT; }
CASE(01) { II ii = ld_SS_word<BC,0>(); NEXT; }
CASE(11) { II ii = ld_SS_word<DE,0>(); NEXT; }
CASE(21) { II ii = ld_SS_word<HL,0>(); NEXT; }
CASE(31) { II ii = ld_SS_word<SP,0>(); NEXT; }
CASE(04) { II ii = inc_R<B,0>(); NEXT; }
CASE(0C) { II ii = inc_R<C,0>(); NEXT; }
CASE(14) { II ii = inc_R<D,0>(); NEXT; }
CASE(1C) { II ii = inc_R<E,0>(); NEXT; }
CASE(24) { II ii = inc_R<H,0>(); NEXT; }
CASE(2C) { II ii = inc_R<L,0>(); NEXT; }
CASE(3C) { II ii = inc_R<A,0>(); NEXT; }
CASE(34) { II ii = inc_xhl();  NEXT; }
CASE(05) { II ii = dec_R<B,0>(); NEXT; }
CASE(0D) { II ii = dec_R<C,0>(); NEXT; }
CASE(15) { II ii = dec_R<D,0>(); NEXT; }
CASE(1D) { II ii = dec_R<E,0>(); NEXT; }
CASE(25) { II ii = dec_R<H,0>(); NEXT; }
CASE(2D) { II ii = dec_R<L,0>(); NEXT; }
CASE(3D) { II ii = dec_R<A,0>(); NEXT; }
CASE(35) { II ii = dec_xhl();  NEXT; }
CASE(06) { II ii = ld_R_byte<B,0>(); NEXT; }
CASE(0E) { II ii = ld_R_byte<C,0>(); NEXT; }
CASE(16) { II ii = ld_R_byte<D,0>(); NEXT; }
CASE(1E) { II ii = ld_R_byte<E,0>(); NEXT; }
CASE(26) { II ii = ld_R_byte<H,0>(); NEXT; }
CASE(2E) { II ii = ld_R_byte<L,0>(); NEXT; }
CASE(3E) { II ii = ld_R_byte<A,0>(); NEXT; }
CASE(36) { II ii = ld_xhl_byte();  NEXT; }

CASE(41) { II ii = ld_R_R<B,C,0>(); NEXT; }
CASE(42) { II ii = ld_R_R<B,D,0>(); NEXT; }
CASE(43) { II ii = ld_R_R<B,E,0>(); NEXT; }
CASE(44) { II ii = ld_R_R<B,H,0>(); NEXT; }
CASE(45) { II ii = ld_R_R<B,L,0>(); NEXT; }
CASE(47) { II ii = ld_R_R<B,A,0>(); NEXT; }
CASE(48) { II ii = ld_R_R<C,B,0>(); NEXT; }
CASE(4A) { II ii = ld_R_R<C,D,0>(); NEXT; }
CASE(4B) { II ii = ld_R_R<C,E,0>(); NEXT; }
CASE(4C) { II ii = ld_R_R<C,H,0>(); NEXT; }
CASE(4D) { II ii = ld_R_R<C,L,0>(); NEXT; }
CASE(4F) { II ii = ld_R_R<C,A,0>(); NEXT; }
CASE(50) { II ii = ld_R_R<D,B,0>(); NEXT; }
CASE(51) { II ii = ld_R_R<D,C,0>(); NEXT; }
CASE(53) { II ii = ld_R_R<D,E,0>(); NEXT; }
CASE(54) { II ii = ld_R_R<D,H,0>(); NEXT; }
CASE(55) { II ii = ld_R_R<D,L,0>(); NEXT; }
CASE(57) { II ii = ld_R_R<D,A,0>(); NEXT; }
CASE(58) { II ii = ld_R_R<E,B,0>(); NEXT; }
CASE(59) { II ii = ld_R_R<E,C,0>(); NEXT; }
CASE(5A) { II ii = ld_R_R<E,D,0>(); NEXT; }
CASE(5C) { II ii = ld_R_R<E,H,0>(); NEXT; }
CASE(5D) { II ii = ld_R_R<E,L,0>(); NEXT; }
CASE(5F) { II ii = ld_R_R<E,A,0>(); NEXT; }
CASE(60) { II ii = ld_R_R<H,B,0>(); NEXT; }
CASE(61) { II ii = ld_R_R<H,C,0>(); NEXT; }
CASE(62) { II ii = ld_R_R<H,D,0>(); NEXT; }
CASE(63) { II ii = ld_R_R<H,E,0>(); NEXT; }
CASE(65) { II ii = ld_R_R<H,L,0>(); NEXT; }
CASE(67) { II ii = ld_R_R<H,A,0>(); NEXT; }
CASE(68) { II ii = ld_R_R<L,B,0>(); NEXT; }
CASE(69) { II ii = ld_R_R<L,C,0>(); NEXT; }
CASE(6A) { II ii = ld_R_R<L,D,0>(); NEXT; }
CASE(6B) { II ii = ld_R_R<L,E,0>(); NEXT; }
CASE(6C) { II ii = ld_R_R<L,H,0>(); NEXT; }
CASE(6F) { II ii = ld_R_R<L,A,0>(); NEXT; }
CASE(78) { II ii = ld_R_R<A,B,0>(); NEXT; }
CASE(79) { II ii = ld_R_R<A,C,0>(); NEXT; }
CASE(7A) { II ii = ld_R_R<A,D,0>(); NEXT; }
CASE(7B) { II ii = ld_R_R<A,E,0>(); NEXT; }
CASE(7C) { II ii = ld_R_R<A,H,0>(); NEXT; }
CASE(7D) { II ii = ld_R_R<A,L,0>(); NEXT; }
CASE(70) { II ii = ld_xhl_R<B>(); NEXT; }
CASE(71) { II ii = ld_xhl_R<C>(); NEXT; }
CASE(72) { II ii = ld_xhl_R<D>(); NEXT; }
CASE(73) { II ii = ld_xhl_R<E>(); NEXT; }
CASE(74) { II ii = ld_xhl_R<H>(); NEXT; }
CASE(75) { II ii = ld_xhl_R<L>(); NEXT; }
CASE(77) { II ii = ld_xhl_R<A>(); NEXT; }
CASE(46) { II ii = ld_R_xhl<B>(); NEXT; }
CASE(4E) { II ii = ld_R_xhl<C>(); NEXT; }
CASE(56) { II ii = ld_R_xhl<D>(); NEXT; }
CASE(5E) { II ii = ld_R_xhl<E>(); NEXT; }
CASE(66) { II ii = ld_R_xhl<H>(); NEXT; }
CASE(6E) { II ii = ld_R_xhl<L>(); NEXT; }
CASE(7E) { II ii = ld_R_xhl<A>(); NEXT; }
CASE(76) { II ii = halt(); NEXT_STOP; }

CASE(80) { II ii = add_a_R<B,0>(); NEXT; }
CASE(81) { II ii = add_a_R<C,0>(); NEXT; }
CASE(82) { II ii = add_a_R<D,0>(); NEXT; }
CASE(83) { II ii = add_a_R<E,0>(); NEXT; }
CASE(84) { II ii = add_a_R<H,0>(); NEXT; }
CASE(85) { II ii = add_a_R<L,0>(); NEXT; }
CASE(86) { II ii = add_a_xhl();   NEXT; }
CASE(87) { II ii = add_a_a();     NEXT; }
CASE(88) { II ii = adc_a_R<B,0>(); NEXT; }
CASE(89) { II ii = adc_a_R<C,0>(); NEXT; }
CASE(8A) { II ii = adc_a_R<D,0>(); NEXT; }
CASE(8B) { II ii = adc_a_R<E,0>(); NEXT; }
CASE(8C) { II ii = adc_a_R<H,0>(); NEXT; }
CASE(8D) { II ii = adc_a_R<L,0>(); NEXT; }
CASE(8E) { II ii = adc_a_xhl();   NEXT; }
CASE(8F) { II ii = adc_a_a();     NEXT; }
CASE(90) { II ii = sub_R<B,0>(); NEXT; }
CASE(91) { II ii = sub_R<C,0>(); NEXT; }
CASE(92) { II ii = sub_R<D,0>(); NEXT; }
CASE(93) { II ii = sub_R<E,0>(); NEXT; }
CASE(94) { II ii = sub_R<H,0>(); NEXT; }
CASE(95) { II ii = sub_R<L,0>(); NEXT; }
CASE(96) { II ii = sub_xhl();   NEXT; }
CASE(97) { II ii = sub_a();     NEXT; }
CASE(98) { II ii = sbc_a_R<B,0>(); NEXT; }
CASE(99) { II ii = sbc_a_R<C,0>(); NEXT; }
CASE(9A) { II ii = sbc_a_R<D,0>(); NEXT; }
CASE(9B) { II ii = sbc_a_R<E,0>(); NEXT; }
CASE(9C) { II ii = sbc_a_R<H,0>(); NEXT; }
CASE(9D) { II ii = sbc_a_R<L,0>(); NEXT; }
CASE(9E) { II ii = sbc_a_xhl();   NEXT; }
CASE(9F) { II ii = sbc_a_a();     NEXT; }
CASE(A0) { II ii = and_R<B,0>(); NEXT; }
CASE(A1) { II ii = and_R<C,0>(); NEXT; }
CASE(A2) { II ii = and_R<D,0>(); NEXT; }
CASE(A3) { II ii = and_R<E,0>(); NEXT; }
CASE(A4) { II ii = and_R<H,0>(); NEXT; }
CASE(A5) { II ii = and_R<L,0>(); NEXT; }
CASE(A6) { II ii = and_xhl();   NEXT; }
CASE(A7) { II ii = and_a();     NEXT; }
CASE(A8) { II ii = xor_R<B,0>(); NEXT; }
CASE(A9) { II ii = xor_R<C,0>(); NEXT; }
CASE(AA) { II ii = xor_R<D,0>(); NEXT; }
CASE(AB) { II ii = xor_R<E,0>(); NEXT; }
CASE(AC) { II ii = xor_R<H,0>(); NEXT; }
CASE(AD) { II ii = xor_R<L,0>(); NEXT; }
CASE(AE) { II ii = xor_xhl();   NEXT; }
CASE(AF) { II ii = xor_a();     NEXT; }
CASE(B0) { II ii = or_R<B,0>(); NEXT; }
CASE(B1) { II ii = or_R<C,0>(); NEXT; }
CASE(B2) { II ii = or_R<D,0>(); NEXT; }
CASE(B3) { II ii = or_R<E,0>(); NEXT; }
CASE(B4) { II ii = or_R<H,0>(); NEXT; }
CASE(B5) { II ii = or_R<L,0>(); NEXT; }
CASE(B6) { II ii = or_xhl();   NEXT; }
CASE(B7) { II ii = or_a();     NEXT; }
CASE(B8) { II ii = cp_R<B,0>(); NEXT; }
CASE(B9) { II ii = cp_R<C,0>(); NEXT; }
CASE(BA) { II ii = cp_R<D,0>(); NEXT; }
CASE(BB) { II ii = cp_R<E,0>(); NEXT; }
CASE(BC) { II ii = cp_R<H,0>(); NEXT; }
CASE(BD) { II ii = cp_R<L,0>(); NEXT; }
CASE(BE) { II ii = cp_xhl();   NEXT; }
CASE(BF) { II ii = cp_a();     NEXT; }

CASE(D3) { II ii = out_byte_a(); NEXT; }
CASE(DB) { II ii = in_a_byte();  NEXT; }
CASE(D9) { II ii = exx(); NEXT; }
CASE(E3) { II ii = ex_xsp_SS<HL,0>(); NEXT; }
CASE(EB) { II ii = ex_de_hl(); NEXT; }
CASE(E9) { II ii = jp_SS<HL,0>(); NEXT; }
CASE(F9) { II ii = ld_sp_SS<HL,0>(); NEXT; }
CASE(F3) { II ii = di(); NEXT; }
CASE(FB) { II ii = ei(); NEXT_EI; }
CASE(C6) { II ii = add_a_byte(); NEXT; }
CASE(CE) { II ii = adc_a_byte(); NEXT; }
CASE(D6) { II ii = sub_byte();   NEXT; }
CASE(DE) { II ii = sbc_a_byte(); NEXT; }
CASE(E6) { II ii = and_byte();   NEXT; }
CASE(EE) { II ii = xor_byte();   NEXT; }
CASE(F6) { II ii = or_byte();    NEXT; }
CASE(FE) { II ii = cp_byte();    NEXT; }
CASE(C0) { II ii = ret(CondNZ()); NEXT; }
CASE(C8) { II ii = ret(CondZ ()); NEXT; }
CASE(D0) { II ii = ret(CondNC()); NEXT; }
CASE(D8) { II ii = ret(CondC ()); NEXT; }
CASE(E0) { II ii = ret(CondPO()); NEXT; }
CASE(E8) { II ii = ret(CondPE()); NEXT; }
CASE(F0) { II ii = ret(CondP ()); NEXT; }
CASE(F8) { II ii = ret(CondM ()); NEXT; }
CASE(C9) { II ii = ret();         NEXT; }
CASE(C2) { II ii = jp(CondNZ()); NEXT; }
CASE(CA) { II ii = jp(CondZ ()); NEXT; }
CASE(D2) { II ii = jp(CondNC()); NEXT; }
CASE(DA) { II ii = jp(CondC ()); NEXT; }
CASE(E2) { II ii = jp(CondPO()); NEXT; }
CASE(EA) { II ii = jp(CondPE()); NEXT; }
CASE(F2) { II ii = jp(CondP ()); NEXT; }
CASE(FA) { II ii = jp(CondM ()); NEXT; }
CASE(C3) { II ii = jp(CondTrue()); NEXT; }
CASE(C4) { II ii = call(CondNZ()); NEXT; }
CASE(CC) { II ii = call(CondZ ()); NEXT; }
CASE(D4) { II ii = call(CondNC()); NEXT; }
CASE(DC) { II ii = call(CondC ()); NEXT; }
CASE(E4) { II ii = call(CondPO()); NEXT; }
CASE(EC) { II ii = call(CondPE()); NEXT; }
CASE(F4) { II ii = call(CondP ()); NEXT; }
CASE(FC) { II ii = call(CondM ()); NEXT; }
CASE(CD) { II ii = call(CondTrue()); NEXT; }
CASE(C1) { II ii = pop_SS <BC,0>(); NEXT; }
CASE(D1) { II ii = pop_SS <DE,0>(); NEXT; }
CASE(E1) { II ii = pop_SS <HL,0>(); NEXT; }
CASE(F1) { II ii = pop_SS <AF,0>(); NEXT; }
CASE(C5) { II ii = push_SS<BC,0>(); NEXT; }
CASE(D5) { II ii = push_SS<DE,0>(); NEXT; }
CASE(E5) { II ii = push_SS<HL,0>(); NEXT; }
CASE(F5) { II ii = push_SS<AF,0>(); NEXT; }
CASE(C7) { II ii = rst<0x00>(); NEXT; }
CASE(CF) { II ii = rst<0x08>(); NEXT; }
CASE(D7) { II ii = rst<0x10>(); NEXT; }
CASE(DF) { II ii = rst<0x18>(); NEXT; }
CASE(E7) { II ii = rst<0x20>(); NEXT; }
CASE(EF) { II ii = rst<0x28>(); NEXT; }
CASE(F7) { II ii = rst<0x30>(); NEXT; }
CASE(FF) { II ii = rst<0x38>(); NEXT; }
CASE(CB) {
	setPC(getPC() + 1); // M1 cycle at this point
	byte cb_opcode = RDMEM_OPCODE<0>(T::CC_PREFIX);
	incR(1);
	switch (cb_opcode) {
		case 0x00: { II ii = rlc_R<B>(); NEXT; }
		case 0x01: { II ii = rlc_R<C>(); NEXT; }
		case 0x02: { II ii = rlc_R<D>(); NEXT; }
		case 0x03: { II ii = rlc_R<E>(); NEXT; }
		case 0x04: { II ii = rlc_R<H>(); NEXT; }
		case 0x05: { II ii = rlc_R<L>(); NEXT; }
		case 0x07: { II ii = rlc_R<A>(); NEXT; }
		case 0x06: { II ii = rlc_xhl();  NEXT; }
		case 0x08: { II ii = rrc_R<B>(); NEXT; }
		case 0x09: { II ii = rrc_R<C>(); NEXT; }
		case 0x0a: { II ii = rrc_R<D>(); NEXT; }
		case 0x0b: { II ii = rrc_R<E>(); NEXT; }
		case 0x0c: { II ii = rrc_R<H>(); NEXT; }
		case 0x0d: { II ii = rrc_R<L>(); NEXT; }
		case 0x0f: { II ii = rrc_R<A>(); NEXT; }
		case 0x0e: { II ii = rrc_xhl();  NEXT; }
		case 0x10: { II ii = rl_R<B>(); NEXT; }
		case 0x11: { II ii = rl_R<C>(); NEXT; }
		case 0x12: { II ii = rl_R<D>(); NEXT; }
		case 0x13: { II ii = rl_R<E>(); NEXT; }
		case 0x14: { II ii = rl_R<H>(); NEXT; }
		case 0x15: { II ii = rl_R<L>(); NEXT; }
		case 0x17: { II ii = rl_R<A>(); NEXT; }
		case 0x16: { II ii = rl_xhl();  NEXT; }
		case 0x18: { II ii = rr_R<B>(); NEXT; }
		case 0x19: { II ii = rr_R<C>(); NEXT; }
		case 0x1a: { II ii = rr_R<D>(); NEXT; }
		case 0x1b: { II ii = rr_R<E>(); NEXT; }
		case 0x1c: { II ii = rr_R<H>(); NEXT; }
		case 0x1d: { II ii = rr_R<L>(); NEXT; }
		case 0x1f: { II ii = rr_R<A>(); NEXT; }
		case 0x1e: { II ii = rr_xhl();  NEXT; }
		case 0x20: { II ii = sla_R<B>(); NEXT; }
		case 0x21: { II ii = sla_R<C>(); NEXT; }
		case 0x22: { II ii = sla_R<D>(); NEXT; }
		case 0x23: { II ii = sla_R<E>(); NEXT; }
		case 0x24: { II ii = sla_R<H>(); NEXT; }
		case 0x25: { II ii = sla_R<L>(); NEXT; }
		case 0x27: { II ii = sla_R<A>(); NEXT; }
		case 0x26: { II ii = sla_xhl();  NEXT; }
		case 0x28: { II ii = sra_R<B>(); NEXT; }
		case 0x29: { II ii = sra_R<C>(); NEXT; }
		case 0x2a: { II ii = sra_R<D>(); NEXT; }
		case 0x2b: { II ii = sra_R<E>(); NEXT; }
		case 0x2c: { II ii = sra_R<H>(); NEXT; }
		case 0x2d: { II ii = sra_R<L>(); NEXT; }
		case 0x2f: { II ii = sra_R<A>(); NEXT; }
		case 0x2e: { II ii = sra_xhl();  NEXT; }
		case 0x30: { II ii = T::IS_R800 ? sla_R<B>() : sll_R<B>(); NEXT; }
		case 0x31: { II ii = T::IS_R800 ? sla_R<C>() : sll_R<C>(); NEXT; }
		case 0x32: { II ii = T::IS_R800 ? sla_R<D>() : sll_R<D>(); NEXT; }
		case 0x33: { II ii = T::IS_R800 ? sla_R<E>() : sll_R<E>(); NEXT; }
		case 0x34: { II ii = T::IS_R800 ? sla_R<H>() : sll_R<H>(); NEXT; }
		case 0x35: { II ii = T::IS_R800 ? sla_R<L>() : sll_R<L>(); NEXT; }
		case 0x37: { II ii = T::IS_R800 ? sla_R<A>() : sll_R<A>(); NEXT; }
		case 0x36: { II ii = T::IS_R800 ? sla_xhl()  : sll_xhl();  NEXT; }
		case 0x38: { II ii = srl_R<B>(); NEXT; }
		case 0x39: { II ii = srl_R<C>(); NEXT; }
		case 0x3a: { II ii = srl_R<D>(); NEXT; }
		case 0x3b: { II ii = srl_R<E>(); NEXT; }
		case 0x3c: { II ii = srl_R<H>(); NEXT; }
		case 0x3d: { II ii = srl_R<L>(); NEXT; }
		case 0x3f: { II ii = srl_R<A>(); NEXT; }
		case 0x3e: { II ii = srl_xhl();  NEXT; }

		case 0x40: { II ii = bit_N_R<0,B>(); NEXT; }
		case 0x41: { II ii = bit_N_R<0,C>(); NEXT; }
		case 0x42: { II ii = bit_N_R<0,D>(); NEXT; }
		case 0x43: { II ii = bit_N_R<0,E>(); NEXT; }
		case 0x44: { II ii = bit_N_R<0,H>(); NEXT; }
		case 0x45: { II ii = bit_N_R<0,L>(); NEXT; }
		case 0x47: { II ii = bit_N_R<0,A>(); NEXT; }
		case 0x48: { II ii = bit_N_R<1,B>(); NEXT; }
		case 0x49: { II ii = bit_N_R<1,C>(); NEXT; }
		case 0x4a: { II ii = bit_N_R<1,D>(); NEXT; }
		case 0x4b: { II ii = bit_N_R<1,E>(); NEXT; }
		case 0x4c: { II ii = bit_N_R<1,H>(); NEXT; }
		case 0x4d: { II ii = bit_N_R<1,L>(); NEXT; }
		case 0x4f: { II ii = bit_N_R<1,A>(); NEXT; }
		case 0x50: { II ii = bit_N_R<2,B>(); NEXT; }
		case 0x51: { II ii = bit_N_R<2,C>(); NEXT; }
		case 0x52: { II ii = bit_N_R<2,D>(); NEXT; }
		case 0x53: { II ii = bit_N_R<2,E>(); NEXT; }
		case 0x54: { II ii = bit_N_R<2,H>(); NEXT; }
		case 0x55: { II ii = bit_N_R<2,L>(); NEXT; }
		case 0x57: { II ii = bit_N_R<2,A>(); NEXT; }
		case 0x58: { II ii = bit_N_R<3,B>(); NEXT; }
		case 0x59: { II ii = bit_N_R<3,C>(); NEXT; }
		case 0x5a: { II ii = bit_N_R<3,D>(); NEXT; }
		case 0x5b: { II ii = bit_N_R<3,E>(); NEXT; }
		case 0x5c: { II ii = bit_N_R<3,H>(); NEXT; }
		case 0x5d: { II ii = bit_N_R<3,L>(); NEXT; }
		case 0x5f: { II ii = bit_N_R<3,A>(); NEXT; }
		case 0x60: { II ii = bit_N_R<4,B>(); NEXT; }
		case 0x61: { II ii = bit_N_R<4,C>(); NEXT; }
		case 0x62: { II ii = bit_N_R<4,D>(); NEXT; }
		case 0x63: { II ii = bit_N_R<4,E>(); NEXT; }
		case 0x64: { II ii = bit_N_R<4,H>(); NEXT; }
		case 0x65: { II ii = bit_N_R<4,L>(); NEXT; }
		case 0x67: { II ii = bit_N_R<4,A>(); NEXT; }
		case 0x68: { II ii = bit_N_R<5,B>(); NEXT; }
		case 0x69: { II ii = bit_N_R<5,C>(); NEXT; }
		case 0x6a: { II ii = bit_N_R<5,D>(); NEXT; }
		case 0x6b: { II ii = bit_N_R<5,E>(); NEXT; }
		case 0x6c: { II ii = bit_N_R<5,H>(); NEXT; }
		case 0x6d: { II ii = bit_N_R<5,L>(); NEXT; }
		case 0x6f: { II ii = bit_N_R<5,A>(); NEXT; }
		case 0x70: { II ii = bit_N_R<6,B>(); NEXT; }
		case 0x71: { II ii = bit_N_R<6,C>(); NEXT; }
		case 0x72: { II ii = bit_N_R<6,D>(); NEXT; }
		case 0x73: { II ii = bit_N_R<6,E>(); NEXT; }
		case 0x74: { II ii = bit_N_R<6,H>(); NEXT; }
		case 0x75: { II ii = bit_N_R<6,L>(); NEXT; }
		case 0x77: { II ii = bit_N_R<6,A>(); NEXT; }
		case 0x78: { II ii = bit_N_R<7,B>(); NEXT; }
		case 0x79: { II ii = bit_N_R<7,C>(); NEXT; }
		case 0x7a: { II ii = bit_N_R<7,D>(); NEXT; }
		case 0x7b: { II ii = bit_N_R<7,E>(); NEXT; }
		case 0x7c: { II ii = bit_N_R<7,H>(); NEXT; }
		case 0x7d: { II ii = bit_N_R<7,L>(); NEXT; }
		case 0x7f: { II ii = bit_N_R<7,A>(); NEXT; }
		case 0x46: { II ii = bit_N_xhl<0>(); NEXT; }
		case 0x4e: { II ii = bit_N_xhl<1>(); NEXT; }
		case 0x56: { II ii = bit_N_xhl<2>(); NEXT; }
		case 0x5e: { II ii = bit_N_xhl<3>(); NEXT; }
		case 0x66: { II ii = bit_N_xhl<4>(); NEXT; }
		case 0x6e: { II ii = bit_N_xhl<5>(); NEXT; }
		case 0x76: { II ii = bit_N_xhl<6>(); NEXT; }
		case 0x7e: { II ii = bit_N_xhl<7>(); NEXT; }

		case 0x80: { II ii = res_N_R<0,B>(); NEXT; }
		case 0x81: { II ii = res_N_R<0,C>(); NEXT; }
		case 0x82: { II ii = res_N_R<0,D>(); NEXT; }
		case 0x83: { II ii = res_N_R<0,E>(); NEXT; }
		case 0x84: { II ii = res_N_R<0,H>(); NEXT; }
		case 0x85: { II ii = res_N_R<0,L>(); NEXT; }
		case 0x87: { II ii = res_N_R<0,A>(); NEXT; }
		case 0x88: { II ii = res_N_R<1,B>(); NEXT; }
		case 0x89: { II ii = res_N_R<1,C>(); NEXT; }
		case 0x8a: { II ii = res_N_R<1,D>(); NEXT; }
		case 0x8b: { II ii = res_N_R<1,E>(); NEXT; }
		case 0x8c: { II ii = res_N_R<1,H>(); NEXT; }
		case 0x8d: { II ii = res_N_R<1,L>(); NEXT; }
		case 0x8f: { II ii = res_N_R<1,A>(); NEXT; }
		case 0x90: { II ii = res_N_R<2,B>(); NEXT; }
		case 0x91: { II ii = res_N_R<2,C>(); NEXT; }
		case 0x92: { II ii = res_N_R<2,D>(); NEXT; }
		case 0x93: { II ii = res_N_R<2,E>(); NEXT; }
		case 0x94: { II ii = res_N_R<2,H>(); NEXT; }
		case 0x95: { II ii = res_N_R<2,L>(); NEXT; }
		case 0x97: { II ii = res_N_R<2,A>(); NEXT; }
		case 0x98: { II ii = res_N_R<3,B>(); NEXT; }
		case 0x99: { II ii = res_N_R<3,C>(); NEXT; }
		case 0x9a: { II ii = res_N_R<3,D>(); NEXT; }
		case 0x9b: { II ii = res_N_R<3,E>(); NEXT; }
		case 0x9c: { II ii = res_N_R<3,H>(); NEXT; }
		case 0x9d: { II ii = res_N_R<3,L>(); NEXT; }
		case 0x9f: { II ii = res_N_R<3,A>(); NEXT; }
		case 0xa0: { II ii = res_N_R<4,B>(); NEXT; }
		case 0xa1: { II ii = res_N_R<4,C>(); NEXT; }
		case 0xa2: { II ii = res_N_R<4,D>(); NEXT; }
		case 0xa3: { II ii = res_N_R<4,E>(); NEXT; }
		case 0xa4: { II ii = res_N_R<4,H>(); NEXT; }
		case 0xa5: { II ii = res_N_R<4,L>(); NEXT; }
		case 0xa7: { II ii = res_N_R<4,A>(); NEXT; }
		case 0xa8: { II ii = res_N_R<5,B>(); NEXT; }
		case 0xa9: { II ii = res_N_R<5,C>(); NEXT; }
		case 0xaa: { II ii = res_N_R<5,D>(); NEXT; }
		case 0xab: { II ii = res_N_R<5,E>(); NEXT; }
		case 0xac: { II ii = res_N_R<5,H>(); NEXT; }
		case 0xad: { II ii = res_N_R<5,L>(); NEXT; }
		case 0xaf: { II ii = res_N_R<5,A>(); NEXT; }
		case 0xb0: { II ii = res_N_R<6,B>(); NEXT; }
		case 0xb1: { II ii = res_N_R<6,C>(); NEXT; }
		case 0xb2: { II ii = res_N_R<6,D>(); NEXT; }
		case 0xb3: { II ii = res_N_R<6,E>(); NEXT; }
		case 0xb4: { II ii = res_N_R<6,H>(); NEXT; }
		case 0xb5: { II ii = res_N_R<6,L>(); NEXT; }
		case 0xb7: { II ii = res_N_R<6,A>(); NEXT; }
		case 0xb8: { II ii = res_N_R<7,B>(); NEXT; }
		case 0xb9: { II ii = res_N_R<7,C>(); NEXT; }
		case 0xba: { II ii = res_N_R<7,D>(); NEXT; }
		case 0xbb: { II ii = res_N_R<7,E>(); NEXT; }
		case 0xbc: { II ii = res_N_R<7,H>(); NEXT; }
		case 0xbd: { II ii = res_N_R<7,L>(); NEXT; }
		case 0xbf: { II ii = res_N_R<7,A>(); NEXT; }
		case 0x86: { II ii = res_N_xhl<0>(); NEXT; }
		case 0x8e: { II ii = res_N_xhl<1>(); NEXT; }
		case 0x96: { II ii = res_N_xhl<2>(); NEXT; }
		case 0x9e: { II ii = res_N_xhl<3>(); NEXT; }
		case 0xa6: { II ii = res_N_xhl<4>(); NEXT; }
		case 0xae: { II ii = res_N_xhl<5>(); NEXT; }
		case 0xb6: { II ii = res_N_xhl<6>(); NEXT; }
		case 0xbe: { II ii = res_N_xhl<7>(); NEXT; }

		case 0xc0: { II ii = set_N_R<0,B>(); NEXT; }
		case 0xc1: { II ii = set_N_R<0,C>(); NEXT; }
		case 0xc2: { II ii = set_N_R<0,D>(); NEXT; }
		case 0xc3: { II ii = set_N_R<0,E>(); NEXT; }
		case 0xc4: { II ii = set_N_R<0,H>(); NEXT; }
		case 0xc5: { II ii = set_N_R<0,L>(); NEXT; }
		case 0xc7: { II ii = set_N_R<0,A>(); NEXT; }
		case 0xc8: { II ii = set_N_R<1,B>(); NEXT; }
		case 0xc9: { II ii = set_N_R<1,C>(); NEXT; }
		case 0xca: { II ii = set_N_R<1,D>(); NEXT; }
		case 0xcb: { II ii = set_N_R<1,E>(); NEXT; }
		case 0xcc: { II ii = set_N_R<1,H>(); NEXT; }
		case 0xcd: { II ii = set_N_R<1,L>(); NEXT; }
		case 0xcf: { II ii = set_N_R<1,A>(); NEXT; }
		case 0xd0: { II ii = set_N_R<2,B>(); NEXT; }
		case 0xd1: { II ii = set_N_R<2,C>(); NEXT; }
		case 0xd2: { II ii = set_N_R<2,D>(); NEXT; }
		case 0xd3: { II ii = set_N_R<2,E>(); NEXT; }
		case 0xd4: { II ii = set_N_R<2,H>(); NEXT; }
		case 0xd5: { II ii = set_N_R<2,L>(); NEXT; }
		case 0xd7: { II ii = set_N_R<2,A>(); NEXT; }
		case 0xd8: { II ii = set_N_R<3,B>(); NEXT; }
		case 0xd9: { II ii = set_N_R<3,C>(); NEXT; }
		case 0xda: { II ii = set_N_R<3,D>(); NEXT; }
		case 0xdb: { II ii = set_N_R<3,E>(); NEXT; }
		case 0xdc: { II ii = set_N_R<3,H>(); NEXT; }
		case 0xdd: { II ii = set_N_R<3,L>(); NEXT; }
		case 0xdf: { II ii = set_N_R<3,A>(); NEXT; }
		case 0xe0: { II ii = set_N_R<4,B>(); NEXT; }
		case 0xe1: { II ii = set_N_R<4,C>(); NEXT; }
		case 0xe2: { II ii = set_N_R<4,D>(); NEXT; }
		case 0xe3: { II ii = set_N_R<4,E>(); NEXT; }
		case 0xe4: { II ii = set_N_R<4,H>(); NEXT; }
		case 0xe5: { II ii = set_N_R<4,L>(); NEXT; }
		case 0xe7: { II ii = set_N_R<4,A>(); NEXT; }
		case 0xe8: { II ii = set_N_R<5,B>(); NEXT; }
		case 0xe9: { II ii = set_N_R<5,C>(); NEXT; }
		case 0xea: { II ii = set_N_R<5,D>(); NEXT; }
		case 0xeb: { II ii = set_N_R<5,E>(); NEXT; }
		case 0xec: { II ii = set_N_R<5,H>(); NEXT; }
		case 0xed: { II ii = set_N_R<5,L>(); NEXT; }
		case 0xef: { II ii = set_N_R<5,A>(); NEXT; }
		case 0xf0: { II ii = set_N_R<6,B>(); NEXT; }
		case 0xf1: { II ii = set_N_R<6,C>(); NEXT; }
		case 0xf2: { II ii = set_N_R<6,D>(); NEXT; }
		case 0xf3: { II ii = set_N_R<6,E>(); NEXT; }
		case 0xf4: { II ii = set_N_R<6,H>(); NEXT; }
		case 0xf5: { II ii = set_N_R<6,L>(); NEXT; }
		case 0xf7: { II ii = set_N_R<6,A>(); NEXT; }
		case 0xf8: { II ii = set_N_R<7,B>(); NEXT; }
		case 0xf9: { II ii = set_N_R<7,C>(); NEXT; }
		case 0xfa: { II ii = set_N_R<7,D>(); NEXT; }
		case 0xfb: { II ii = set_N_R<7,E>(); NEXT; }
		case 0xfc: { II ii = set_N_R<7,H>(); NEXT; }
		case 0xfd: { II ii = set_N_R<7,L>(); NEXT; }
		case 0xff: { II ii = set_N_R<7,A>(); NEXT; }
		case 0xc6: { II ii = set_N_xhl<0>(); NEXT; }
		case 0xce: { II ii = set_N_xhl<1>(); NEXT; }
		case 0xd6: { II ii = set_N_xhl<2>(); NEXT; }
		case 0xde: { II ii = set_N_xhl<3>(); NEXT; }
		case 0xe6: { II ii = set_N_xhl<4>(); NEXT; }
		case 0xee: { II ii = set_N_xhl<5>(); NEXT; }
		case 0xf6: { II ii = set_N_xhl<6>(); NEXT; }
		case 0xfe: { II ii = set_N_xhl<7>(); NEXT; }
		default: UNREACHABLE; return;
	}
}
CASE(ED) {
	setPC(getPC() + 1); // M1 cycle at this point
	byte ed_opcode = RDMEM_OPCODE<0>(T::CC_PREFIX);
	incR(1);
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
		           { II ii = nop(); NEXT; }

		case 0x40: { II ii = in_R_c<B>(); NEXT; }
		case 0x48: { II ii = in_R_c<C>(); NEXT; }
		case 0x50: { II ii = in_R_c<D>(); NEXT; }
		case 0x58: { II ii = in_R_c<E>(); NEXT; }
		case 0x60: { II ii = in_R_c<H>(); NEXT; }
		case 0x68: { II ii = in_R_c<L>(); NEXT; }
		case 0x70: { II ii = in_R_c<DUMMY>(); NEXT; }
		case 0x78: { II ii = in_R_c<A>(); NEXT; }

		case 0x41: { II ii = out_c_R<B>(); NEXT; }
		case 0x49: { II ii = out_c_R<C>(); NEXT; }
		case 0x51: { II ii = out_c_R<D>(); NEXT; }
		case 0x59: { II ii = out_c_R<E>(); NEXT; }
		case 0x61: { II ii = out_c_R<H>(); NEXT; }
		case 0x69: { II ii = out_c_R<L>(); NEXT; }
		case 0x71: { II ii = out_c_0();    NEXT; }
		case 0x79: { II ii = out_c_R<A>(); NEXT; }

		case 0x42: { II ii = sbc_hl_SS<BC>(); NEXT; }
		case 0x52: { II ii = sbc_hl_SS<DE>(); NEXT; }
		case 0x62: { II ii = sbc_hl_hl    (); NEXT; }
		case 0x72: { II ii = sbc_hl_SS<SP>(); NEXT; }

		case 0x4a: { II ii = adc_hl_SS<BC>(); NEXT; }
		case 0x5a: { II ii = adc_hl_SS<DE>(); NEXT; }
		case 0x6a: { II ii = adc_hl_hl    (); NEXT; }
		case 0x7a: { II ii = adc_hl_SS<SP>(); NEXT; }

		case 0x43: { II ii = ld_xword_SS_ED<BC>(); NEXT; }
		case 0x53: { II ii = ld_xword_SS_ED<DE>(); NEXT; }
		case 0x63: { II ii = ld_xword_SS_ED<HL>(); NEXT; }
		case 0x73: { II ii = ld_xword_SS_ED<SP>(); NEXT; }

		case 0x4b: { II ii = ld_SS_xword_ED<BC>(); NEXT; }
		case 0x5b: { II ii = ld_SS_xword_ED<DE>(); NEXT; }
		case 0x6b: { II ii = ld_SS_xword_ED<HL>(); NEXT; }
		case 0x7b: { II ii = ld_SS_xword_ED<SP>(); NEXT; }

		case 0x47: { II ii = ld_i_a(); NEXT; }
		case 0x4f: { II ii = ld_r_a(); NEXT; }
		case 0x57: { II ii = ld_a_IR<REG_I>(); if (T::IS_R800) { NEXT; } else { NEXT_STOP; }}
		case 0x5f: { II ii = ld_a_IR<REG_R>(); if (T::IS_R800) { NEXT; } else { NEXT_STOP; }}

		case 0x67: { II ii = rrd(); NEXT; }
		case 0x6f: { II ii = rld(); NEXT; }

		case 0x45: case 0x4d: case 0x55: case 0x5d:
		case 0x65: case 0x6d: case 0x75: case 0x7d:
		           { II ii = retn(); NEXT_STOP; }
		case 0x46: case 0x4e: case 0x66: case 0x6e:
		           { II ii = im_N<0>(); NEXT; }
		case 0x56: case 0x76:
		           { II ii = im_N<1>(); NEXT; }
		case 0x5e: case 0x7e:
		           { II ii = im_N<2>(); NEXT; }
		case 0x44: case 0x4c: case 0x54: case 0x5c:
		case 0x64: case 0x6c: case 0x74: case 0x7c:
		           { II ii = neg(); NEXT; }

		case 0xa0: { II ii = ldi();  NEXT; }
		case 0xa1: { II ii = cpi();  NEXT; }
		case 0xa2: { II ii = ini();  NEXT; }
		case 0xa3: { II ii = outi(); NEXT; }
		case 0xa8: { II ii = ldd();  NEXT; }
		case 0xa9: { II ii = cpd();  NEXT; }
		case 0xaa: { II ii = ind();  NEXT; }
		case 0xab: { II ii = outd(); NEXT; }
		case 0xb0: { II ii = ldir(); NEXT; }
		case 0xb1: { II ii = cpir(); NEXT; }
		case 0xb2: { II ii = inir(); NEXT; }
		case 0xb3: { II ii = otir(); NEXT; }
		case 0xb8: { II ii = lddr(); NEXT; }
		case 0xb9: { II ii = cpdr(); NEXT; }
		case 0xba: { II ii = indr(); NEXT; }
		case 0xbb: { II ii = otdr(); NEXT; }

		case 0xc1: { II ii = T::IS_R800 ? mulub_a_R<B>() : nop(); NEXT; }
		case 0xc9: { II ii = T::IS_R800 ? mulub_a_R<C>() : nop(); NEXT; }
		case 0xd1: { II ii = T::IS_R800 ? mulub_a_R<D>() : nop(); NEXT; }
		case 0xd9: { II ii = T::IS_R800 ? mulub_a_R<E>() : nop(); NEXT; }
		case 0xc3: { II ii = T::IS_R800 ? muluw_hl_SS<BC>() : nop(); NEXT; }
		case 0xf3: { II ii = T::IS_R800 ? muluw_hl_SS<SP>() : nop(); NEXT; }
		default: UNREACHABLE; return;
	}
}
opDD_2:
CASE(DD) {
	setPC(getPC() + 1); // M1 cycle at this point
	byte opcodeDD = RDMEM_OPCODE<0>(T::CC_DD + T::CC_MAIN);
	incR(1);
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
			if /*constexpr*/ (T::IS_R800) {
				II ii = nop();
				ii.cycles += T::CC_DD;
				NEXT;
			} else {
				T::add(T::CC_DD);
				#ifdef USE_COMPUTED_GOTO
				goto *(opcodeTable[opcodeDD]);
				#else
				opcodeMain = opcodeDD;
				goto switchopcode;
				#endif
			}

		case 0x09: { II ii = add_SS_TT<IX,BC,T::CC_DD>(); NEXT; }
		case 0x19: { II ii = add_SS_TT<IX,DE,T::CC_DD>(); NEXT; }
		case 0x29: { II ii = add_SS_SS<IX   ,T::CC_DD>(); NEXT; }
		case 0x39: { II ii = add_SS_TT<IX,SP,T::CC_DD>(); NEXT; }
		case 0x21: { II ii = ld_SS_word<IX,T::CC_DD>();  NEXT; }
		case 0x22: { II ii = ld_xword_SS<IX,T::CC_DD>(); NEXT; }
		case 0x2a: { II ii = ld_SS_xword<IX,T::CC_DD>(); NEXT; }
		case 0x23: { II ii = inc_SS<IX,T::CC_DD>(); NEXT; }
		case 0x2b: { II ii = dec_SS<IX,T::CC_DD>(); NEXT; }
		case 0x24: { II ii = inc_R<IXH,T::CC_DD>(); NEXT; }
		case 0x2c: { II ii = inc_R<IXL,T::CC_DD>(); NEXT; }
		case 0x25: { II ii = dec_R<IXH,T::CC_DD>(); NEXT; }
		case 0x2d: { II ii = dec_R<IXL,T::CC_DD>(); NEXT; }
		case 0x26: { II ii = ld_R_byte<IXH,T::CC_DD>(); NEXT; }
		case 0x2e: { II ii = ld_R_byte<IXL,T::CC_DD>(); NEXT; }
		case 0x34: { II ii = inc_xix<IX>(); NEXT; }
		case 0x35: { II ii = dec_xix<IX>(); NEXT; }
		case 0x36: { II ii = ld_xix_byte<IX>(); NEXT; }

		case 0x44: { II ii = ld_R_R<B,IXH,T::CC_DD>(); NEXT; }
		case 0x45: { II ii = ld_R_R<B,IXL,T::CC_DD>(); NEXT; }
		case 0x4c: { II ii = ld_R_R<C,IXH,T::CC_DD>(); NEXT; }
		case 0x4d: { II ii = ld_R_R<C,IXL,T::CC_DD>(); NEXT; }
		case 0x54: { II ii = ld_R_R<D,IXH,T::CC_DD>(); NEXT; }
		case 0x55: { II ii = ld_R_R<D,IXL,T::CC_DD>(); NEXT; }
		case 0x5c: { II ii = ld_R_R<E,IXH,T::CC_DD>(); NEXT; }
		case 0x5d: { II ii = ld_R_R<E,IXL,T::CC_DD>(); NEXT; }
		case 0x7c: { II ii = ld_R_R<A,IXH,T::CC_DD>(); NEXT; }
		case 0x7d: { II ii = ld_R_R<A,IXL,T::CC_DD>(); NEXT; }
		case 0x60: { II ii = ld_R_R<IXH,B,T::CC_DD>(); NEXT; }
		case 0x61: { II ii = ld_R_R<IXH,C,T::CC_DD>(); NEXT; }
		case 0x62: { II ii = ld_R_R<IXH,D,T::CC_DD>(); NEXT; }
		case 0x63: { II ii = ld_R_R<IXH,E,T::CC_DD>(); NEXT; }
		case 0x65: { II ii = ld_R_R<IXH,IXL,T::CC_DD>(); NEXT; }
		case 0x67: { II ii = ld_R_R<IXH,A,T::CC_DD>(); NEXT; }
		case 0x68: { II ii = ld_R_R<IXL,B,T::CC_DD>(); NEXT; }
		case 0x69: { II ii = ld_R_R<IXL,C,T::CC_DD>(); NEXT; }
		case 0x6a: { II ii = ld_R_R<IXL,D,T::CC_DD>(); NEXT; }
		case 0x6b: { II ii = ld_R_R<IXL,E,T::CC_DD>(); NEXT; }
		case 0x6c: { II ii = ld_R_R<IXL,IXH,T::CC_DD>(); NEXT; }
		case 0x6f: { II ii = ld_R_R<IXL,A,T::CC_DD>(); NEXT; }
		case 0x70: { II ii = ld_xix_R<IX,B>(); NEXT; }
		case 0x71: { II ii = ld_xix_R<IX,C>(); NEXT; }
		case 0x72: { II ii = ld_xix_R<IX,D>(); NEXT; }
		case 0x73: { II ii = ld_xix_R<IX,E>(); NEXT; }
		case 0x74: { II ii = ld_xix_R<IX,H>(); NEXT; }
		case 0x75: { II ii = ld_xix_R<IX,L>(); NEXT; }
		case 0x77: { II ii = ld_xix_R<IX,A>(); NEXT; }
		case 0x46: { II ii = ld_R_xix<B,IX>(); NEXT; }
		case 0x4e: { II ii = ld_R_xix<C,IX>(); NEXT; }
		case 0x56: { II ii = ld_R_xix<D,IX>(); NEXT; }
		case 0x5e: { II ii = ld_R_xix<E,IX>(); NEXT; }
		case 0x66: { II ii = ld_R_xix<H,IX>(); NEXT; }
		case 0x6e: { II ii = ld_R_xix<L,IX>(); NEXT; }
		case 0x7e: { II ii = ld_R_xix<A,IX>(); NEXT; }

		case 0x84: { II ii = add_a_R<IXH,T::CC_DD>(); NEXT; }
		case 0x85: { II ii = add_a_R<IXL,T::CC_DD>(); NEXT; }
		case 0x86: { II ii = add_a_xix<IX>(); NEXT; }
		case 0x8c: { II ii = adc_a_R<IXH,T::CC_DD>(); NEXT; }
		case 0x8d: { II ii = adc_a_R<IXL,T::CC_DD>(); NEXT; }
		case 0x8e: { II ii = adc_a_xix<IX>(); NEXT; }
		case 0x94: { II ii = sub_R<IXH,T::CC_DD>(); NEXT; }
		case 0x95: { II ii = sub_R<IXL,T::CC_DD>(); NEXT; }
		case 0x96: { II ii = sub_xix<IX>(); NEXT; }
		case 0x9c: { II ii = sbc_a_R<IXH,T::CC_DD>(); NEXT; }
		case 0x9d: { II ii = sbc_a_R<IXL,T::CC_DD>(); NEXT; }
		case 0x9e: { II ii = sbc_a_xix<IX>(); NEXT; }
		case 0xa4: { II ii = and_R<IXH,T::CC_DD>(); NEXT; }
		case 0xa5: { II ii = and_R<IXL,T::CC_DD>(); NEXT; }
		case 0xa6: { II ii = and_xix<IX>(); NEXT; }
		case 0xac: { II ii = xor_R<IXH,T::CC_DD>(); NEXT; }
		case 0xad: { II ii = xor_R<IXL,T::CC_DD>(); NEXT; }
		case 0xae: { II ii = xor_xix<IX>(); NEXT; }
		case 0xb4: { II ii = or_R<IXH,T::CC_DD>(); NEXT; }
		case 0xb5: { II ii = or_R<IXL,T::CC_DD>(); NEXT; }
		case 0xb6: { II ii = or_xix<IX>(); NEXT; }
		case 0xbc: { II ii = cp_R<IXH,T::CC_DD>(); NEXT; }
		case 0xbd: { II ii = cp_R<IXL,T::CC_DD>(); NEXT; }
		case 0xbe: { II ii = cp_xix<IX>(); NEXT; }

		case 0xe1: { II ii = pop_SS <IX,T::CC_DD>(); NEXT; }
		case 0xe5: { II ii = push_SS<IX,T::CC_DD>(); NEXT; }
		case 0xe3: { II ii = ex_xsp_SS<IX,T::CC_DD>(); NEXT; }
		case 0xe9: { II ii = jp_SS<IX,T::CC_DD>(); NEXT; }
		case 0xf9: { II ii = ld_sp_SS<IX,T::CC_DD>(); NEXT; }
		case 0xcb: ixy = getIX(); goto xx_cb;
		case 0xdd: T::add(T::CC_DD); goto opDD_2;
		case 0xfd: T::add(T::CC_DD); goto opFD_2;
		default: UNREACHABLE; return;
	}
}
opFD_2:
CASE(FD) {
	setPC(getPC() + 1); // M1 cycle at this point
	byte opcodeFD = RDMEM_OPCODE<0>(T::CC_DD + T::CC_MAIN);
	incR(1);
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
			if constexpr (T::IS_R800) {
				II ii = nop();
				ii.cycles += T::CC_DD;
				NEXT;
			} else {
				T::add(T::CC_DD);
				#ifdef USE_COMPUTED_GOTO
				goto *(opcodeTable[opcodeFD]);
				#else
				opcodeMain = opcodeFD;
				goto switchopcode;
				#endif
			}

		case 0x09: { II ii = add_SS_TT<IY,BC,T::CC_DD>(); NEXT; }
		case 0x19: { II ii = add_SS_TT<IY,DE,T::CC_DD>(); NEXT; }
		case 0x29: { II ii = add_SS_SS<IY   ,T::CC_DD>(); NEXT; }
		case 0x39: { II ii = add_SS_TT<IY,SP,T::CC_DD>(); NEXT; }
		case 0x21: { II ii = ld_SS_word<IY,T::CC_DD>();  NEXT; }
		case 0x22: { II ii = ld_xword_SS<IY,T::CC_DD>(); NEXT; }
		case 0x2a: { II ii = ld_SS_xword<IY,T::CC_DD>(); NEXT; }
		case 0x23: { II ii = inc_SS<IY,T::CC_DD>(); NEXT; }
		case 0x2b: { II ii = dec_SS<IY,T::CC_DD>(); NEXT; }
		case 0x24: { II ii = inc_R<IYH,T::CC_DD>(); NEXT; }
		case 0x2c: { II ii = inc_R<IYL,T::CC_DD>(); NEXT; }
		case 0x25: { II ii = dec_R<IYH,T::CC_DD>(); NEXT; }
		case 0x2d: { II ii = dec_R<IYL,T::CC_DD>(); NEXT; }
		case 0x26: { II ii = ld_R_byte<IYH,T::CC_DD>(); NEXT; }
		case 0x2e: { II ii = ld_R_byte<IYL,T::CC_DD>(); NEXT; }
		case 0x34: { II ii = inc_xix<IY>(); NEXT; }
		case 0x35: { II ii = dec_xix<IY>(); NEXT; }
		case 0x36: { II ii = ld_xix_byte<IY>(); NEXT; }

		case 0x44: { II ii = ld_R_R<B,IYH,T::CC_DD>(); NEXT; }
		case 0x45: { II ii = ld_R_R<B,IYL,T::CC_DD>(); NEXT; }
		case 0x4c: { II ii = ld_R_R<C,IYH,T::CC_DD>(); NEXT; }
		case 0x4d: { II ii = ld_R_R<C,IYL,T::CC_DD>(); NEXT; }
		case 0x54: { II ii = ld_R_R<D,IYH,T::CC_DD>(); NEXT; }
		case 0x55: { II ii = ld_R_R<D,IYL,T::CC_DD>(); NEXT; }
		case 0x5c: { II ii = ld_R_R<E,IYH,T::CC_DD>(); NEXT; }
		case 0x5d: { II ii = ld_R_R<E,IYL,T::CC_DD>(); NEXT; }
		case 0x7c: { II ii = ld_R_R<A,IYH,T::CC_DD>(); NEXT; }
		case 0x7d: { II ii = ld_R_R<A,IYL,T::CC_DD>(); NEXT; }
		case 0x60: { II ii = ld_R_R<IYH,B,T::CC_DD>(); NEXT; }
		case 0x61: { II ii = ld_R_R<IYH,C,T::CC_DD>(); NEXT; }
		case 0x62: { II ii = ld_R_R<IYH,D,T::CC_DD>(); NEXT; }
		case 0x63: { II ii = ld_R_R<IYH,E,T::CC_DD>(); NEXT; }
		case 0x65: { II ii = ld_R_R<IYH,IYL,T::CC_DD>(); NEXT; }
		case 0x67: { II ii = ld_R_R<IYH,A,T::CC_DD>(); NEXT; }
		case 0x68: { II ii = ld_R_R<IYL,B,T::CC_DD>(); NEXT; }
		case 0x69: { II ii = ld_R_R<IYL,C,T::CC_DD>(); NEXT; }
		case 0x6a: { II ii = ld_R_R<IYL,D,T::CC_DD>(); NEXT; }
		case 0x6b: { II ii = ld_R_R<IYL,E,T::CC_DD>(); NEXT; }
		case 0x6c: { II ii = ld_R_R<IYL,IYH,T::CC_DD>(); NEXT; }
		case 0x6f: { II ii = ld_R_R<IYL,A,T::CC_DD>(); NEXT; }
		case 0x70: { II ii = ld_xix_R<IY,B>(); NEXT; }
		case 0x71: { II ii = ld_xix_R<IY,C>(); NEXT; }
		case 0x72: { II ii = ld_xix_R<IY,D>(); NEXT; }
		case 0x73: { II ii = ld_xix_R<IY,E>(); NEXT; }
		case 0x74: { II ii = ld_xix_R<IY,H>(); NEXT; }
		case 0x75: { II ii = ld_xix_R<IY,L>(); NEXT; }
		case 0x77: { II ii = ld_xix_R<IY,A>(); NEXT; }
		case 0x46: { II ii = ld_R_xix<B,IY>(); NEXT; }
		case 0x4e: { II ii = ld_R_xix<C,IY>(); NEXT; }
		case 0x56: { II ii = ld_R_xix<D,IY>(); NEXT; }
		case 0x5e: { II ii = ld_R_xix<E,IY>(); NEXT; }
		case 0x66: { II ii = ld_R_xix<H,IY>(); NEXT; }
		case 0x6e: { II ii = ld_R_xix<L,IY>(); NEXT; }
		case 0x7e: { II ii = ld_R_xix<A,IY>(); NEXT; }

		case 0x84: { II ii = add_a_R<IYH,T::CC_DD>(); NEXT; }
		case 0x85: { II ii = add_a_R<IYL,T::CC_DD>(); NEXT; }
		case 0x86: { II ii = add_a_xix<IY>(); NEXT; }
		case 0x8c: { II ii = adc_a_R<IYH,T::CC_DD>(); NEXT; }
		case 0x8d: { II ii = adc_a_R<IYL,T::CC_DD>(); NEXT; }
		case 0x8e: { II ii = adc_a_xix<IY>(); NEXT; }
		case 0x94: { II ii = sub_R<IYH,T::CC_DD>(); NEXT; }
		case 0x95: { II ii = sub_R<IYL,T::CC_DD>(); NEXT; }
		case 0x96: { II ii = sub_xix<IY>(); NEXT; }
		case 0x9c: { II ii = sbc_a_R<IYH,T::CC_DD>(); NEXT; }
		case 0x9d: { II ii = sbc_a_R<IYL,T::CC_DD>(); NEXT; }
		case 0x9e: { II ii = sbc_a_xix<IY>(); NEXT; }
		case 0xa4: { II ii = and_R<IYH,T::CC_DD>(); NEXT; }
		case 0xa5: { II ii = and_R<IYL,T::CC_DD>(); NEXT; }
		case 0xa6: { II ii = and_xix<IY>(); NEXT; }
		case 0xac: { II ii = xor_R<IYH,T::CC_DD>(); NEXT; }
		case 0xad: { II ii = xor_R<IYL,T::CC_DD>(); NEXT; }
		case 0xae: { II ii = xor_xix<IY>(); NEXT; }
		case 0xb4: { II ii = or_R<IYH,T::CC_DD>(); NEXT; }
		case 0xb5: { II ii = or_R<IYL,T::CC_DD>(); NEXT; }
		case 0xb6: { II ii = or_xix<IY>(); NEXT; }
		case 0xbc: { II ii = cp_R<IYH,T::CC_DD>(); NEXT; }
		case 0xbd: { II ii = cp_R<IYL,T::CC_DD>(); NEXT; }
		case 0xbe: { II ii = cp_xix<IY>(); NEXT; }

		case 0xe1: { II ii = pop_SS <IY,T::CC_DD>(); NEXT; }
		case 0xe5: { II ii = push_SS<IY,T::CC_DD>(); NEXT; }
		case 0xe3: { II ii = ex_xsp_SS<IY,T::CC_DD>(); NEXT; }
		case 0xe9: { II ii = jp_SS<IY,T::CC_DD>(); NEXT; }
		case 0xf9: { II ii = ld_sp_SS<IY,T::CC_DD>(); NEXT; }
		case 0xcb: ixy = getIY(); goto xx_cb;
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
		unsigned tmp = RD_WORD_PC<1>(T::CC_DD + T::CC_DD_CB);
		int8_t ofst = tmp & 0xFF;
		unsigned addr = (ixy + ofst) & 0xFFFF;
		byte xxcb_opcode = tmp >> 8;
		switch (xxcb_opcode) {
			case 0x00: { II ii = rlc_xix_R<B>(addr); NEXT; }
			case 0x01: { II ii = rlc_xix_R<C>(addr); NEXT; }
			case 0x02: { II ii = rlc_xix_R<D>(addr); NEXT; }
			case 0x03: { II ii = rlc_xix_R<E>(addr); NEXT; }
			case 0x04: { II ii = rlc_xix_R<H>(addr); NEXT; }
			case 0x05: { II ii = rlc_xix_R<L>(addr); NEXT; }
			case 0x06: { II ii = rlc_xix_R<DUMMY>(addr); NEXT; }
			case 0x07: { II ii = rlc_xix_R<A>(addr); NEXT; }
			case 0x08: { II ii = rrc_xix_R<B>(addr); NEXT; }
			case 0x09: { II ii = rrc_xix_R<C>(addr); NEXT; }
			case 0x0a: { II ii = rrc_xix_R<D>(addr); NEXT; }
			case 0x0b: { II ii = rrc_xix_R<E>(addr); NEXT; }
			case 0x0c: { II ii = rrc_xix_R<H>(addr); NEXT; }
			case 0x0d: { II ii = rrc_xix_R<L>(addr); NEXT; }
			case 0x0e: { II ii = rrc_xix_R<DUMMY>(addr); NEXT; }
			case 0x0f: { II ii = rrc_xix_R<A>(addr); NEXT; }
			case 0x10: { II ii = rl_xix_R<B>(addr); NEXT; }
			case 0x11: { II ii = rl_xix_R<C>(addr); NEXT; }
			case 0x12: { II ii = rl_xix_R<D>(addr); NEXT; }
			case 0x13: { II ii = rl_xix_R<E>(addr); NEXT; }
			case 0x14: { II ii = rl_xix_R<H>(addr); NEXT; }
			case 0x15: { II ii = rl_xix_R<L>(addr); NEXT; }
			case 0x16: { II ii = rl_xix_R<DUMMY>(addr); NEXT; }
			case 0x17: { II ii = rl_xix_R<A>(addr); NEXT; }
			case 0x18: { II ii = rr_xix_R<B>(addr); NEXT; }
			case 0x19: { II ii = rr_xix_R<C>(addr); NEXT; }
			case 0x1a: { II ii = rr_xix_R<D>(addr); NEXT; }
			case 0x1b: { II ii = rr_xix_R<E>(addr); NEXT; }
			case 0x1c: { II ii = rr_xix_R<H>(addr); NEXT; }
			case 0x1d: { II ii = rr_xix_R<L>(addr); NEXT; }
			case 0x1e: { II ii = rr_xix_R<DUMMY>(addr); NEXT; }
			case 0x1f: { II ii = rr_xix_R<A>(addr); NEXT; }
			case 0x20: { II ii = sla_xix_R<B>(addr); NEXT; }
			case 0x21: { II ii = sla_xix_R<C>(addr); NEXT; }
			case 0x22: { II ii = sla_xix_R<D>(addr); NEXT; }
			case 0x23: { II ii = sla_xix_R<E>(addr); NEXT; }
			case 0x24: { II ii = sla_xix_R<H>(addr); NEXT; }
			case 0x25: { II ii = sla_xix_R<L>(addr); NEXT; }
			case 0x26: { II ii = sla_xix_R<DUMMY>(addr); NEXT; }
			case 0x27: { II ii = sla_xix_R<A>(addr); NEXT; }
			case 0x28: { II ii = sra_xix_R<B>(addr); NEXT; }
			case 0x29: { II ii = sra_xix_R<C>(addr); NEXT; }
			case 0x2a: { II ii = sra_xix_R<D>(addr); NEXT; }
			case 0x2b: { II ii = sra_xix_R<E>(addr); NEXT; }
			case 0x2c: { II ii = sra_xix_R<H>(addr); NEXT; }
			case 0x2d: { II ii = sra_xix_R<L>(addr); NEXT; }
			case 0x2e: { II ii = sra_xix_R<DUMMY>(addr); NEXT; }
			case 0x2f: { II ii = sra_xix_R<A>(addr); NEXT; }
			case 0x30: { II ii = T::IS_R800 ? sll2() : sll_xix_R<B>(addr); NEXT; }
			case 0x31: { II ii = T::IS_R800 ? sll2() : sll_xix_R<C>(addr); NEXT; }
			case 0x32: { II ii = T::IS_R800 ? sll2() : sll_xix_R<D>(addr); NEXT; }
			case 0x33: { II ii = T::IS_R800 ? sll2() : sll_xix_R<E>(addr); NEXT; }
			case 0x34: { II ii = T::IS_R800 ? sll2() : sll_xix_R<H>(addr); NEXT; }
			case 0x35: { II ii = T::IS_R800 ? sll2() : sll_xix_R<L>(addr); NEXT; }
			case 0x36: { II ii = T::IS_R800 ? sll2() : sll_xix_R<DUMMY>(addr); NEXT; }
			case 0x37: { II ii = T::IS_R800 ? sll2() : sll_xix_R<A>(addr); NEXT; }
			case 0x38: { II ii = srl_xix_R<B>(addr); NEXT; }
			case 0x39: { II ii = srl_xix_R<C>(addr); NEXT; }
			case 0x3a: { II ii = srl_xix_R<D>(addr); NEXT; }
			case 0x3b: { II ii = srl_xix_R<E>(addr); NEXT; }
			case 0x3c: { II ii = srl_xix_R<H>(addr); NEXT; }
			case 0x3d: { II ii = srl_xix_R<L>(addr); NEXT; }
			case 0x3e: { II ii = srl_xix_R<DUMMY>(addr); NEXT; }
			case 0x3f: { II ii = srl_xix_R<A>(addr); NEXT; }

			case 0x40: case 0x41: case 0x42: case 0x43:
			case 0x44: case 0x45: case 0x46: case 0x47:
			           { II ii = bit_N_xix<0>(addr); NEXT; }
			case 0x48: case 0x49: case 0x4a: case 0x4b:
			case 0x4c: case 0x4d: case 0x4e: case 0x4f:
			           { II ii = bit_N_xix<1>(addr); NEXT; }
			case 0x50: case 0x51: case 0x52: case 0x53:
			case 0x54: case 0x55: case 0x56: case 0x57:
			           { II ii = bit_N_xix<2>(addr); NEXT; }
			case 0x58: case 0x59: case 0x5a: case 0x5b:
			case 0x5c: case 0x5d: case 0x5e: case 0x5f:
			           { II ii = bit_N_xix<3>(addr); NEXT; }
			case 0x60: case 0x61: case 0x62: case 0x63:
			case 0x64: case 0x65: case 0x66: case 0x67:
			           { II ii = bit_N_xix<4>(addr); NEXT; }
			case 0x68: case 0x69: case 0x6a: case 0x6b:
			case 0x6c: case 0x6d: case 0x6e: case 0x6f:
			           { II ii = bit_N_xix<5>(addr); NEXT; }
			case 0x70: case 0x71: case 0x72: case 0x73:
			case 0x74: case 0x75: case 0x76: case 0x77:
			           { II ii = bit_N_xix<6>(addr); NEXT; }
			case 0x78: case 0x79: case 0x7a: case 0x7b:
			case 0x7c: case 0x7d: case 0x7e: case 0x7f:
			           { II ii = bit_N_xix<7>(addr); NEXT; }

			case 0x80: { II ii = res_N_xix_R<0,B>(addr); NEXT; }
			case 0x81: { II ii = res_N_xix_R<0,C>(addr); NEXT; }
			case 0x82: { II ii = res_N_xix_R<0,D>(addr); NEXT; }
			case 0x83: { II ii = res_N_xix_R<0,E>(addr); NEXT; }
			case 0x84: { II ii = res_N_xix_R<0,H>(addr); NEXT; }
			case 0x85: { II ii = res_N_xix_R<0,L>(addr); NEXT; }
			case 0x87: { II ii = res_N_xix_R<0,A>(addr); NEXT; }
			case 0x88: { II ii = res_N_xix_R<1,B>(addr); NEXT; }
			case 0x89: { II ii = res_N_xix_R<1,C>(addr); NEXT; }
			case 0x8a: { II ii = res_N_xix_R<1,D>(addr); NEXT; }
			case 0x8b: { II ii = res_N_xix_R<1,E>(addr); NEXT; }
			case 0x8c: { II ii = res_N_xix_R<1,H>(addr); NEXT; }
			case 0x8d: { II ii = res_N_xix_R<1,L>(addr); NEXT; }
			case 0x8f: { II ii = res_N_xix_R<1,A>(addr); NEXT; }
			case 0x90: { II ii = res_N_xix_R<2,B>(addr); NEXT; }
			case 0x91: { II ii = res_N_xix_R<2,C>(addr); NEXT; }
			case 0x92: { II ii = res_N_xix_R<2,D>(addr); NEXT; }
			case 0x93: { II ii = res_N_xix_R<2,E>(addr); NEXT; }
			case 0x94: { II ii = res_N_xix_R<2,H>(addr); NEXT; }
			case 0x95: { II ii = res_N_xix_R<2,L>(addr); NEXT; }
			case 0x97: { II ii = res_N_xix_R<2,A>(addr); NEXT; }
			case 0x98: { II ii = res_N_xix_R<3,B>(addr); NEXT; }
			case 0x99: { II ii = res_N_xix_R<3,C>(addr); NEXT; }
			case 0x9a: { II ii = res_N_xix_R<3,D>(addr); NEXT; }
			case 0x9b: { II ii = res_N_xix_R<3,E>(addr); NEXT; }
			case 0x9c: { II ii = res_N_xix_R<3,H>(addr); NEXT; }
			case 0x9d: { II ii = res_N_xix_R<3,L>(addr); NEXT; }
			case 0x9f: { II ii = res_N_xix_R<3,A>(addr); NEXT; }
			case 0xa0: { II ii = res_N_xix_R<4,B>(addr); NEXT; }
			case 0xa1: { II ii = res_N_xix_R<4,C>(addr); NEXT; }
			case 0xa2: { II ii = res_N_xix_R<4,D>(addr); NEXT; }
			case 0xa3: { II ii = res_N_xix_R<4,E>(addr); NEXT; }
			case 0xa4: { II ii = res_N_xix_R<4,H>(addr); NEXT; }
			case 0xa5: { II ii = res_N_xix_R<4,L>(addr); NEXT; }
			case 0xa7: { II ii = res_N_xix_R<4,A>(addr); NEXT; }
			case 0xa8: { II ii = res_N_xix_R<5,B>(addr); NEXT; }
			case 0xa9: { II ii = res_N_xix_R<5,C>(addr); NEXT; }
			case 0xaa: { II ii = res_N_xix_R<5,D>(addr); NEXT; }
			case 0xab: { II ii = res_N_xix_R<5,E>(addr); NEXT; }
			case 0xac: { II ii = res_N_xix_R<5,H>(addr); NEXT; }
			case 0xad: { II ii = res_N_xix_R<5,L>(addr); NEXT; }
			case 0xaf: { II ii = res_N_xix_R<5,A>(addr); NEXT; }
			case 0xb0: { II ii = res_N_xix_R<6,B>(addr); NEXT; }
			case 0xb1: { II ii = res_N_xix_R<6,C>(addr); NEXT; }
			case 0xb2: { II ii = res_N_xix_R<6,D>(addr); NEXT; }
			case 0xb3: { II ii = res_N_xix_R<6,E>(addr); NEXT; }
			case 0xb4: { II ii = res_N_xix_R<6,H>(addr); NEXT; }
			case 0xb5: { II ii = res_N_xix_R<6,L>(addr); NEXT; }
			case 0xb7: { II ii = res_N_xix_R<6,A>(addr); NEXT; }
			case 0xb8: { II ii = res_N_xix_R<7,B>(addr); NEXT; }
			case 0xb9: { II ii = res_N_xix_R<7,C>(addr); NEXT; }
			case 0xba: { II ii = res_N_xix_R<7,D>(addr); NEXT; }
			case 0xbb: { II ii = res_N_xix_R<7,E>(addr); NEXT; }
			case 0xbc: { II ii = res_N_xix_R<7,H>(addr); NEXT; }
			case 0xbd: { II ii = res_N_xix_R<7,L>(addr); NEXT; }
			case 0xbf: { II ii = res_N_xix_R<7,A>(addr); NEXT; }
			case 0x86: { II ii = res_N_xix_R<0,DUMMY>(addr); NEXT; }
			case 0x8e: { II ii = res_N_xix_R<1,DUMMY>(addr); NEXT; }
			case 0x96: { II ii = res_N_xix_R<2,DUMMY>(addr); NEXT; }
			case 0x9e: { II ii = res_N_xix_R<3,DUMMY>(addr); NEXT; }
			case 0xa6: { II ii = res_N_xix_R<4,DUMMY>(addr); NEXT; }
			case 0xae: { II ii = res_N_xix_R<5,DUMMY>(addr); NEXT; }
			case 0xb6: { II ii = res_N_xix_R<6,DUMMY>(addr); NEXT; }
			case 0xbe: { II ii = res_N_xix_R<7,DUMMY>(addr); NEXT; }

			case 0xc0: { II ii = set_N_xix_R<0,B>(addr); NEXT; }
			case 0xc1: { II ii = set_N_xix_R<0,C>(addr); NEXT; }
			case 0xc2: { II ii = set_N_xix_R<0,D>(addr); NEXT; }
			case 0xc3: { II ii = set_N_xix_R<0,E>(addr); NEXT; }
			case 0xc4: { II ii = set_N_xix_R<0,H>(addr); NEXT; }
			case 0xc5: { II ii = set_N_xix_R<0,L>(addr); NEXT; }
			case 0xc7: { II ii = set_N_xix_R<0,A>(addr); NEXT; }
			case 0xc8: { II ii = set_N_xix_R<1,B>(addr); NEXT; }
			case 0xc9: { II ii = set_N_xix_R<1,C>(addr); NEXT; }
			case 0xca: { II ii = set_N_xix_R<1,D>(addr); NEXT; }
			case 0xcb: { II ii = set_N_xix_R<1,E>(addr); NEXT; }
			case 0xcc: { II ii = set_N_xix_R<1,H>(addr); NEXT; }
			case 0xcd: { II ii = set_N_xix_R<1,L>(addr); NEXT; }
			case 0xcf: { II ii = set_N_xix_R<1,A>(addr); NEXT; }
			case 0xd0: { II ii = set_N_xix_R<2,B>(addr); NEXT; }
			case 0xd1: { II ii = set_N_xix_R<2,C>(addr); NEXT; }
			case 0xd2: { II ii = set_N_xix_R<2,D>(addr); NEXT; }
			case 0xd3: { II ii = set_N_xix_R<2,E>(addr); NEXT; }
			case 0xd4: { II ii = set_N_xix_R<2,H>(addr); NEXT; }
			case 0xd5: { II ii = set_N_xix_R<2,L>(addr); NEXT; }
			case 0xd7: { II ii = set_N_xix_R<2,A>(addr); NEXT; }
			case 0xd8: { II ii = set_N_xix_R<3,B>(addr); NEXT; }
			case 0xd9: { II ii = set_N_xix_R<3,C>(addr); NEXT; }
			case 0xda: { II ii = set_N_xix_R<3,D>(addr); NEXT; }
			case 0xdb: { II ii = set_N_xix_R<3,E>(addr); NEXT; }
			case 0xdc: { II ii = set_N_xix_R<3,H>(addr); NEXT; }
			case 0xdd: { II ii = set_N_xix_R<3,L>(addr); NEXT; }
			case 0xdf: { II ii = set_N_xix_R<3,A>(addr); NEXT; }
			case 0xe0: { II ii = set_N_xix_R<4,B>(addr); NEXT; }
			case 0xe1: { II ii = set_N_xix_R<4,C>(addr); NEXT; }
			case 0xe2: { II ii = set_N_xix_R<4,D>(addr); NEXT; }
			case 0xe3: { II ii = set_N_xix_R<4,E>(addr); NEXT; }
			case 0xe4: { II ii = set_N_xix_R<4,H>(addr); NEXT; }
			case 0xe5: { II ii = set_N_xix_R<4,L>(addr); NEXT; }
			case 0xe7: { II ii = set_N_xix_R<4,A>(addr); NEXT; }
			case 0xe8: { II ii = set_N_xix_R<5,B>(addr); NEXT; }
			case 0xe9: { II ii = set_N_xix_R<5,C>(addr); NEXT; }
			case 0xea: { II ii = set_N_xix_R<5,D>(addr); NEXT; }
			case 0xeb: { II ii = set_N_xix_R<5,E>(addr); NEXT; }
			case 0xec: { II ii = set_N_xix_R<5,H>(addr); NEXT; }
			case 0xed: { II ii = set_N_xix_R<5,L>(addr); NEXT; }
			case 0xef: { II ii = set_N_xix_R<5,A>(addr); NEXT; }
			case 0xf0: { II ii = set_N_xix_R<6,B>(addr); NEXT; }
			case 0xf1: { II ii = set_N_xix_R<6,C>(addr); NEXT; }
			case 0xf2: { II ii = set_N_xix_R<6,D>(addr); NEXT; }
			case 0xf3: { II ii = set_N_xix_R<6,E>(addr); NEXT; }
			case 0xf4: { II ii = set_N_xix_R<6,H>(addr); NEXT; }
			case 0xf5: { II ii = set_N_xix_R<6,L>(addr); NEXT; }
			case 0xf7: { II ii = set_N_xix_R<6,A>(addr); NEXT; }
			case 0xf8: { II ii = set_N_xix_R<7,B>(addr); NEXT; }
			case 0xf9: { II ii = set_N_xix_R<7,C>(addr); NEXT; }
			case 0xfa: { II ii = set_N_xix_R<7,D>(addr); NEXT; }
			case 0xfb: { II ii = set_N_xix_R<7,E>(addr); NEXT; }
			case 0xfc: { II ii = set_N_xix_R<7,H>(addr); NEXT; }
			case 0xfd: { II ii = set_N_xix_R<7,L>(addr); NEXT; }
			case 0xff: { II ii = set_N_xix_R<7,A>(addr); NEXT; }
			case 0xc6: { II ii = set_N_xix_R<0,DUMMY>(addr); NEXT; }
			case 0xce: { II ii = set_N_xix_R<1,DUMMY>(addr); NEXT; }
			case 0xd6: { II ii = set_N_xix_R<2,DUMMY>(addr); NEXT; }
			case 0xde: { II ii = set_N_xix_R<3,DUMMY>(addr); NEXT; }
			case 0xe6: { II ii = set_N_xix_R<4,DUMMY>(addr); NEXT; }
			case 0xee: { II ii = set_N_xix_R<5,DUMMY>(addr); NEXT; }
			case 0xf6: { II ii = set_N_xix_R<6,DUMMY>(addr); NEXT; }
			case 0xfe: { II ii = set_N_xix_R<7,DUMMY>(addr); NEXT; }
			default: UNREACHABLE;
		}
	}
}

template<typename T> inline void CPUCore<T>::cpuTracePre()
{
	start_pc = getPC();
}
template<typename T> inline void CPUCore<T>::cpuTracePost()
{
	if (unlikely(tracingEnabled)) {
		cpuTracePost_slow();
	}
}
template<typename T> void CPUCore<T>::cpuTracePost_slow()
{
	byte opBuf[4];
	std::string dasmOutput;
	dasm(*interface, start_pc, opBuf, dasmOutput, T::getTimeFast());
	std::cout << strCat(hex_string<4>(start_pc),
	                    " : ", dasmOutput,
	                    " AF=", hex_string<4>(getAF()),
	                    " BC=", hex_string<4>(getBC()),
	                    " DE=", hex_string<4>(getDE()),
	                    " HL=", hex_string<4>(getHL()),
	                    " IX=", hex_string<4>(getIX()),
	                    " IY=", hex_string<4>(getIY()),
	                    " SP=", hex_string<4>(getSP()),
	                    '\n')
	          << std::flush;
}

template<typename T> ExecIRQ CPUCore<T>::getExecIRQ() const
{
	if (unlikely(nmiEdge)) return ExecIRQ::NMI;
	if (unlikely(IRQStatus && getIFF1() && !prevWasEI())) return ExecIRQ::IRQ;
	return ExecIRQ::NONE;
}

template<typename T> void CPUCore<T>::executeSlow(ExecIRQ execIRQ)
{
	if (unlikely(execIRQ == ExecIRQ::NMI)) {
		nmiEdge = false;
		nmi(); // NMI occurred
	} else if (unlikely(execIRQ == ExecIRQ::IRQ)) {
		// normal interrupt
		if (unlikely(prevWasLDAI())) {
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
			assert(getF() & V_FLAG);
			setF(getF() & ~V_FLAG);
		}
		IRQAccept.signal();
		switch (getIM()) {
			case 0: irq0();
				break;
			case 1: irq1();
				break;
			case 2: irq2();
				break;
			default:
				UNREACHABLE;
		}
	} else if (unlikely(getHALT())) {
		// in halt mode
		incR(T::advanceHalt(T::HALT_STATES, scheduler.getNext()));
		setSlowInstructions();
	} else {
		cpuTracePre();
		assert(T::limitReached()); // we want only one instruction
		executeInstructions();
		endInstruction();

		if constexpr (T::IS_R800) {
			if (unlikely(prev2WasCall()) && likely(!prevWasPopRet())) {
				// On R800 a CALL or RST instruction not _immediately_
				// followed by a (single-byte) POP or RET instruction
				// causes an extra cycle in that following instruction.
				// No idea why yet. See doc/internal/r800-call.txt
				// for more information.
				//
				// TODO this implementation adds the extra cycle at
				// the end of the instruction POP/RET. It is not known
				// where in the instruction the real R800 adds this cycle.
				T::add(1);
			}
		}
		cpuTracePost();
	}
}

template<typename T> void CPUCore<T>::execute(bool fastForward)
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

template<typename T> void CPUCore<T>::execute2(bool fastForward)
{
	// note: Don't use getTimeFast() here, because 'once in a while' we
	//       need to CPUClock::sync() to avoid overflow.
	//       Should be done at least once per second (approx). So only
	//       once in this method is enough.
	scheduler.schedule(T::getTime());
	setSlowInstructions();

	// Note: we call scheduler _after_ executing the instruction and before
	// deciding between executeFast() and executeSlow() (because a
	// SyncPoint could set an IRQ and then we must choose executeSlow())
	if (fastForward ||
	    (!interface->anyBreakPoints() && !tracingEnabled)) {
		// fast path, no breakpoints, no tracing
		do {
			if (slowInstructions) {
				--slowInstructions;
				executeSlow(getExecIRQ());
				scheduler.schedule(T::getTimeFast());
			} else {
				while (slowInstructions == 0) {
					T::enableLimit(); // does CPUClock::sync()
					if (likely(!T::limitReached())) {
						// multiple instructions
						executeInstructions();
						// note: pipeline only shifted one
						// step for multiple instructions
						endInstruction();
					}
					scheduler.schedule(T::getTimeFast());
					if (needExitCPULoop()) return;
				}
			}
		} while (!needExitCPULoop());
	} else {
		do {
			if (slowInstructions == 0) {
				cpuTracePre();
				assert(T::limitReached()); // only one instruction
				executeInstructions();
				endInstruction();
				cpuTracePost();
			} else {
				--slowInstructions;
				executeSlow(getExecIRQ());
			}
			// Don't use getTimeFast() here, we need a call to
			// CPUClock::sync() 'once in a while'. (During a
			// reverse fast-forward this wasn't always the case).
			scheduler.schedule(T::getTime());

			// Only check for breakpoints when we're not about to jump to an IRQ handler.
			//
			// This fixes the following problem reported by Grauw:
			//
			//   I found a breakpoints bug: sometimes a breakpoint gets hit twice even
			//   though the code is executed once. This manifests itself in my profiler
			//   as an imbalance between section begin- and end-calls.
			//
			//   Turns out this occurs when an interrupt occurs exactly on the line of
			//   the breakpoint, then the breakpoint gets hit before immediately going
			//   to the ISR, as well as when returning from the ISR.
			//
			//   The IRQ is handled by the Z80 at the end of an instruction. So it
			//   should change the PC before the next instruction is fetched and the
			//   breakpoints should be evaluated during instruction fetch.
			//
			// I think Grauw's analysis is correct. Though for performance reasons we
			// don't emulate the Z80 like that: we don't check for IRQs at the end of
			// every instruction. In the openMSX emulation model, we can only enter an
			// ISR:
			//  - (One instruction after) switching from DI to EI mode.
			//  - After emulating device code. This can be:
			//    * When the Z80 communicated with the device (IO or memory mapped IO).
			//    * The device had set a synchronization point.
			//  In all cases disableLimit() gets called which will cause
			//  limitReached() to return true (and possibly slowInstructions to be > 0).
			// So after most emulated Z80 instructions there can't be a pending IRQ, so
			// checking for it is wasteful. Also synchronization points are handled
			// between emulated Z80 instructions, that means me must check for pending
			// IRQs at the start (instead of end) of an instruction.
			//
			auto execIRQ = getExecIRQ();
			if ((execIRQ == ExecIRQ::NONE) &&
			    interface->checkBreakPoints(getPC(), motherboard)) {
				assert(interface->isBreaked());
				break;
			}
		} while (!needExitCPULoop());
	}
}

template<typename T> template<Reg8 R8> ALWAYS_INLINE byte CPUCore<T>::get8() const {
	if      constexpr (R8 == A)     { return getA(); }
	else if constexpr (R8 == F)     { return getF(); }
	else if constexpr (R8 == B)     { return getB(); }
	else if constexpr (R8 == C)     { return getC(); }
	else if constexpr (R8 == D)     { return getD(); }
	else if constexpr (R8 == E)     { return getE(); }
	else if constexpr (R8 == H)     { return getH(); }
	else if constexpr (R8 == L)     { return getL(); }
	else if constexpr (R8 == IXH)   { return getIXh(); }
	else if constexpr (R8 == IXL)   { return getIXl(); }
	else if constexpr (R8 == IYH)   { return getIYh(); }
	else if constexpr (R8 == IYL)   { return getIYl(); }
	else if constexpr (R8 == REG_I) { return getI(); }
	else if constexpr (R8 == REG_R) { return getR(); }
	else if constexpr (R8 == DUMMY) { return 0; }
	else { UNREACHABLE; return 0; }
}
template<typename T> template<Reg16 R16> ALWAYS_INLINE unsigned CPUCore<T>::get16() const {
	if      constexpr (R16 == AF) { return getAF(); }
	else if constexpr (R16 == BC) { return getBC(); }
	else if constexpr (R16 == DE) { return getDE(); }
	else if constexpr (R16 == HL) { return getHL(); }
	else if constexpr (R16 == IX) { return getIX(); }
	else if constexpr (R16 == IY) { return getIY(); }
	else if constexpr (R16 == SP) { return getSP(); }
	else { UNREACHABLE; return 0; }
}
template<typename T> template<Reg8 R8> ALWAYS_INLINE void CPUCore<T>::set8(byte x) {
	if      constexpr (R8 == A)     { setA(x); }
	else if constexpr (R8 == F)     { setF(x); }
	else if constexpr (R8 == B)     { setB(x); }
	else if constexpr (R8 == C)     { setC(x); }
	else if constexpr (R8 == D)     { setD(x); }
	else if constexpr (R8 == E)     { setE(x); }
	else if constexpr (R8 == H)     { setH(x); }
	else if constexpr (R8 == L)     { setL(x); }
	else if constexpr (R8 == IXH)   { setIXh(x); }
	else if constexpr (R8 == IXL)   { setIXl(x); }
	else if constexpr (R8 == IYH)   { setIYh(x); }
	else if constexpr (R8 == IYL)   { setIYl(x); }
	else if constexpr (R8 == REG_I) { setI(x); }
	else if constexpr (R8 == REG_R) { setR(x); }
	else if constexpr (R8 == DUMMY) { /* nothing */ }
	else { UNREACHABLE; }
}
template<typename T> template<Reg16 R16> ALWAYS_INLINE void CPUCore<T>::set16(unsigned x) {
	if      constexpr (R16 == AF) { setAF(x); }
	else if constexpr (R16 == BC) { setBC(x); }
	else if constexpr (R16 == DE) { setDE(x); }
	else if constexpr (R16 == HL) { setHL(x); }
	else if constexpr (R16 == IX) { setIX(x); }
	else if constexpr (R16 == IY) { setIY(x); }
	else if constexpr (R16 == SP) { setSP(x); }
	else { UNREACHABLE; }
}

// LD r,r
template<typename T> template<Reg8 DST, Reg8 SRC, int EE> II CPUCore<T>::ld_R_R() {
	set8<DST>(get8<SRC>()); return {1, T::CC_LD_R_R + EE};
}

// LD SP,ss
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::ld_sp_SS() {
	setSP(get16<REG>()); return {1, T::CC_LD_SP_HL + EE};
}

// LD (ss),a
template<typename T> template<Reg16 REG> II CPUCore<T>::ld_SS_a() {
	T::setMemPtr((getA() << 8) | ((get16<REG>() + 1) & 0xFF));
	WRMEM(get16<REG>(), getA(), T::CC_LD_SS_A_1);
	return {1, T::CC_LD_SS_A};
}

// LD (HL),r
template<typename T> template<Reg8 SRC> II CPUCore<T>::ld_xhl_R() {
	WRMEM(getHL(), get8<SRC>(), T::CC_LD_HL_R_1);
	return {1, T::CC_LD_HL_R};
}

// LD (IXY+e),r
template<typename T> template<Reg16 IXY, Reg8 SRC> II CPUCore<T>::ld_xix_R() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_LD_XIX_R_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	WRMEM(addr, get8<SRC>(), T::CC_DD + T::CC_LD_XIX_R_2);
	return {2, T::CC_DD + T::CC_LD_XIX_R};
}

// LD (HL),n
template<typename T> II CPUCore<T>::ld_xhl_byte() {
	byte val = RDMEM_OPCODE<1>(T::CC_LD_HL_N_1);
	WRMEM(getHL(), val, T::CC_LD_HL_N_2);
	return {2, T::CC_LD_HL_N};
}

// LD (IXY+e),n
template<typename T> template<Reg16 IXY> II CPUCore<T>::ld_xix_byte() {
	unsigned tmp = RD_WORD_PC<1>(T::CC_DD + T::CC_LD_XIX_N_1);
	int8_t ofst = tmp & 0xFF;
	byte val = tmp >> 8;
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	WRMEM(addr, val, T::CC_DD + T::CC_LD_XIX_N_2);
	return {3, T::CC_DD + T::CC_LD_XIX_N};
}

// LD (nn),A
template<typename T> II CPUCore<T>::ld_xbyte_a() {
	unsigned x = RD_WORD_PC<1>(T::CC_LD_NN_A_1);
	T::setMemPtr((getA() << 8) | ((x + 1) & 0xFF));
	WRMEM(x, getA(), T::CC_LD_NN_A_2);
	return {3, T::CC_LD_NN_A};
}

// LD (nn),ss
template<typename T> template<int EE> inline II CPUCore<T>::WR_NN_Y(unsigned reg) {
	unsigned addr = RD_WORD_PC<1>(T::CC_LD_XX_HL_1 + EE);
	T::setMemPtr(addr + 1);
	WR_WORD(addr, reg, T::CC_LD_XX_HL_2 + EE);
	return {3, T::CC_LD_XX_HL + EE};
}
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::ld_xword_SS() {
	return WR_NN_Y<EE      >(get16<REG>());
}
template<typename T> template<Reg16 REG> II CPUCore<T>::ld_xword_SS_ED() {
	return WR_NN_Y<T::EE_ED>(get16<REG>());
}

// LD A,(ss)
template<typename T> template<Reg16 REG> II CPUCore<T>::ld_a_SS() {
	T::setMemPtr(get16<REG>() + 1);
	setA(RDMEM(get16<REG>(), T::CC_LD_A_SS_1));
	return {1, T::CC_LD_A_SS};
}

// LD A,(nn)
template<typename T> II CPUCore<T>::ld_a_xbyte() {
	unsigned addr = RD_WORD_PC<1>(T::CC_LD_A_NN_1);
	T::setMemPtr(addr + 1);
	setA(RDMEM(addr, T::CC_LD_A_NN_2));
	return {3, T::CC_LD_A_NN};
}

// LD r,n
template<typename T> template<Reg8 DST, int EE> II CPUCore<T>::ld_R_byte() {
	set8<DST>(RDMEM_OPCODE<1>(T::CC_LD_R_N_1 + EE)); return {2, T::CC_LD_R_N + EE};
}

// LD r,(hl)
template<typename T> template<Reg8 DST> II CPUCore<T>::ld_R_xhl() {
	set8<DST>(RDMEM(getHL(), T::CC_LD_R_HL_1)); return {1, T::CC_LD_R_HL};
}

// LD r,(IXY+e)
template<typename T> template<Reg8 DST, Reg16 IXY> II CPUCore<T>::ld_R_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_LD_R_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	set8<DST>(RDMEM(addr, T::CC_DD + T::CC_LD_R_XIX_2));
	return {2, T::CC_DD + T::CC_LD_R_XIX};
}

// LD ss,(nn)
template<typename T> template<int EE> inline unsigned CPUCore<T>::RD_P_XX() {
	unsigned addr = RD_WORD_PC<1>(T::CC_LD_HL_XX_1 + EE);
	T::setMemPtr(addr + 1);
	unsigned result = RD_WORD(addr, T::CC_LD_HL_XX_2 + EE);
	return result;
}
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::ld_SS_xword() {
	set16<REG>(RD_P_XX<EE>());       return {3, T::CC_LD_HL_XX + EE};
}
template<typename T> template<Reg16 REG> II CPUCore<T>::ld_SS_xword_ED() {
	set16<REG>(RD_P_XX<T::EE_ED>()); return {3, T::CC_LD_HL_XX + T::EE_ED};
}

// LD ss,nn
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::ld_SS_word() {
	set16<REG>(RD_WORD_PC<1>(T::CC_LD_SS_NN_1 + EE)); return {3, T::CC_LD_SS_NN + EE};
}


// ADC A,r
template<typename T> inline void CPUCore<T>::ADC(byte reg) {
	unsigned res = getA() + reg + ((getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         ((getA() ^ res ^ reg) & H_FLAG) |
	         (((getA() ^ res) & (reg ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= table.ZS[res & 0xFF];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSXY[res & 0xFF];
	}
	setF(f);
	setA(res);
}
template<typename T> inline II CPUCore<T>::adc_a_a() {
	unsigned res = 2 * getA() + ((getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         (res & H_FLAG) |
	         (((getA() ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= table.ZS[res & 0xFF];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSXY[res & 0xFF];
	}
	setF(f);
	setA(res);
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::adc_a_R() {
	ADC(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::adc_a_byte() {
	ADC(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::adc_a_xhl() {
	ADC(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::adc_a_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	ADC(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}

// ADD A,r
template<typename T> inline void CPUCore<T>::ADD(byte reg) {
	unsigned res = getA() + reg;
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         ((getA() ^ res ^ reg) & H_FLAG) |
	         (((getA() ^ res) & (reg ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= table.ZS[res & 0xFF];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSXY[res & 0xFF];
	}
	setF(f);
	setA(res);
}
template<typename T> inline II CPUCore<T>::add_a_a() {
	unsigned res = 2 * getA();
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         (res & H_FLAG) |
	         (((getA() ^ res) & 0x80) >> 5) | // V_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= table.ZS[res & 0xFF];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSXY[res & 0xFF];
	}
	setF(f);
	setA(res);
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::add_a_R() {
	ADD(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::add_a_byte() {
	ADD(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::add_a_xhl() {
	ADD(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::add_a_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	ADD(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}

// AND r
template<typename T> inline void CPUCore<T>::AND(byte reg) {
	setA(getA() & reg);
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSPH[getA()];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[getA()] | H_FLAG;
	}
	setF(f);
}
template<typename T> II CPUCore<T>::and_a() {
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSPH[getA()];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[getA()] | H_FLAG;
	}
	setF(f);
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::and_R() {
	AND(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::and_byte() {
	AND(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::and_xhl() {
	AND(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::and_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	AND(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}

// CP r
template<typename T> inline void CPUCore<T>::CP(byte reg) {
	unsigned q = getA() - reg;
	byte f = table.ZS[q & 0xFF] |
	         ((q & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((getA() ^ q ^ reg) & H_FLAG) |
	         (((reg ^ getA()) & (getA() ^ q) & 0x80) >> 5); // V_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= reg & (X_FLAG | Y_FLAG); // XY from operand, not from result
	}
	setF(f);
}
template<typename T> II CPUCore<T>::cp_a() {
	byte f = ZS0 | N_FLAG;
	if constexpr (T::IS_R800) {
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= getA() & (X_FLAG | Y_FLAG); // XY from operand, not from result
	}
	setF(f);
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::cp_R() {
	CP(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::cp_byte() {
	CP(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::cp_xhl() {
	CP(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::cp_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	CP(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}

// OR r
template<typename T> inline void CPUCore<T>::OR(byte reg) {
	setA(getA() | reg);
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[getA()];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[getA()];
	}
	setF(f);
}
template<typename T> II CPUCore<T>::or_a() {
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[getA()];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[getA()];
	}
	setF(f);
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::or_R() {
	OR(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::or_byte() {
	OR(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::or_xhl() {
	OR(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::or_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	OR(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}

// SBC A,r
template<typename T> inline void CPUCore<T>::SBC(byte reg) {
	unsigned res = getA() - reg - ((getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((getA() ^ res ^ reg) & H_FLAG) |
	         (((reg ^ getA()) & (getA() ^ res) & 0x80) >> 5); // V_FLAG
	if constexpr (T::IS_R800) {
		f |= table.ZS[res & 0xFF];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSXY[res & 0xFF];
	}
	setF(f);
	setA(res);
}
template<typename T> II CPUCore<T>::sbc_a_a() {
	if constexpr (T::IS_R800) {
		word t = (getF() & C_FLAG)
		       ? (255 * 256 | ZS255 | C_FLAG | H_FLAG | N_FLAG)
		       : (  0 * 256 | ZS0   |                   N_FLAG);
		setAF(t | (getF() & (X_FLAG | Y_FLAG)));
	} else {
		setAF((getF() & C_FLAG) ?
			(255 * 256 | ZSXY255 | C_FLAG | H_FLAG | N_FLAG) :
			(  0 * 256 | ZSXY0   |                   N_FLAG));
	}
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::sbc_a_R() {
	SBC(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::sbc_a_byte() {
	SBC(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::sbc_a_xhl() {
	SBC(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::sbc_a_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	SBC(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}

// SUB r
template<typename T> inline void CPUCore<T>::SUB(byte reg) {
	unsigned res = getA() - reg;
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((getA() ^ res ^ reg) & H_FLAG) |
	         (((reg ^ getA()) & (getA() ^ res) & 0x80) >> 5); // V_FLAG
	if constexpr (T::IS_R800) {
		f |= table.ZS[res & 0xFF];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSXY[res & 0xFF];
	}
	setF(f);
	setA(res);
}
template<typename T> II CPUCore<T>::sub_a() {
	if constexpr (T::IS_R800) {
		word t = 0 * 256 | ZS0 | N_FLAG;
		setAF(t | (getF() & (X_FLAG | Y_FLAG)));
	} else {
		setAF(0 * 256 | ZSXY0 | N_FLAG);
	}
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::sub_R() {
	SUB(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::sub_byte() {
	SUB(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::sub_xhl() {
	SUB(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::sub_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	SUB(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}

// XOR r
template<typename T> inline void CPUCore<T>::XOR(byte reg) {
	setA(getA() ^ reg);
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[getA()];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[getA()];
	}
	setF(f);
}
template<typename T> II CPUCore<T>::xor_a() {
	if constexpr (T::IS_R800) {
		word t = 0 * 256 + ZSP0;
		setAF(t | (getF() & (X_FLAG | Y_FLAG)));
	} else {
		setAF(0 * 256 + ZSPXY0);
	}
	return {1, T::CC_CP_R};
}
template<typename T> template<Reg8 SRC, int EE> II CPUCore<T>::xor_R() {
	XOR(get8<SRC>()); return {1, T::CC_CP_R + EE};
}
template<typename T> II CPUCore<T>::xor_byte() {
	XOR(RDMEM_OPCODE<1>(T::CC_CP_N_1)); return {2, T::CC_CP_N};
}
template<typename T> II CPUCore<T>::xor_xhl() {
	XOR(RDMEM(getHL(), T::CC_CP_XHL_1)); return {1, T::CC_CP_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::xor_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_CP_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	XOR(RDMEM(addr, T::CC_DD + T::CC_CP_XIX_2));
	return {2, T::CC_DD + T::CC_CP_XIX};
}


// DEC r
template<typename T> inline byte CPUCore<T>::DEC(byte reg) {
	byte res = reg - 1;
	byte f = ((reg & ~res & 0x80) >> 5) | // V_FLAG
	         (((res & 0x0F) + 1) & H_FLAG) |
	         N_FLAG;
	if constexpr (T::IS_R800) {
		f |= getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= table.ZS[res];
	} else {
		f |= getF() & C_FLAG;
		f |= table.ZSXY[res];
	}
	setF(f);
	return res;
}
template<typename T> template<Reg8 REG, int EE> II CPUCore<T>::dec_R() {
	set8<REG>(DEC(get8<REG>())); return {1, T::CC_INC_R + EE};
}
template<typename T> template<int EE> inline void CPUCore<T>::DEC_X(unsigned x) {
	byte val = DEC(RDMEM(x, T::CC_INC_XHL_1 + EE));
	WRMEM(x, val, T::CC_INC_XHL_2 + EE);
}
template<typename T> II CPUCore<T>::dec_xhl() {
	DEC_X<0>(getHL());
	return {1, T::CC_INC_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::dec_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_INC_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	DEC_X<T::CC_DD + T::EE_INC_XIX>(addr);
	return {2, T::CC_INC_XHL + T::CC_DD + T::EE_INC_XIX};
}

// INC r
template<typename T> inline byte CPUCore<T>::INC(byte reg) {
	reg++;
	byte f = ((reg & -reg & 0x80) >> 5) | // V_FLAG
	         (((reg & 0x0F) - 1) & H_FLAG) |
		 0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= table.ZS[reg];
	} else {
		f |= getF() & C_FLAG;
		f |= table.ZSXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<Reg8 REG, int EE> II CPUCore<T>::inc_R() {
	set8<REG>(INC(get8<REG>())); return {1, T::CC_INC_R + EE};
}
template<typename T> template<int EE> inline void CPUCore<T>::INC_X(unsigned x) {
	byte val = INC(RDMEM(x, T::CC_INC_XHL_1 + EE));
	WRMEM(x, val, T::CC_INC_XHL_2 + EE);
}
template<typename T> II CPUCore<T>::inc_xhl() {
	INC_X<0>(getHL());
	return {1, T::CC_INC_XHL};
}
template<typename T> template<Reg16 IXY> II CPUCore<T>::inc_xix() {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_DD + T::CC_INC_XIX_1);
	unsigned addr = (get16<IXY>() + ofst) & 0xFFFF;
	T::setMemPtr(addr);
	INC_X<T::CC_DD + T::EE_INC_XIX>(addr);
	return {2, T::CC_INC_XHL + T::CC_DD + T::EE_INC_XIX};
}


// ADC HL,ss
template<typename T> template<Reg16 REG> inline II CPUCore<T>::adc_hl_SS() {
	unsigned reg = get16<REG>();
	T::setMemPtr(getHL() + 1);
	unsigned res = getHL() + reg + ((getF() & C_FLAG) ? 1 : 0);
	byte f = (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (X_FLAG | Y_FLAG);
	}
	if (res & 0xFFFF) {
		f |= ((getHL() ^ res ^ reg) >> 8) & H_FLAG;
		f |= 0; // Z_FLAG
		f |= ((getHL() ^ res) & (reg ^ res) & 0x8000) >> 13; // V_FLAG
		if constexpr (T::IS_R800) {
			f |= (res >> 8) & S_FLAG;
		} else {
			f |= (res >> 8) & (S_FLAG | X_FLAG | Y_FLAG);
		}
	} else {
		f |= ((getHL() ^ reg) >> 8) & H_FLAG;
		f |= Z_FLAG;
		f |= (getHL() & reg & 0x8000) >> 13; // V_FLAG
		f |= 0; // S_FLAG (X_FLAG Y_FLAG)
	}
	setF(f);
	setHL(res);
	return {1, T::CC_ADC_HL_SS};
}
template<typename T> II CPUCore<T>::adc_hl_hl() {
	T::setMemPtr(getHL() + 1);
	unsigned res = 2 * getHL() + ((getF() & C_FLAG) ? 1 : 0);
	byte f = (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (X_FLAG | Y_FLAG);
	}
	if (res & 0xFFFF) {
		f |= 0; // Z_FLAG
		f |= ((getHL() ^ res) & 0x8000) >> 13; // V_FLAG
		if constexpr (T::IS_R800) {
			f |= (res >> 8) & (H_FLAG | S_FLAG);
		} else {
			f |= (res >> 8) & (H_FLAG | S_FLAG | X_FLAG | Y_FLAG);
		}
	} else {
		f |= Z_FLAG;
		f |= (getHL() & 0x8000) >> 13; // V_FLAG
		f |= 0; // H_FLAG S_FLAG (X_FLAG Y_FLAG)
	}
	setF(f);
	setHL(res);
	return {1, T::CC_ADC_HL_SS};
}

// ADD HL/IX/IY,ss
template<typename T> template<Reg16 REG1, Reg16 REG2, int EE> II CPUCore<T>::add_SS_TT() {
	unsigned reg1 = get16<REG1>();
	unsigned reg2 = get16<REG2>();
	T::setMemPtr(reg1 + 1);
	unsigned res = reg1 + reg2;
	byte f = (((reg1 ^ res ^ reg2) >> 8) & H_FLAG) |
	         (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | V_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | V_FLAG);
		f |= (res >> 8) & (X_FLAG | Y_FLAG);
	}
	setF(f);
	set16<REG1>(res & 0xFFFF);
	return {1, T::CC_ADD_HL_SS + EE};
}
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::add_SS_SS() {
	unsigned reg = get16<REG>();
	T::setMemPtr(reg + 1);
	unsigned res = 2 * reg;
	byte f = (res >> 16) | // C_FLAG
	         0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | V_FLAG | X_FLAG | Y_FLAG);
		f |= (res >> 8) & H_FLAG;
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | V_FLAG);
		f |= (res >> 8) & (H_FLAG | X_FLAG | Y_FLAG);
	}
	setF(f);
	set16<REG>(res & 0xFFFF);
	return {1, T::CC_ADD_HL_SS + EE};
}

// SBC HL,ss
template<typename T> template<Reg16 REG> inline II CPUCore<T>::sbc_hl_SS() {
	unsigned reg = get16<REG>();
	T::setMemPtr(getHL() + 1);
	unsigned res = getHL() - reg - ((getF() & C_FLAG) ? 1 : 0);
	byte f = ((res & 0x10000) ? C_FLAG : 0) |
		 N_FLAG;
	if constexpr (T::IS_R800) {
		f |= getF() & (X_FLAG | Y_FLAG);
	}
	if (res & 0xFFFF) {
		f |= ((getHL() ^ res ^ reg) >> 8) & H_FLAG;
		f |= 0; // Z_FLAG
		f |= ((reg ^ getHL()) & (getHL() ^ res) & 0x8000) >> 13; // V_FLAG
		if constexpr (T::IS_R800) {
			f |= (res >> 8) & S_FLAG;
		} else {
			f |= (res >> 8) & (S_FLAG | X_FLAG | Y_FLAG);
		}
	} else {
		f |= ((getHL() ^ reg) >> 8) & H_FLAG;
		f |= Z_FLAG;
		f |= ((reg ^ getHL()) & getHL() & 0x8000) >> 13; // V_FLAG
		f |= 0; // S_FLAG (X_FLAG Y_FLAG)
	}
	setF(f);
	setHL(res);
	return {1, T::CC_ADC_HL_SS};
}
template<typename T> II CPUCore<T>::sbc_hl_hl() {
	T::setMemPtr(getHL() + 1);
	byte f = T::IS_R800 ? (getF() & (X_FLAG | Y_FLAG)) : 0;
	if (getF() & C_FLAG) {
		f |= C_FLAG | H_FLAG | S_FLAG | N_FLAG;
		if constexpr (!T::IS_R800) {
			f |= X_FLAG | Y_FLAG;
		}
		setHL(0xFFFF);
	} else {
		f |= Z_FLAG | N_FLAG;
		setHL(0);
	}
	setF(f);
	return {1, T::CC_ADC_HL_SS};
}

// DEC ss
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::dec_SS() {
	set16<REG>(get16<REG>() - 1); return {1, T::CC_INC_SS + EE};
}

// INC ss
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::inc_SS() {
	set16<REG>(get16<REG>() + 1); return {1, T::CC_INC_SS + EE};
}


// BIT n,r
template<typename T> template<unsigned N, Reg8 REG> II CPUCore<T>::bit_N_R() {
	byte reg = get8<REG>();
	byte f = 0; // N_FLAG
	if constexpr (T::IS_R800) {
		// this is very different from Z80 (not only XY flags)
		f |= getF() & (S_FLAG | V_FLAG | C_FLAG | X_FLAG | Y_FLAG);
		f |= H_FLAG;
		f |= (reg & (1 << N)) ? 0 : Z_FLAG;
	} else {
		f |= table.ZSPH[reg & (1 << N)];
		f |= getF() & C_FLAG;
		f |= reg & (X_FLAG | Y_FLAG);
	}
	setF(f);
	return {1, T::CC_BIT_R};
}
template<typename T> template<unsigned N> inline II CPUCore<T>::bit_N_xhl() {
	byte m = RDMEM(getHL(), T::CC_BIT_XHL_1) & (1 << N);
	byte f = 0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | V_FLAG | C_FLAG | X_FLAG | Y_FLAG);
		f |= H_FLAG;
		f |= m ? 0 : Z_FLAG;
	} else {
		f |= table.ZSPH[m];
		f |= getF() & C_FLAG;
		f |= (T::getMemPtr() >> 8) & (X_FLAG | Y_FLAG);
	}
	setF(f);
	return {1, T::CC_BIT_XHL};
}
template<typename T> template<unsigned N> inline II CPUCore<T>::bit_N_xix(unsigned addr) {
	T::setMemPtr(addr);
	byte m = RDMEM(addr, T::CC_DD + T::CC_BIT_XIX_1) & (1 << N);
	byte f = 0; // N_FLAG
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | V_FLAG | C_FLAG | X_FLAG | Y_FLAG);
		f |= H_FLAG;
		f |= m ? 0 : Z_FLAG;
	} else {
		f |= table.ZSPH[m];
		f |= getF() & C_FLAG;
		f |= (addr >> 8) & (X_FLAG | Y_FLAG);
	}
	setF(f);
	return {3, T::CC_DD + T::CC_BIT_XIX};
}

// RES n,r
static constexpr byte RES(unsigned b, byte reg) {
	return reg & ~(1 << b);
}
template<typename T> template<unsigned N, Reg8 REG> II CPUCore<T>::res_N_R() {
	set8<REG>(RES(N, get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> template<int EE> inline byte CPUCore<T>::RES_X(unsigned bit, unsigned addr) {
	byte res = RES(bit, RDMEM(addr, T::CC_SET_XHL_1 + EE));
	WRMEM(addr, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<unsigned N> II CPUCore<T>::res_N_xhl() {
	RES_X<0>(N, getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<unsigned N, Reg8 REG> II CPUCore<T>::res_N_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(RES_X<T::CC_DD + T::EE_SET_XIX>(N, a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// SET n,r
static constexpr byte SET(unsigned b, byte reg) {
	return reg | (1 << b);
}
template<typename T> template<unsigned N, Reg8 REG> II CPUCore<T>::set_N_R() {
	set8<REG>(SET(N, get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> template<int EE> inline byte CPUCore<T>::SET_X(unsigned bit, unsigned addr) {
	byte res = SET(bit, RDMEM(addr, T::CC_SET_XHL_1 + EE));
	WRMEM(addr, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<unsigned N> II CPUCore<T>::set_N_xhl() {
	SET_X<0>(N, getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<unsigned N, Reg8 REG> II CPUCore<T>::set_N_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(SET_X<T::CC_DD + T::EE_SET_XIX>(N, a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// RL r
template<typename T> inline byte CPUCore<T>::RL(byte reg) {
	byte c = reg >> 7;
	reg = (reg << 1) | ((getF() & C_FLAG) ? 0x01 : 0);
	byte f = c ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[reg];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::RL_X(unsigned x) {
	byte res = RL(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rl_R() {
	set8<REG>(RL(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::rl_xhl() {
	RL_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rl_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(RL_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// RLC r
template<typename T> inline byte CPUCore<T>::RLC(byte reg) {
	byte c = reg >> 7;
	reg = (reg << 1) | c;
	byte f = c ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[reg];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::RLC_X(unsigned x) {
	byte res = RLC(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rlc_R() {
	set8<REG>(RLC(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::rlc_xhl() {
	RLC_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rlc_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(RLC_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// RR r
template<typename T> inline byte CPUCore<T>::RR(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | ((getF() & C_FLAG) << 7);
	byte f = c ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[reg];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::RR_X(unsigned x) {
	byte res = RR(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rr_R() {
	set8<REG>(RR(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::rr_xhl() {
	RR_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rr_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(RR_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// RRC r
template<typename T> inline byte CPUCore<T>::RRC(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | (c << 7);
	byte f = c ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[reg];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::RRC_X(unsigned x) {
	byte res = RRC(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rrc_R() {
	set8<REG>(RRC(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::rrc_xhl() {
	RRC_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::rrc_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(RRC_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// SLA r
template<typename T> inline byte CPUCore<T>::SLA(byte reg) {
	byte c = reg >> 7;
	reg <<= 1;
	byte f = c ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[reg];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::SLA_X(unsigned x) {
	byte res = SLA(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::sla_R() {
	set8<REG>(SLA(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::sla_xhl() {
	SLA_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::sla_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(SLA_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// SLL r
template<typename T> inline byte CPUCore<T>::SLL(byte reg) {
	assert(!T::IS_R800); // this instruction is Z80-only
	byte c = reg >> 7;
	reg = (reg << 1) | 1;
	byte f = c ? C_FLAG : 0;
	f |= table.ZSPXY[reg];
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::SLL_X(unsigned x) {
	byte res = SLL(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::sll_R() {
	set8<REG>(SLL(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::sll_xhl() {
	SLL_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::sll_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(SLL_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}
template<typename T> II CPUCore<T>::sll2() {
	assert(T::IS_R800); // this instruction is R800-only
	byte f = (getF() & (X_FLAG | Y_FLAG)) |
	         (getA() >> 7) | // C_FLAG
	         0; // all other flags zero
	setF(f);
	return {3, T::CC_DD + T::CC_SET_XIX}; // TODO
}

// SRA r
template<typename T> inline byte CPUCore<T>::SRA(byte reg) {
	byte c = reg & 1;
	reg = (reg >> 1) | (reg & 0x80);
	byte f = c ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[reg];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::SRA_X(unsigned x) {
	byte res = SRA(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::sra_R() {
	set8<REG>(SRA(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::sra_xhl() {
	SRA_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::sra_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(SRA_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// SRL R
template<typename T> inline byte CPUCore<T>::SRL(byte reg) {
	byte c = reg & 1;
	reg >>= 1;
	byte f = c ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= table.ZSP[reg];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSPXY[reg];
	}
	setF(f);
	return reg;
}
template<typename T> template<int EE> inline byte CPUCore<T>::SRL_X(unsigned x) {
	byte res = SRL(RDMEM(x, T::CC_SET_XHL_1 + EE));
	WRMEM(x, res, T::CC_SET_XHL_2 + EE);
	return res;
}
template<typename T> template<Reg8 REG> II CPUCore<T>::srl_R() {
	set8<REG>(SRL(get8<REG>())); return {1, T::CC_SET_R};
}
template<typename T> II CPUCore<T>::srl_xhl() {
	SRL_X<0>(getHL()); return {1, T::CC_SET_XHL};
}
template<typename T> template<Reg8 REG> II CPUCore<T>::srl_xix_R(unsigned a) {
	T::setMemPtr(a);
	set8<REG>(SRL_X<T::CC_DD + T::EE_SET_XIX>(a));
	return {3, T::CC_DD + T::CC_SET_XIX};
}

// RLA RLCA RRA RRCA
template<typename T> II CPUCore<T>::rla() {
	byte c = getF() & C_FLAG;
	byte f = (getA() & 0x80) ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG);
	}
	setA((getA() << 1) | (c ? 1 : 0));
	if constexpr (!T::IS_R800) {
		f |= getA() & (X_FLAG | Y_FLAG);
	}
	setF(f);
	return {1, T::CC_RLA};
}
template<typename T> II CPUCore<T>::rlca() {
	setA((getA() << 1) | (getA() >> 7));
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
		f |= getA() & C_FLAG;
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG);
		f |= getA() & (Y_FLAG | X_FLAG | C_FLAG);
	}
	setF(f);
	return {1, T::CC_RLA};
}
template<typename T> II CPUCore<T>::rra() {
	byte c = (getF() & C_FLAG) << 7;
	byte f = (getA() & 0x01) ? C_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG);
	}
	setA((getA() >> 1) | c);
	if constexpr (!T::IS_R800) {
		f |= getA() & (X_FLAG | Y_FLAG);
	}
	setF(f);
	return {1, T::CC_RLA};
}
template<typename T> II CPUCore<T>::rrca() {
	byte f = getA() & C_FLAG;
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG);
	}
	setA((getA() >> 1) | (getA() << 7));
	if constexpr (!T::IS_R800) {
		f |= getA() & (X_FLAG | Y_FLAG);
	}
	setF(f);
	return {1, T::CC_RLA};
}


// RLD
template<typename T> II CPUCore<T>::rld() {
	byte val = RDMEM(getHL(), T::CC_RLD_1);
	T::setMemPtr(getHL() + 1);
	WRMEM(getHL(), (val << 4) | (getA() & 0x0F), T::CC_RLD_2);
	setA((getA() & 0xF0) | (val >> 4));
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= table.ZSP[getA()];
	} else {
		f |= getF() & C_FLAG;
		f |= table.ZSPXY[getA()];
	}
	setF(f);
	return {1, T::CC_RLD};
}

// RRD
template<typename T> II CPUCore<T>::rrd() {
	byte val = RDMEM(getHL(), T::CC_RLD_1);
	T::setMemPtr(getHL() + 1);
	WRMEM(getHL(), (val >> 4) | (getA() << 4), T::CC_RLD_2);
	setA((getA() & 0xF0) | (val & 0x0F));
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= table.ZSP[getA()];
	} else {
		f |= getF() & C_FLAG;
		f |= table.ZSPXY[getA()];
	}
	setF(f);
	return {1, T::CC_RLD};
}


// PUSH ss
template<typename T> template<int EE> inline void CPUCore<T>::PUSH(unsigned reg) {
	setSP(getSP() - 2);
	WR_WORD_rev<true, true>(getSP(), reg, T::CC_PUSH_1 + EE);
}
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::push_SS() {
	PUSH<EE>(get16<REG>()); return {1, T::CC_PUSH + EE};
}

// POP ss
template<typename T> template<int EE> inline unsigned CPUCore<T>::POP() {
	unsigned addr = getSP();
	setSP(addr + 2);
	if constexpr (T::IS_R800) {
		// handles both POP and RET instructions (RET with condition = true)
		if constexpr (EE == 0) { // not reti/retn, not pop ix/iy
			setCurrentPopRet();
			// No need for setSlowInstructions()
			// -> this only matters directly after a CALL
			//    instruction and in that case we're still
			//    executing slow instructions.
		}
	}
	return RD_WORD(addr, T::CC_POP_1 + EE);
}
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::pop_SS() {
	set16<REG>(POP<EE>()); return {1, T::CC_POP + EE};
}


// CALL nn / CALL cc,nn
template<typename T> template<typename COND> II CPUCore<T>::call(COND cond) {
	unsigned addr = RD_WORD_PC<1>(T::CC_CALL_1);
	T::setMemPtr(addr);
	if (cond(getF())) {
		PUSH<T::EE_CALL>(getPC() + 3); /**/
		setPC(addr);
		if constexpr (T::IS_R800) {
			setCurrentCall();
			setSlowInstructions();
		}
		return {0/*3*/, T::CC_CALL_A};
	} else {
		return {3, T::CC_CALL_B};
	}
}


// RST n
template<typename T> template<unsigned ADDR> II CPUCore<T>::rst() {
	PUSH<0>(getPC() + 1); /**/
	T::setMemPtr(ADDR);
	setPC(ADDR);
	if constexpr (T::IS_R800) {
		setCurrentCall();
		setSlowInstructions();
	}
	return {0/*1*/, T::CC_RST};
}


// RET
template<typename T> template<int EE, typename COND> inline II CPUCore<T>::RET(COND cond) {
	if (cond(getF())) {
		unsigned addr = POP<EE>();
		T::setMemPtr(addr);
		setPC(addr);
		return {0/*1*/, T::CC_RET_A + EE};
	} else {
		return {1, T::CC_RET_B + EE};
	}
}
template<typename T> template<typename COND> II CPUCore<T>::ret(COND cond) {
	return RET<T::EE_RET_C>(cond);
}
template<typename T> II CPUCore<T>::ret() {
	return RET<0>(CondTrue());
}
template<typename T> II CPUCore<T>::retn() { // also reti
	setIFF1(getIFF2());
	setSlowInstructions();
	return RET<T::EE_RETN>(CondTrue());
}


// JP ss
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::jp_SS() {
	setPC(get16<REG>()); T::R800ForcePageBreak(); return {0/*1*/, T::CC_JP_HL + EE};
}

// JP nn / JP cc,nn
template<typename T> template<typename COND> II CPUCore<T>::jp(COND cond) {
	unsigned addr = RD_WORD_PC<1>(T::CC_JP_1);
	T::setMemPtr(addr);
	if (cond(getF())) {
		setPC(addr);
		T::R800ForcePageBreak();
		return {0/*3*/, T::CC_JP_A};
	} else {
		return {3, T::CC_JP_B};
	}
}

// JR e
template<typename T> template<typename COND> II CPUCore<T>::jr(COND cond) {
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_JR_1);
	if (cond(getF())) {
		if (((getPC() + 2) & 0xFF) == 0) { /**/
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
			// instructions or events that change PC?)
			//
			// See doc/r800-djnz.txt for more details.
			T::R800ForcePageBreak();
		}
		setPC((getPC() + 2 + ofst) & 0xFFFF); /**/
		T::setMemPtr(getPC());
		return {0/*2*/, T::CC_JR_A};
	} else {
		return {2, T::CC_JR_B};
	}
}

// DJNZ e
template<typename T> II CPUCore<T>::djnz() {
	byte b = getB() - 1;
	setB(b);
	int8_t ofst = RDMEM_OPCODE<1>(T::CC_JR_1 + T::EE_DJNZ);
	if (b) {
		if (((getPC() + 2) & 0xFF) == 0) { /**/
			// See comment in jr()
			T::R800ForcePageBreak();
		}
		setPC((getPC() + 2 + ofst) & 0xFFFF); /**/
		T::setMemPtr(getPC());
		return {0/*2*/, T::CC_JR_A + T::EE_DJNZ};
	} else {
		return {2, T::CC_JR_B + T::EE_DJNZ};
	}
}

// EX (SP),ss
template<typename T> template<Reg16 REG, int EE> II CPUCore<T>::ex_xsp_SS() {
	unsigned res = RD_WORD_impl<true, false>(getSP(), T::CC_EX_SP_HL_1 + EE);
	T::setMemPtr(res);
	WR_WORD_rev<false, true>(getSP(), get16<REG>(), T::CC_EX_SP_HL_2 + EE);
	set16<REG>(res);
	return {1, T::CC_EX_SP_HL + EE};
}

// IN r,(c)
template<typename T> template<Reg8 REG> II CPUCore<T>::in_R_c() {
	if constexpr (T::IS_R800) T::waitForEvenCycle(T::CC_IN_R_C_1);
	T::setMemPtr(getBC() + 1);
	byte res = READ_PORT(getBC(), T::CC_IN_R_C_1);
	byte f = 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= table.ZSP[res];
	} else {
		f |= getF() & C_FLAG;
		f |= table.ZSPXY[res];
	}
	setF(f);
	set8<REG>(res);
	return {1, T::CC_IN_R_C};
}

// IN a,(n)
template<typename T> II CPUCore<T>::in_a_byte() {
	unsigned y = RDMEM_OPCODE<1>(T::CC_IN_A_N_1) + 256 * getA();
	T::setMemPtr(y + 1);
	if constexpr (T::IS_R800) T::waitForEvenCycle(T::CC_IN_A_N_2);
	setA(READ_PORT(y, T::CC_IN_A_N_2));
	return {2, T::CC_IN_A_N};
}

// OUT (c),r
template<typename T> template<Reg8 REG> II CPUCore<T>::out_c_R() {
	if constexpr (T::IS_R800) T::waitForEvenCycle(T::CC_OUT_C_R_1);
	T::setMemPtr(getBC() + 1);
	WRITE_PORT(getBC(), get8<REG>(), T::CC_OUT_C_R_1);
	return {1, T::CC_OUT_C_R};
}
template<typename T> II CPUCore<T>::out_c_0() {
	// TODO not on R800
	if constexpr (T::IS_R800) T::waitForEvenCycle(T::CC_OUT_C_R_1);
	T::setMemPtr(getBC() + 1);
	byte out_c_x = isTurboR ? 255 : 0;
	WRITE_PORT(getBC(), out_c_x, T::CC_OUT_C_R_1);
	return {1, T::CC_OUT_C_R};
}

// OUT (n),a
template<typename T> II CPUCore<T>::out_byte_a() {
	byte port = RDMEM_OPCODE<1>(T::CC_OUT_N_A_1);
	unsigned y = (getA() << 8) |   port;
	T::setMemPtr((getA() << 8) | ((port + 1) & 255));
	if constexpr (T::IS_R800) T::waitForEvenCycle(T::CC_OUT_N_A_2);
	WRITE_PORT(y, getA(), T::CC_OUT_N_A_2);
	return {2, T::CC_OUT_N_A};
}


// block CP
template<typename T> inline II CPUCore<T>::BLOCK_CP(int increase, bool repeat) {
	T::setMemPtr(T::getMemPtr() + increase);
	byte val = RDMEM(getHL(), T::CC_CPI_1);
	byte res = getA() - val;
	setHL(getHL() + increase);
	setBC(getBC() - 1);
	byte f = ((getA() ^ val ^ res) & H_FLAG) |
	         table.ZS[res] |
	         N_FLAG |
	         (getBC() ? V_FLAG : 0);
	if constexpr (T::IS_R800) {
		f |= getF() & (C_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= getF() & C_FLAG;
		unsigned k = res - ((f & H_FLAG) >> 4);
		f |= (k << 4) & Y_FLAG; // bit 1 -> flag 5
		f |= k & X_FLAG;        // bit 3 -> flag 3
	}
	setF(f);
	if (repeat && getBC() && res) {
		//setPC(getPC() - 2);
		T::setMemPtr(getPC() + 1);
		return {-1/*1*/, T::CC_CPIR};
	} else {
		return {1, T::CC_CPI};
	}
}
template<typename T> II CPUCore<T>::cpd()  { return BLOCK_CP(-1, false); }
template<typename T> II CPUCore<T>::cpi()  { return BLOCK_CP( 1, false); }
template<typename T> II CPUCore<T>::cpdr() { return BLOCK_CP(-1, true ); }
template<typename T> II CPUCore<T>::cpir() { return BLOCK_CP( 1, true ); }


// block LD
template<typename T> inline II CPUCore<T>::BLOCK_LD(int increase, bool repeat) {
	byte val = RDMEM(getHL(), T::CC_LDI_1);
	WRMEM(getDE(), val, T::CC_LDI_2);
	setHL(getHL() + increase);
	setDE(getDE() + increase);
	setBC(getBC() - 1);
	byte f = getBC() ? V_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | C_FLAG | X_FLAG | Y_FLAG);
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | C_FLAG);
		f |= ((getA() + val) << 4) & Y_FLAG; // bit 1 -> flag 5
		f |= (getA() + val) & X_FLAG;        // bit 3 -> flag 3
	}
	setF(f);
	if (repeat && getBC()) {
		//setPC(getPC() - 2);
		T::setMemPtr(getPC() + 1);
		return {-1/*1*/, T::CC_LDIR};
	} else {
		return {1, T::CC_LDI};
	}
}
template<typename T> II CPUCore<T>::ldd()  { return BLOCK_LD(-1, false); }
template<typename T> II CPUCore<T>::ldi()  { return BLOCK_LD( 1, false); }
template<typename T> II CPUCore<T>::lddr() { return BLOCK_LD(-1, true ); }
template<typename T> II CPUCore<T>::ldir() { return BLOCK_LD( 1, true ); }


// block IN
template<typename T> inline II CPUCore<T>::BLOCK_IN(int increase, bool repeat) {
	// TODO R800 flags
	if constexpr (T::IS_R800) T::waitForEvenCycle(T::CC_INI_1);
	T::setMemPtr(getBC() + increase);
	setBC(getBC() - 0x100); // decr before use
	byte val = READ_PORT(getBC(), T::CC_INI_1);
	WRMEM(getHL(), val, T::CC_INI_2);
	setHL(getHL() + increase);
	unsigned k = val + ((getC() + increase) & 0xFF);
	byte b = getB();
	setF(((val & S_FLAG) >> 6) | // N_FLAG
	       ((k & 0x100) ? (H_FLAG | C_FLAG) : 0) |
	       table.ZSXY[b] |
	       (table.ZSPXY[(k & 0x07) ^ b] & P_FLAG));
	if (repeat && b) {
		//setPC(getPC() - 2);
		return {-1/*1*/, T::CC_INIR};
	} else {
		return {1, T::CC_INI};
	}
}
template<typename T> II CPUCore<T>::ind()  { return BLOCK_IN(-1, false); }
template<typename T> II CPUCore<T>::ini()  { return BLOCK_IN( 1, false); }
template<typename T> II CPUCore<T>::indr() { return BLOCK_IN(-1, true ); }
template<typename T> II CPUCore<T>::inir() { return BLOCK_IN( 1, true ); }


// block OUT
template<typename T> inline II CPUCore<T>::BLOCK_OUT(int increase, bool repeat) {
	// TODO R800 flags
	byte val = RDMEM(getHL(), T::CC_OUTI_1);
	setHL(getHL() + increase);
	if constexpr (T::IS_R800) T::waitForEvenCycle(T::CC_OUTI_2);
	WRITE_PORT(getBC(), val, T::CC_OUTI_2);
	setBC(getBC() - 0x100); // decr after use
	T::setMemPtr(getBC() + increase);
	unsigned k = val + getL();
	byte b = getB();
	setF(((val & S_FLAG) >> 6) | // N_FLAG
	       ((k & 0x100) ? (H_FLAG | C_FLAG) : 0) |
	       table.ZSXY[b] |
	       (table.ZSPXY[(k & 0x07) ^ b] & P_FLAG));
	if (repeat && b) {
		//setPC(getPC() - 2);
		return {-1/*1*/, T::CC_OTIR};
	} else {
		return {1, T::CC_OUTI};
	}
}
template<typename T> II CPUCore<T>::outd() { return BLOCK_OUT(-1, false); }
template<typename T> II CPUCore<T>::outi() { return BLOCK_OUT( 1, false); }
template<typename T> II CPUCore<T>::otdr() { return BLOCK_OUT(-1, true ); }
template<typename T> II CPUCore<T>::otir() { return BLOCK_OUT( 1, true ); }


// various
template<typename T> II CPUCore<T>::nop() { return {1, T::CC_NOP}; }
template<typename T> II CPUCore<T>::ccf() {
	byte f = 0;
	if constexpr (T::IS_R800) {
		// H flag is different from Z80 (and as always XY flags as well)
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG | X_FLAG | Y_FLAG | H_FLAG);
	} else {
		f |= (getF() & C_FLAG) << 4; // H_FLAG
		// only set X(Y) flag (don't reset if already set)
		if (isTurboR) {
			// Y flag is not changed on a turboR-Z80
			f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG | Y_FLAG);
			f |= (getF() | getA()) & X_FLAG;
		} else {
			f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG);
			f |= (getF() | getA()) & (X_FLAG | Y_FLAG);
		}
	}
	f ^= C_FLAG;
	setF(f);
	return {1, T::CC_CCF};
}
template<typename T> II CPUCore<T>::cpl() {
	setA(getA() ^ 0xFF);
	byte f = H_FLAG | N_FLAG;
	if constexpr (T::IS_R800) {
		f |= getF();
	} else {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | C_FLAG);
		f |= getA() & (X_FLAG | Y_FLAG);
	}
	setF(f);
	return {1, T::CC_CPL};
}
template<typename T> II CPUCore<T>::daa() {
	byte a = getA();
	byte f = getF();
	byte adjust = 0;
	if ((f & H_FLAG) || ((getA() & 0xf) > 9)) adjust += 6;
	if ((f & C_FLAG) || (getA() > 0x99)) adjust += 0x60;
	if (f & N_FLAG) a -= adjust; else a += adjust;
	if constexpr (T::IS_R800) {
		f &= C_FLAG | N_FLAG | X_FLAG | Y_FLAG;
		f |= table.ZSP[a];
	} else {
		f &= C_FLAG | N_FLAG;
		f |= table.ZSPXY[a];
	}
	f |= (getA() > 0x99) | ((getA() ^ a) & H_FLAG);
	setA(a);
	setF(f);
	return {1, T::CC_DAA};
}
template<typename T> II CPUCore<T>::neg() {
	// alternative: LUT   word negTable[256]
	unsigned a = getA();
	unsigned res = -signed(a);
	byte f = ((res & 0x100) ? C_FLAG : 0) |
	         N_FLAG |
	         ((res ^ a) & H_FLAG) |
	         ((a & res & 0x80) >> 5); // V_FLAG
	if constexpr (T::IS_R800) {
		f |= table.ZS[res & 0xFF];
		f |= getF() & (X_FLAG | Y_FLAG);
	} else {
		f |= table.ZSXY[res & 0xFF];
	}
	setF(f);
	setA(res);
	return {1, T::CC_NEG};
}
template<typename T> II CPUCore<T>::scf() {
	byte f = C_FLAG;
	if constexpr (T::IS_R800) {
		f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | X_FLAG | Y_FLAG);
	} else {
		// only set X(Y) flag (don't reset if already set)
		if (isTurboR) {
			// Y flag is not changed on a turboR-Z80
			f |= getF() & (S_FLAG | Z_FLAG | P_FLAG | Y_FLAG);
			f |= (getF() | getA()) & X_FLAG;
		} else {
			f |= getF() & (S_FLAG | Z_FLAG | P_FLAG);
			f |= (getF() | getA()) & (X_FLAG | Y_FLAG);
		}
	}
	setF(f);
	return {1, T::CC_SCF};
}

template<typename T> II CPUCore<T>::ex_af_af() {
	unsigned t = getAF2(); setAF2(getAF()); setAF(t);
	return {1, T::CC_EX};
}
template<typename T> II CPUCore<T>::ex_de_hl() {
	unsigned t = getDE(); setDE(getHL()); setHL(t);
	return {1, T::CC_EX};
}
template<typename T> II CPUCore<T>::exx() {
	unsigned t1 = getBC2(); setBC2(getBC()); setBC(t1);
	unsigned t2 = getDE2(); setDE2(getDE()); setDE(t2);
	unsigned t3 = getHL2(); setHL2(getHL()); setHL(t3);
	return {1, T::CC_EX};
}

template<typename T> II CPUCore<T>::di() {
	setIFF1(false);
	setIFF2(false);
	return {1, T::CC_DI};
}
template<typename T> II CPUCore<T>::ei() {
	setIFF1(true);
	setIFF2(true);
	setCurrentEI(); // no ints directly after this instr
	setSlowInstructions();
	return {1, T::CC_EI};
}
template<typename T> II CPUCore<T>::halt() {
	setHALT(true);
	setSlowInstructions();

	if (!(getIFF1() || getIFF2())) {
		diHaltCallback.execute();
	}
	return {1, T::CC_HALT};
}
template<typename T> template<unsigned N> II CPUCore<T>::im_N() {
	setIM(N); return {1, T::CC_IM};
}

// LD A,I/R
template<typename T> template<Reg8 REG> II CPUCore<T>::ld_a_IR() {
	setA(get8<REG>());
	byte f = getIFF2() ? V_FLAG : 0;
	if constexpr (T::IS_R800) {
		f |= getF() & (C_FLAG | X_FLAG | Y_FLAG);
		f |= table.ZS[getA()];
	} else {
		f |= getF() & C_FLAG;
		f |= table.ZSXY[getA()];
		// see comment in the IRQ acceptance part of executeSlow().
		setCurrentLDAI(); // only Z80 (not R800) has this quirk
		setSlowInstructions();
	}
	setF(f);
	return {1, T::CC_LD_A_I};
}

// LD I/R,A
template<typename T> II CPUCore<T>::ld_r_a() {
	// This code sequence:
	//   XOR A  /  LD R,A   / LD A,R
	// gives A=2 for Z80, but A=1 for R800. The difference can possibly be
	// explained by a difference in the relative time between writing the
	// new value to the R register and increasing the R register per M1
	// cycle. Here we implemented the R800 behaviour by storing 'A-1' into
	// R, that's good enough for now.
	byte val = getA();
	if constexpr (T::IS_R800) val -= 1;
	setR(val);
	return {1, T::CC_LD_A_I};
}
template<typename T> II CPUCore<T>::ld_i_a() {
	setI(getA());
	return {1, T::CC_LD_A_I};
}

// MULUB A,r
template<typename T> template<Reg8 REG> II CPUCore<T>::mulub_a_R() {
	assert(T::IS_R800); // this instruction is R800-only
	// Verified on real R800:
	//   YHXN flags are unchanged
	//   SV   flags are reset
	//   Z    flag is set when result is zero
	//   C    flag is set when result doesn't fit in 8-bit
	setHL(unsigned(getA()) * get8<REG>());
	setF((getF() & (N_FLAG | H_FLAG | X_FLAG | Y_FLAG)) |
	       0 | // S_FLAG V_FLAG
	       (getHL() ? 0 : Z_FLAG) |
	       ((getHL() & 0xFF00) ? C_FLAG : 0));
	return {1, T::CC_MULUB};
}

// MULUW HL,ss
template<typename T> template<Reg16 REG> II CPUCore<T>::muluw_hl_SS() {
	assert(T::IS_R800); // this instruction is R800-only
	// Verified on real R800:
	//   YHXN flags are unchanged
	//   SV   flags are reset
	//   Z    flag is set when result is zero
	//   C    flag is set when result doesn't fit in 16-bit
	unsigned res = unsigned(getHL()) * get16<REG>();
	setDE(res >> 16);
	setHL(res & 0xffff);
	setF((getF() & (N_FLAG | H_FLAG | X_FLAG | Y_FLAG)) |
	       0 | // S_FLAG V_FLAG
	       (res ? 0 : Z_FLAG) |
	       ((res & 0xFFFF0000) ? C_FLAG : 0));
	return {1, T::CC_MULUW};
}


// versions:
//  1 -> initial version
//  2 -> moved memptr from here to Z80TYPE (and not to R800TYPE)
//  3 -> timing of the emulation changed (no changes in serialization)
//  4 -> timing of the emulation changed again (see doc/internal/r800-call.txt)
//  5 -> added serialization of nmiEdge
template<typename T> template<typename Archive>
void CPUCore<T>::serialize(Archive& ar, unsigned version)
{
	T::serialize(ar, version);
	ar.serialize("regs", static_cast<CPURegs&>(*this));
	if (ar.versionBelow(version, 2)) {
		unsigned mPtr = 0; // dummy value (avoid warning)
		ar.serialize("memptr", mPtr);
		T::setMemPtr(mPtr);
	}

	if (ar.versionBelow(version, 5)) {
		// NMI is unused on MSX and even on systems where it is used nmiEdge
		// is true only between the moment the NMI request comes in and the
		// moment the Z80 jumps to the NMI handler, so defaulting to false
		// is pretty safe.
		nmiEdge = false;
	} else {
		// CPU is deserialized after devices, so nmiEdge is restored to the
		// saved version even if IRQHelpers set it on deserialization.
		ar.serialize("nmiEdge", nmiEdge);
	}

	// Don't serialize:
	// - IRQStatus, NMIStatus:
	//     the IRQHelper deserialization makes sure these get the right value
	// - slowInstructions, exitLoop:
	//     serialization happens outside the CPU emulation loop

	if constexpr (T::IS_R800) {
		if (ar.versionBelow(version, 4)) {
			motherboard.getMSXCliComm().printWarning(
				"Loading an old savestate: the timing of the R800 "
				"emulation has changed. This may cause synchronization "
				"problems in replay.");
		}
	}
}

// Force template instantiation
template class CPUCore<Z80TYPE>;
template class CPUCore<R800TYPE>;

INSTANTIATE_SERIALIZE_METHODS(CPUCore<Z80TYPE>);
INSTANTIATE_SERIALIZE_METHODS(CPUCore<R800TYPE>);

} // namespace openmsx
