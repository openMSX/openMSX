// $Id$

#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "Debugger.hh"
#include "Scheduler.hh"
#include "SimpleDebuggable.hh"
#include "BooleanSetting.hh"
#include "CPUCore.hh"
#include "Z80.hh"
#include "R800.hh"
#include "BreakPoint.hh"
#include "InfoTopic.hh"
#include "TclObject.hh"
#include "serialize.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class TimeInfoTopic : public InfoTopic
{
public:
	TimeInfoTopic(InfoCommand& machineInfoCommand,
	              MSXCPU& msxcpu);
	virtual void execute(const vector<TclObject*>& tokens,
	                     TclObject& result) const;
	virtual std::string help (const vector<string>& tokens) const;
private:
	MSXCPU& msxcpu;
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
	, traceSetting(new BooleanSetting(motherboard.getCommandController(),
	                              "cputrace", "CPU tracing on/off", false,
	                              Setting::DONT_SAVE))
	, z80 (new CPUCore<Z80TYPE> (motherboard, "z80",  *traceSetting,
	                             EmuTime::zero))
	, r800(motherboard.isTurboR()
	       ? new CPUCore<R800TYPE>(motherboard, "r800", *traceSetting,
	                               EmuTime::zero)
	       : NULL)
	, reference(EmuTime::zero)
	, timeInfo(new TimeInfoTopic(
		motherboard.getMachineInfoCommand(), *this))
	, debuggable(new MSXCPUDebuggable(motherboard_, *this))
{
	activeCPU = z80.get(); // setActiveCPU(CPU_Z80);
	newCPU = 0;

	motherboard.getDebugger().setCPU(this);
	motherboard.getScheduler().setCPU(this);
	traceSetting->attach(*this);
}

MSXCPU::~MSXCPU()
{
	traceSetting->detach(*this);
	motherboard.getScheduler().setCPU(0);
	motherboard.getDebugger().setCPU(0);
}

void MSXCPU::setInterface(MSXCPUInterface* interface)
{
	z80 ->setInterface(interface);
	if (r800.get()) {
		r800->setInterface(interface);
	}
}

void MSXCPU::doReset(EmuTime::param time)
{
	z80 ->doReset(time);
	if (r800.get()) {
		r800->doReset(time);
	}

	reference = time;
}

void MSXCPU::setActiveCPU(CPUType cpu)
{
	CPU* tmp;
	switch (cpu) {
		case CPU_Z80:
			PRT_DEBUG("Active CPU: Z80");
			tmp = z80.get();
			break;
		case CPU_R800:
			PRT_DEBUG("Active CPU: R800");
			assert(r800.get());
			tmp = r800.get();
			break;
		default:
			assert(false);
			tmp = NULL; // prevent warning
	}
	if (tmp != activeCPU) {
		exitCPULoopSync();
		newCPU = tmp;
	}
}

void MSXCPU::setDRAMmode(bool dram)
{
	assert(r800.get());
	r800->setDRAMmode(dram);
}

void MSXCPU::execute()
{
	if (newCPU) {
		newCPU->warp(activeCPU->getCurrentTime());
		newCPU->invalidateMemCache(0x0000, 0x10000);
		activeCPU = newCPU;
		newCPU = 0;
	}
	activeCPU->execute();
}

void MSXCPU::exitCPULoopSync()
{
	activeCPU->exitCPULoopSync();
}
void MSXCPU::exitCPULoopAsync()
{
	activeCPU->exitCPULoopAsync();
}


EmuTime::param MSXCPU::getCurrentTime() const
{
	return activeCPU->getCurrentTime();
}

void MSXCPU::setNextSyncPoint(EmuTime::param time)
{
	activeCPU->setNextSyncPoint(time);
}


void MSXCPU::updateVisiblePage(byte page, byte primarySlot, byte secondarySlot)
{
	invalidateMemCache(page * 0x4000, 0x4000);
	if (r800.get()) {
		r800->updateVisiblePage(page, primarySlot, secondarySlot);
	}
}

void MSXCPU::invalidateMemCache(word start, unsigned size)
{
	activeCPU->invalidateMemCache(start, size);
}

void MSXCPU::raiseIRQ()
{
	z80 ->raiseIRQ();
	if (r800.get()) {
		r800->raiseIRQ();
	}
}
void MSXCPU::lowerIRQ()
{
	z80 ->lowerIRQ();
	if (r800.get()) {
		r800->lowerIRQ();
	}
}
void MSXCPU::raiseNMI()
{
	z80 ->raiseNMI();
	if (r800.get()) {
		r800->raiseNMI();
	}
}
void MSXCPU::lowerNMI()
{
	z80 ->lowerNMI();
	if (r800.get()) {
		r800->lowerNMI();
	}
}

bool MSXCPU::isR800Active()
{
	return activeCPU == r800.get();
}

void MSXCPU::setZ80Freq(unsigned freq)
{
	z80->setFreq(freq);
}

void MSXCPU::wait(EmuTime::param time)
{
	activeCPU->wait(time);
}

void MSXCPU::waitCycles(unsigned cycles)
{
	activeCPU->waitCycles(cycles);
}

void MSXCPU::update(const Setting& setting)
{
	(void)setting;
	assert(&setting == traceSetting.get());
	exitCPULoopSync();
}

// Command

void MSXCPU::doStep()
{
	activeCPU->doStep();
}

void MSXCPU::doContinue()
{
	activeCPU->doContinue();
}

void MSXCPU::doBreak()
{
	activeCPU->doBreak();
}

bool MSXCPU::isBreaked()
{
	return activeCPU->isBreaked();
}

void MSXCPU::disasmCommand(const vector<TclObject*>& tokens,
                           TclObject& result) const
{
	activeCPU->disasmCommand(tokens, result);
}

void MSXCPU::insertBreakPoint(std::auto_ptr<BreakPoint> bp)
{
	activeCPU->insertBreakPoint(bp);
}

void MSXCPU::removeBreakPoint(const BreakPoint& bp)
{
	activeCPU->removeBreakPoint(bp);
}

const CPU::BreakPoints& MSXCPU::getBreakPoints() const
{
	return activeCPU->getBreakPoints();
}


void MSXCPU::setPaused(bool paused)
{
	activeCPU->setPaused(paused);
}


// class TimeInfoTopic

TimeInfoTopic::TimeInfoTopic(InfoCommand& machineInfoCommand,
                             MSXCPU& msxcpu_)
	: InfoTopic(machineInfoCommand, "time")
	, msxcpu(msxcpu_)
{
}

void TimeInfoTopic::execute(const vector<TclObject*>& /*tokens*/,
                            TclObject& result) const
{
	EmuDuration dur = msxcpu.getCurrentTime() - msxcpu.reference;
	result.setDouble(dur.toDouble());
}

string TimeInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Prints the time in seconds that the MSX is powered on\n";
}


// class MSXCPUDebuggable

//  0 ->  A      1 ->  F      2 -> B       3 -> C
//  4 ->  D      5 ->  E      6 -> H       7 -> L
//  8 ->  A'     9 ->  F'    10 -> B'     11 -> C'
// 12 ->  D'    13 ->  E'    14 -> H'     15 -> L'
// 16 -> IXH    17 -> IXL    18 -> IYH    19 -> IYL
// 20 -> PCH    21 -> PCL    22 -> SPH    23 -> SPL
// 24 ->  I     25 ->  R     26 -> IM     27 -> IFF1/2

MSXCPUDebuggable::MSXCPUDebuggable(MSXMotherBoard& motherboard, MSXCPU& cpu_)
	: SimpleDebuggable(motherboard, "CPU regs",
	                   "Registers of the active CPU (Z80 or R800)", 28)
	, cpu(cpu_)
{
}

byte MSXCPUDebuggable::read(unsigned address)
{
	const CPU::CPURegs& regs = cpu.activeCPU->getRegisters();
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
	case 27: return regs.getIFF1() + 2 * regs.getIFF2();
	}
	assert(false);
	return 0;
}

void MSXCPUDebuggable::write(unsigned address, byte value)
{
	CPU::CPURegs& regs = cpu.activeCPU->getRegisters();
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
		regs.setIFF1(value & 0x01);
		regs.setIFF2(value & 0x02);
		break;
	default:
		assert(false);
		break;
	}
}


template<typename Archive>
void MSXCPU::serialize(Archive& ar, unsigned /*version*/)
{
	ar.serialize("z80",  *z80);
	if (r800.get()) {
		ar.serialize("r800", *r800);
	}
	ar.serializePointerID("activeCPU", activeCPU);
	ar.serializePointerID("newCPU",    newCPU);
	ar.serialize("resetTime", reference);
}
INSTANTIATE_SERIALIZE_METHODS(MSXCPU);

} // namespace openmsx
