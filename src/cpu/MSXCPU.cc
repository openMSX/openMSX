#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"
#include "Scheduler.hh"
#include "SimpleDebuggable.hh"
#include "BooleanSetting.hh"
#include "IntegerSetting.hh"
#include "TclCallback.hh"
#include "CPUCore.hh"
#include "Z80.hh"
#include "R800.hh"
#include "InfoTopic.hh"
#include "TclObject.hh"
#include "serialize.hh"
#include "unreachable.hh"
#include "memory.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class TimeInfoTopic : public InfoTopic
{
public:
	TimeInfoTopic(InfoCommand& machineInfoCommand,
	              MSXCPU& msxcpu);
	virtual void execute(const vector<TclObject>& tokens,
	                     TclObject& result) const;
	virtual string help (const vector<string>& tokens) const;
private:
	MSXCPU& msxcpu;
};

class CPUFreqInfoTopic : public InfoTopic
{
public:
	CPUFreqInfoTopic(InfoCommand& machineInfoCommand,
	                 const string& name, CPUClock& clock);
	virtual void execute(const vector<TclObject>& tokens,
	                     TclObject& result) const;
	virtual string help (const vector<string>& tokens) const;
private:
	CPUClock& clock;
};

class MSXCPUDebuggable : public SimpleDebuggable
{
public:
	MSXCPUDebuggable(MSXMotherBoard& motherboard, MSXCPU& cpu);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	MSXCPU& cpu;
};


MSXCPU::MSXCPU(MSXMotherBoard& motherboard_)
	: motherboard(motherboard_)
	, traceSetting(make_unique<BooleanSetting>(
		motherboard.getCommandController(), "cputrace",
		"CPU tracing on/off", false, Setting::DONT_SAVE))
	, diHaltCallback(make_unique<TclCallback>(
		motherboard.getCommandController(), "di_halt_callback",
		"Tcl proc called when the CPU executed a DI/HALT sequence"))
	, z80(make_unique<CPUCore<Z80TYPE>>(
		motherboard, "z80", *traceSetting,
		*diHaltCallback, EmuTime::zero))
	, r800(motherboard.isTurboR()
		? make_unique<CPUCore<R800TYPE>>(
			motherboard, "r800", *traceSetting,
			*diHaltCallback, EmuTime::zero)
		: nullptr)
	, timeInfo(make_unique<TimeInfoTopic>(
		motherboard.getMachineInfoCommand(), *this))
	, z80FreqInfo(make_unique<CPUFreqInfoTopic>(
		motherboard.getMachineInfoCommand(), "z80_freq", *z80))
	, r800FreqInfo(r800
		? make_unique<CPUFreqInfoTopic>(
			motherboard.getMachineInfoCommand(), "r800_freq", *r800)
		: nullptr)
	, debuggable(make_unique<MSXCPUDebuggable>(motherboard_, *this))
	, reference(EmuTime::zero)
{
	z80Active = true; // setActiveCPU(CPU_Z80);
	newZ80Active = z80Active;

	motherboard.getDebugger().setCPU(this);
	motherboard.getScheduler().setCPU(this);
	traceSetting->attach(*this);

	z80->freqLocked->attach(*this);
	z80->freqValue->attach(*this);
	if (r800) {
		r800->freqLocked->attach(*this);
		r800->freqValue->attach(*this);
	}
}

MSXCPU::~MSXCPU()
{
	traceSetting->detach(*this);
	z80->freqLocked->detach(*this);
	z80->freqValue->detach(*this);
	if (r800) {
		r800->freqLocked->detach(*this);
		r800->freqValue->detach(*this);
	}
	motherboard.getScheduler().setCPU(nullptr);
	motherboard.getDebugger() .setCPU(nullptr);
}

void MSXCPU::setInterface(MSXCPUInterface* interface)
{
	          z80 ->setInterface(interface);
	if (r800) r800->setInterface(interface);
}

void MSXCPU::doReset(EmuTime::param time)
{
	          z80 ->doReset(time);
	if (r800) r800->doReset(time);

	reference = time;
}

void MSXCPU::setActiveCPU(CPUType cpu)
{
	bool tmp = cpu == CPU_Z80;
	switch (cpu) {
		case CPU_Z80:
			PRT_DEBUG("Active CPU: Z80");
			break;
		case CPU_R800:
			PRT_DEBUG("Active CPU: R800");
			assert(r800);
			break;
		default:
			UNREACHABLE;
	}
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
		invalidateMemCache(0x0000, 0x10000);
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

void MSXCPU::updateVisiblePage(byte page, byte primarySlot, byte secondarySlot)
{
	invalidateMemCache(page * 0x4000, 0x4000);
	if (r800) r800->updateVisiblePage(page, primarySlot, secondarySlot);
}

void MSXCPU::invalidateMemCache(word start, unsigned size)
{
	z80Active ? z80 ->invalidateMemCache(start, size)
	          : r800->invalidateMemCache(start, size);
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

void MSXCPU::waitCycles(unsigned cycles)
{
	z80Active ? z80 ->waitCycles(cycles)
	          : r800->waitCycles(cycles);
}

void MSXCPU::waitCyclesR800(unsigned cycles)
{
	if (isR800Active()) {
		r800->waitCycles(cycles);
	}
}

CPURegs& MSXCPU::getRegisters()
{
	if (z80Active) {
		return *z80;
	} else {
		return *r800;
	}
}

void MSXCPU::update(const Setting& setting)
{
	          z80 ->update(setting);
	if (r800) r800->update(setting);
	exitCPULoopSync();
}

// Command

void MSXCPU::disasmCommand(
	Interpreter& interp, const vector<TclObject>& tokens,
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

TimeInfoTopic::TimeInfoTopic(InfoCommand& machineInfoCommand,
                             MSXCPU& msxcpu_)
	: InfoTopic(machineInfoCommand, "time")
	, msxcpu(msxcpu_)
{
}

void TimeInfoTopic::execute(const vector<TclObject>& /*tokens*/,
                            TclObject& result) const
{
	EmuDuration dur = msxcpu.getCurrentTime() - msxcpu.reference;
	result.setDouble(dur.toDouble());
}

string TimeInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Prints the time in seconds that the MSX is powered on\n";
}


// class CPUFreqInfoTopic

CPUFreqInfoTopic::CPUFreqInfoTopic(InfoCommand& machineInfoCommand,
                                   const string& name, CPUClock& clock_)
	: InfoTopic(machineInfoCommand, name)
	, clock(clock_)
{
}

void CPUFreqInfoTopic::execute(const vector<TclObject>& /*tokens*/,
                               TclObject& result) const
{
	result.setInt(clock.getFreq());
}

string CPUFreqInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Returns the actual frequency of this CPU.\n"
	       "This frequency can vary because:\n"
	       " - the user has overridden the freq via the '{z80,r800}_freq' setting\n"
	       " - (only on some MSX machines) the MSX software can switch the Z80 between 2 frequencies\n"
	       "See also the '{z80,r800}_freq_locked' setting.\n";
}


// class MSXCPUDebuggable

static const char* const CPU_REGS_DESC =
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

MSXCPUDebuggable::MSXCPUDebuggable(MSXMotherBoard& motherboard, MSXCPU& cpu_)
	: SimpleDebuggable(motherboard, "CPU regs", CPU_REGS_DESC, 28)
	, cpu(cpu_)
{
}

byte MSXCPUDebuggable::read(unsigned address)
{
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
	                4 * (regs.getIFF1() && !regs.debugGetAfterEI());
	default: UNREACHABLE; return 0;
	}
}

void MSXCPUDebuggable::write(unsigned address, byte value)
{
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
		ar.serialize("z80Active", z80Active);
		ar.serialize("newZ80Active", newZ80Active);
	} else {
		// backwards-compatibility
		assert(ar.isLoader());

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
}
INSTANTIATE_SERIALIZE_METHODS(MSXCPU);

} // namespace openmsx
