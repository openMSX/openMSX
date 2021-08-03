#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"
#include "Scheduler.hh"
#include "IntegerSetting.hh"
#include "CPUCore.hh"
#include "Z80.hh"
#include "R800.hh"
#include "TclObject.hh"
#include "outer.hh"
#include "ranges.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "xrange.hh"
#include <cassert>
#include <memory>

using std::string;
using std::vector;

namespace openmsx {

MSXCPU::MSXCPU(MSXMotherBoard& motherboard_)
	: motherboard(motherboard_)
	, traceSetting(
		motherboard.getCommandController(), "cputrace",
		"CPU tracing on/off", false, Setting::DONT_SAVE)
	, diHaltCallback(
		motherboard.getCommandController(), "di_halt_callback",
		"Tcl proc called when the CPU executed a DI/HALT sequence")
	, z80(std::make_unique<CPUCore<Z80TYPE>>(
		motherboard, "z80", traceSetting,
		diHaltCallback, EmuTime::zero()))
	, r800(motherboard.isTurboR()
		? std::make_unique<CPUCore<R800TYPE>>(
			motherboard, "r800", traceSetting,
			diHaltCallback, EmuTime::zero())
		: nullptr)
	, timeInfo(motherboard.getMachineInfoCommand())
	, z80FreqInfo(motherboard.getMachineInfoCommand(), "z80_freq", *z80)
	, r800FreqInfo(r800
		? std::optional<CPUFreqInfoTopic>(std::in_place,
			motherboard.getMachineInfoCommand(), "r800_freq", *r800)
		: std::nullopt)
	, debuggable(motherboard_)
	, reference(EmuTime::zero())
{
	z80Active = true; // setActiveCPU(CPU_Z80);
	newZ80Active = z80Active;

	motherboard.getDebugger().setCPU(this);
	motherboard.getScheduler().setCPU(this);
	traceSetting.attach(*this);

	z80->freqLocked.attach(*this);
	z80->freqValue.attach(*this);
	if (r800) {
		r800->freqLocked.attach(*this);
		r800->freqValue.attach(*this);
	}
	invalidateMemCacheSlot();
}

MSXCPU::~MSXCPU()
{
	traceSetting.detach(*this);
	z80->freqLocked.detach(*this);
	z80->freqValue.detach(*this);
	if (r800) {
		r800->freqLocked.detach(*this);
		r800->freqValue.detach(*this);
	}
	motherboard.getScheduler().setCPU(nullptr);
	motherboard.getDebugger() .setCPU(nullptr);
}

void MSXCPU::setInterface(MSXCPUInterface* interface_)
{
	interface = interface_;
	          z80 ->setInterface(interface);
	if (r800) r800->setInterface(interface);
}

void MSXCPU::doReset(EmuTime::param time)
{
	          z80 ->doReset(time);
	if (r800) r800->doReset(time);

	invalidateAllSlotsRWCache(0x0000, 0x10000);

	reference = time;
}

void MSXCPU::setActiveCPU(CPUType cpu)
{
	if (cpu == CPU_R800) assert(r800);

	bool tmp = cpu == CPU_Z80;
	if (tmp != z80Active) {
		exitCPULoopSync();
		newZ80Active = tmp;
	}
}

void MSXCPU::setDRAMmode(bool dram)
{
	assert(r800);
	r800->setDRAMmode(dram);
}

void MSXCPU::execute(bool fastForward)
{
	if (z80Active != newZ80Active) {
		EmuTime time = getCurrentTime();
		z80Active = newZ80Active;
		z80Active ? z80 ->warp(time)
		          : r800->warp(time);
		// copy Z80/R800 cache lines
		auto zCache = z80 ->getCacheLines();
		auto rCache = r800->getCacheLines();
		auto from = z80Active ? rCache : zCache;
		auto to   = z80Active ? zCache : rCache;
		std::copy_n(from.read,  CacheLine::NUM, to.read );
		std::copy_n(from.write, CacheLine::NUM, to.write);
	}
	z80Active ? z80 ->execute(fastForward)
	          : r800->execute(fastForward);
}

void MSXCPU::exitCPULoopSync()
{
	z80Active ? z80 ->exitCPULoopSync()
	          : r800->exitCPULoopSync();
}
void MSXCPU::exitCPULoopAsync()
{
	z80Active ? z80 ->exitCPULoopAsync()
	          : r800->exitCPULoopAsync();
}

EmuTime::param MSXCPU::getCurrentTime() const
{
	return z80Active ? z80 ->getCurrentTime()
	                 : r800->getCurrentTime();
}

void MSXCPU::setNextSyncPoint(EmuTime::param time)
{
	z80Active ? z80 ->setNextSyncPoint(time)
	          : r800->setNextSyncPoint(time);
}

void MSXCPU::invalidateMemCacheSlot()
{
	ranges::fill(slots, 0);

	// nullptr: means not a valid entry and not yet attempted to fill this entry
	for (auto i : xrange(16)) {
		ranges::fill(slotReadLines[i], nullptr);
		ranges::fill(slotWriteLines[i], nullptr);
	}
}

void MSXCPU::updateVisiblePage(byte page, byte primarySlot, byte secondarySlot)
{
	byte from = slots[page];
	byte to = 4 * primarySlot + secondarySlot;
	slots[page] = to;

	auto [cpuReadLines, cpuWriteLines] = z80Active ? z80->getCacheLines() : r800->getCacheLines();

	unsigned first = page * (0x4000 / CacheLine::SIZE);
	unsigned num = 0x4000 / CacheLine::SIZE;
	std::copy_n(&cpuReadLines      [first], num, &slotReadLines [from][first]);
	std::copy_n(&slotReadLines [to][first], num, &cpuReadLines        [first]);
	std::copy_n(&cpuWriteLines     [first], num, &slotWriteLines[from][first]);
	std::copy_n(&slotWriteLines[to][first], num, &cpuWriteLines       [first]);

	if (r800) r800->updateVisiblePage(page, primarySlot, secondarySlot);
}

void MSXCPU::invalidateAllSlotsRWCache(word start, unsigned size)
{
	if (interface) interface->tick(CacheLineCounters::InvalidateAllSlots);
	auto [cpuReadLines, cpuWriteLines] = z80Active ? z80->getCacheLines() : r800->getCacheLines();

	unsigned first = start / CacheLine::SIZE;
	unsigned num = (size + CacheLine::SIZE - 1) / CacheLine::SIZE;
	std::fill_n(cpuReadLines  + first, num, nullptr); // nullptr: means not a valid entry and not
	std::fill_n(cpuWriteLines + first, num, nullptr); //   yet attempted to fill this entry

	for (auto i : xrange(16)) {
		std::fill_n(slotReadLines [i] + first, num, nullptr);
		std::fill_n(slotWriteLines[i] + first, num, nullptr);
	}
}

template<bool READ, bool WRITE, bool SUB_START>
void MSXCPU::setRWCache(unsigned start, unsigned size, const byte* rData, byte* wData, int ps, int ss,
                             const byte* disallowRead, const byte* disallowWrite)
{
	if constexpr (!SUB_START) {
		assert(rData == nullptr);
		assert(wData == nullptr);
	}

	// aligned on cache lines
	assert((start & CacheLine::LOW) == 0);
	assert((size  & CacheLine::LOW) == 0);

	int slot = 4 * ps + ss;
	unsigned page = start >> 14;
	assert(((start + size - 1) >> 14) == page); // all in same page
	if constexpr (SUB_START && READ)  rData -= start;
	if constexpr (SUB_START && WRITE) wData -= start;

	// select between 'active' or 'shadow' cache lines
	auto [readLines, writeLines] = [&] {
		if (slot == slots[page]) {
			return z80Active ? z80->getCacheLines() : r800->getCacheLines();
		} else {
			return CacheLines{slotReadLines [slot],
			                  slotWriteLines[slot]};
		}
	}();

	unsigned first = start / CacheLine::SIZE;
	readLines     += first;
	writeLines    += first;
	disallowRead  += first;
	disallowWrite += first;
	unsigned num = size / CacheLine::SIZE;

	static auto* const NON_CACHEABLE = reinterpret_cast<byte*>(1);
	for (auto i : xrange(num)) {
		if constexpr (READ)  readLines [i] = disallowRead [i] ? NON_CACHEABLE : rData;
		if constexpr (WRITE) writeLines[i] = disallowWrite[i] ? NON_CACHEABLE : wData;
	}
}

static constexpr void extendForAlignment(unsigned& start, unsigned& size)
{
	constexpr unsigned MASK = ~(CacheLine::LOW); // not CacheLine::HIGH because 0x0000ff00 != 0xffffff00

	auto end = start + size;
	start &= MASK;                            // round down to cacheline
	end = (end + CacheLine::SIZE - 1) & MASK; // round up to cacheline
	size = end - start;
}

void MSXCPU::invalidateRWCache(unsigned start, unsigned size, int ps, int ss,
                               const byte* disallowRead, const byte* disallowWrite)
{
	// unaligned [start, start+size) is OK for invalidate, then simply invalidate a little more.
	extendForAlignment(start, size);
	setRWCache<true, true, false>(start, size, nullptr, nullptr, ps, ss, disallowRead, disallowWrite);
}
void MSXCPU::invalidateRCache(unsigned start, unsigned size, int ps, int ss,
                              const byte* disallowRead)
{
	extendForAlignment(start, size);
	setRWCache<true, false, false>(start, size, nullptr, nullptr, ps, ss, disallowRead, nullptr);
}
void MSXCPU::invalidateWCache(unsigned start, unsigned size, int ps, int ss,
                              const byte* disallowWrite)
{
	extendForAlignment(start, size);
	setRWCache<false, true, false>(start, size, nullptr, nullptr, ps, ss, nullptr, disallowWrite);
}

void MSXCPU::fillRWCache(unsigned start, unsigned size, const byte* rData, byte* wData, int ps, int ss,
                         const byte* disallowRead, const byte* disallowWrite)
{
	setRWCache<true, true, true>(start, size, rData, wData, ps, ss, disallowRead, disallowWrite);
}
void MSXCPU::fillRCache(unsigned start, unsigned size, const byte* rData, int ps, int ss,
                        const byte* disallowRead)
{
	setRWCache<true, false, true>(start, size, rData, nullptr, ps, ss, disallowRead, nullptr);
}
void MSXCPU::fillWCache(unsigned start, unsigned size, byte* wData, int ps, int ss,
                        const byte* disallowWrite)
{
	setRWCache<false, true, true>(start, size, nullptr, wData, ps, ss, nullptr, disallowWrite);
}

void MSXCPU::raiseIRQ()
{
	          z80 ->raiseIRQ();
	if (r800) r800->raiseIRQ();
}
void MSXCPU::lowerIRQ()
{
	          z80 ->lowerIRQ();
	if (r800) r800->lowerIRQ();
}
void MSXCPU::raiseNMI()
{
	          z80 ->raiseNMI();
	if (r800) r800->raiseNMI();
}
void MSXCPU::lowerNMI()
{
	          z80 ->lowerNMI();
	if (r800) r800->lowerNMI();
}

bool MSXCPU::isM1Cycle(unsigned address) const
{
	return z80Active ? z80 ->isM1Cycle(address)
	                 : r800->isM1Cycle(address);
}

void MSXCPU::setZ80Freq(unsigned freq)
{
	z80->setFreq(freq);
}

void MSXCPU::wait(EmuTime::param time)
{
	z80Active ? z80 ->wait(time)
	          : r800->wait(time);
}

EmuTime MSXCPU::waitCyclesZ80(EmuTime::param time, unsigned cycles)
{
	return z80Active ? z80 ->waitCycles(time, cycles)
	                 : time;
}

EmuTime MSXCPU::waitCyclesR800(EmuTime::param time, unsigned cycles)
{
	return z80Active ? time
	                 : r800->waitCycles(time, cycles);
}

CPURegs& MSXCPU::getRegisters()
{
	if (z80Active) {
		return *z80;
	} else {
		return *r800;
	}
}

void MSXCPU::update(const Setting& setting) noexcept
{
	          z80 ->update(setting);
	if (r800) r800->update(setting);
	exitCPULoopSync();
}

// Command

void MSXCPU::disasmCommand(
	Interpreter& interp, span<const TclObject> tokens,
	TclObject& result) const
{
	z80Active ? z80 ->disasmCommand(interp, tokens, result)
	          : r800->disasmCommand(interp, tokens, result);
}

void MSXCPU::setPaused(bool paused)
{
	if (z80Active) {
		z80 ->setExtHALT(paused);
		z80 ->exitCPULoopSync();
	} else {
		r800->setExtHALT(paused);
		r800->exitCPULoopSync();
	}
}


// class TimeInfoTopic

MSXCPU::TimeInfoTopic::TimeInfoTopic(InfoCommand& machineInfoCommand)
	: InfoTopic(machineInfoCommand, "time")
{
}

void MSXCPU::TimeInfoTopic::execute(
	span<const TclObject> /*tokens*/, TclObject& result) const
{
	auto& cpu = OUTER(MSXCPU, timeInfo);
	EmuDuration dur = cpu.getCurrentTime() - cpu.reference;
	result = dur.toDouble();
}

string MSXCPU::TimeInfoTopic::help(span<const TclObject> /*tokens*/) const
{
	return "Prints the time in seconds that the MSX is powered on\n";
}


// class CPUFreqInfoTopic

MSXCPU::CPUFreqInfoTopic::CPUFreqInfoTopic(
		InfoCommand& machineInfoCommand,
		const string& name_, CPUClock& clock_)
	: InfoTopic(machineInfoCommand, name_)
	, clock(clock_)
{
}

void MSXCPU::CPUFreqInfoTopic::execute(
	span<const TclObject> /*tokens*/, TclObject& result) const
{
	result = clock.getFreq();
}

string MSXCPU::CPUFreqInfoTopic::help(span<const TclObject> /*tokens*/) const
{
	return "Returns the actual frequency of this CPU.\n"
	       "This frequency can vary because:\n"
	       " - the user has overridden the freq via the '{z80,r800}_freq' setting\n"
	       " - (only on some MSX machines) the MSX software can switch the Z80 between 2 frequencies\n"
	       "See also the '{z80,r800}_freq_locked' setting.\n";
}


// class Debuggable

constexpr static_string_view CPU_REGS_DESC =
	"Registers of the active CPU (Z80 or R800).\n"
	"Each byte in this debuggable represents one 8 bit register:\n"
	"  0 ->  A      1 ->  F      2 -> B       3 -> C\n"
	"  4 ->  D      5 ->  E      6 -> H       7 -> L\n"
	"  8 ->  A'     9 ->  F'    10 -> B'     11 -> C'\n"
	" 12 ->  D'    13 ->  E'    14 -> H'     15 -> L'\n"
	" 16 -> IXH    17 -> IXL    18 -> IYH    19 -> IYL\n"
	" 20 -> PCH    21 -> PCL    22 -> SPH    23 -> SPL\n"
	" 24 ->  I     25 ->  R     26 -> IM     27 -> IFF1/2\n"
	"The last position (27) contains the IFF1 and IFF2 flags in respectively\n"
	"bit 0 and 1. Bit 2 contains 'IFF1 AND last-instruction-was-not-EI', so\n"
	"this effectively indicates that the CPU could accept an interrupt at\n"
	"the start of the current instruction.\n";

MSXCPU::Debuggable::Debuggable(MSXMotherBoard& motherboard_)
	: SimpleDebuggable(motherboard_, "CPU regs", CPU_REGS_DESC, 28)
{
}

byte MSXCPU::Debuggable::read(unsigned address)
{
	auto& cpu = OUTER(MSXCPU, debuggable);
	const CPURegs& regs = cpu.getRegisters();
	switch (address) {
	case  0: return regs.getA();
	case  1: return regs.getF();
	case  2: return regs.getB();
	case  3: return regs.getC();
	case  4: return regs.getD();
	case  5: return regs.getE();
	case  6: return regs.getH();
	case  7: return regs.getL();
	case  8: return regs.getA2();
	case  9: return regs.getF2();
	case 10: return regs.getB2();
	case 11: return regs.getC2();
	case 12: return regs.getD2();
	case 13: return regs.getE2();
	case 14: return regs.getH2();
	case 15: return regs.getL2();
	case 16: return regs.getIXh();
	case 17: return regs.getIXl();
	case 18: return regs.getIYh();
	case 19: return regs.getIYl();
	case 20: return regs.getPCh();
	case 21: return regs.getPCl();
	case 22: return regs.getSPh();
	case 23: return regs.getSPl();
	case 24: return regs.getI();
	case 25: return regs.getR();
	case 26: return regs.getIM();
	case 27: return 1 *  regs.getIFF1() +
	                2 *  regs.getIFF2() +
	                4 * (regs.getIFF1() && !regs.prevWasEI());
	default: UNREACHABLE; return 0;
	}
}

void MSXCPU::Debuggable::write(unsigned address, byte value)
{
	auto& cpu = OUTER(MSXCPU, debuggable);
	CPURegs& regs = cpu.getRegisters();
	switch (address) {
	case  0: regs.setA(value); break;
	case  1: regs.setF(value); break;
	case  2: regs.setB(value); break;
	case  3: regs.setC(value); break;
	case  4: regs.setD(value); break;
	case  5: regs.setE(value); break;
	case  6: regs.setH(value); break;
	case  7: regs.setL(value); break;
	case  8: regs.setA2(value); break;
	case  9: regs.setF2(value); break;
	case 10: regs.setB2(value); break;
	case 11: regs.setC2(value); break;
	case 12: regs.setD2(value); break;
	case 13: regs.setE2(value); break;
	case 14: regs.setH2(value); break;
	case 15: regs.setL2(value); break;
	case 16: regs.setIXh(value); break;
	case 17: regs.setIXl(value); break;
	case 18: regs.setIYh(value); break;
	case 19: regs.setIYl(value); break;
	case 20: regs.setPCh(value); break;
	case 21: regs.setPCl(value); break;
	case 22: regs.setSPh(value); break;
	case 23: regs.setSPl(value); break;
	case 24: regs.setI(value); break;
	case 25: regs.setR(value); break;
	case 26:
		if (value < 3) regs.setIM(value);
		break;
	case 27:
		regs.setIFF1((value & 0x01) != 0);
		regs.setIFF2((value & 0x02) != 0);
		// can't change afterEI
		break;
	default:
		UNREACHABLE;
	}
}

// version 1: initial version
// version 2: activeCPU,newCPU -> z80Active,newZ80Active
template<typename Archive>
void MSXCPU::serialize(Archive& ar, unsigned version)
{
	if (ar.versionAtLeast(version, 2)) {
		          ar.serialize("z80",  *z80);
		if (r800) ar.serialize("r800", *r800);
		ar.serialize("z80Active",    z80Active,
		             "newZ80Active", newZ80Active);
	} else {
		// backwards-compatibility
		assert(Archive::IS_LOADER);

		          ar.serializeWithID("z80",  *z80);
		if (r800) ar.serializeWithID("r800", *r800);
		CPUBase* activeCPU = nullptr;
		CPUBase* newCPU = nullptr;
		ar.serializePointerID("activeCPU", activeCPU);
		ar.serializePointerID("newCPU",    newCPU);
		z80Active = activeCPU == z80.get();
		if (newCPU) {
			newZ80Active = newCPU == z80.get();
		} else {
			newZ80Active = z80Active;
		}
	}
	ar.serialize("resetTime", reference);

	if constexpr (Archive::IS_LOADER) {
		invalidateMemCacheSlot();
		invalidateAllSlotsRWCache(0x0000, 0x10000);
	}
}
INSTANTIATE_SERIALIZE_METHODS(MSXCPU);

} // namespace openmsx
