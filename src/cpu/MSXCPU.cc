// $Id$

#include "MSXCPU.hh"
#include "CPU.hh"
#include "InfoCommand.hh"
#include "Debugger.hh"
#include "BooleanSetting.hh"
#include "CPUCore.hh"
#include "Z80.hh"
#include "R800.hh"
#include "BreakPoint.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

MSXCPU::MSXCPU(Debugger& debugger_)
	: traceSetting(new BooleanSetting("cputrace", "CPU tracing on/off", false))
	, z80 (new CPUCore<Z80TYPE> ("z80",  *traceSetting, EmuTime::zero))
	, r800(new CPUCore<R800TYPE>("r800", *traceSetting, EmuTime::zero))
	, timeInfo(*this)
	, infoCmd(InfoCommand::instance())
	, debugger(debugger_)
{
	activeCPU = z80.get();	// setActiveCPU(CPU_Z80);
	reset(EmuTime::zero);

	infoCmd.registerTopic("time", &timeInfo);
	debugger.setCPU(this);
	debugger.registerDebuggable("CPU regs", *this);

	traceSetting->addListener(this);
}

MSXCPU::~MSXCPU()
{
	traceSetting->removeListener(this);

	debugger.unregisterDebuggable("CPU regs", *this);
	debugger.setCPU(0);
	infoCmd.unregisterTopic("time", &timeInfo);
}

void MSXCPU::setMotherboard(MSXMotherBoard* motherboard)
{
	z80 ->setMotherboard(motherboard);
	r800->setMotherboard(motherboard);
}

void MSXCPU::setInterface(MSXCPUInterface* interface)
{
	z80 ->setInterface(interface);
	r800->setInterface(interface);
}

void MSXCPU::reset(const EmuTime& time)
{
	z80 ->reset(time);
	r800->reset(time);

	reference = time;
}


void MSXCPU::setActiveCPU(CPUType cpu)
{
	CPU* newCPU;
	switch (cpu) {
		case CPU_Z80:
			PRT_DEBUG("Active CPU: Z80");
			newCPU = z80.get();
			break;
		case CPU_R800:
			PRT_DEBUG("Active CPU: R800");
			newCPU = r800.get();
			break;
		default:
			assert(false);
			newCPU = NULL;	// prevent warning
	}
	if (newCPU != activeCPU) {
		newCPU->advance(activeCPU->getCurrentTime());
		newCPU->invalidateMemCache(0x0000, 0x10000);
		exitCPULoop();
		activeCPU = newCPU;
	}
}

void MSXCPU::setDRAMmode(bool dram)
{
	r800->setDRAMmode(dram);
}

void MSXCPU::execute()
{
	activeCPU->execute();
}

void MSXCPU::exitCPULoop()
{
	activeCPU->exitCPULoop();
}


const EmuTime& MSXCPU::getCurrentTime() const
{
	return activeCPU->getCurrentTime();
}


void MSXCPU::updateVisiblePage(byte page, byte primarySlot, byte secondarySlot)
{
	invalidateMemCache(page * 0x4000, 0x4000);
	r800->updateVisiblePage(page, primarySlot, secondarySlot);
}

void MSXCPU::invalidateMemCache(word start, unsigned size)
{
	activeCPU->invalidateMemCache(start, size);
}

void MSXCPU::raiseIRQ()
{
	z80 ->raiseIRQ();
	r800->raiseIRQ();
}
void MSXCPU::lowerIRQ()
{
	z80 ->lowerIRQ();
	r800->lowerIRQ();
}
void MSXCPU::raiseNMI()
{
	z80 ->raiseNMI();
	r800->raiseNMI();
}
void MSXCPU::lowerNMI()
{
	z80 ->lowerNMI();
	r800->lowerNMI();
}

bool MSXCPU::isR800Active()
{
	return activeCPU == r800.get();
}

void MSXCPU::setZ80Freq(unsigned freq)
{
	z80->setFreq(freq);
}

void MSXCPU::wait(const EmuTime& time)
{
	activeCPU->wait(time);
}

void MSXCPU::update(const Setting* setting)
{
	if (setting); // avoid warning
	assert(setting == traceSetting.get());
	exitCPULoop();
}

// DebugInterface

//  0 ->  A      1 ->  F      2 -> B       3 -> C
//  4 ->  D      5 ->  E      6 -> H       7 -> L
//  8 ->  A'     9 ->  F'    10 -> B'     11 -> C'
// 12 ->  D'    13 ->  E'    14 -> H'     15 -> L'
// 16 -> IXH    17 -> IXL    18 -> IYH    19 -> IYL
// 20 -> PCH    21 -> PCL    22 -> SPH    23 -> SPL
// 24 ->  I     25 ->  R     26 -> IM     27 -> IFF1/2

unsigned MSXCPU::getSize() const
{
	return 28; // number of 8 bits registers (16 bits = 2 registers)
}

const string& MSXCPU::getDescription() const
{
	static const string desc =
		"Registers of the active CPU (Z80 or R800)";
	return desc;
}

byte MSXCPU::read(unsigned address)
{
	const CPU::CPURegs& regs = activeCPU->getRegisters();
	const word* registers[] = {
		&regs.AF,  &regs.BC,  &regs.DE,  &regs.HL,
		&regs.AF2, &regs.BC2, &regs.DE2, &regs.HL2,
		&regs.IX,  &regs.IY,  &regs.PC,  &regs.SP
	};

	assert(address < getSize());
	switch (address) {
	case 24:
		return regs.I;
	case 25:
		return regs.R;
	case 26:
		return regs.IM;
	case 27:
		return regs.IFF1 + 2 * regs.IFF2;
	default:
		if (address & 1) {
			return *registers[address / 2] & 255;
		} else {
			return *registers[address / 2] >> 8;
		}
	}
}

void MSXCPU::write(unsigned address, byte value)
{
	CPU::CPURegs& regs = activeCPU->getRegisters();
	word* registers[] = {
		&regs.AF,  &regs.BC,  &regs.DE,  &regs.HL,
		&regs.AF2, &regs.BC2, &regs.DE2, &regs.HL2,
		&regs.IX,  &regs.IY,  &regs.PC,  &regs.SP
	};

	assert(address < getSize());
	switch (address) {
	case 24:
		regs.I = value;
		break;
	case 25:
		regs.R = value;
		break;
	case 26:
		if (value < 3) {
			regs.IM = value;
		}
		break;
	case 27:
		regs.IFF1 = value & 0x01;
		regs.IFF2 = value & 0x02;
		break;
	default:
		word& w = *registers[address / 2];
		if (address & 1) {
			w = (w & 0xFF00) | value;
		} else {
			w = (w & 0x00FF) | (value << 8);
		}
		break;
	}
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

MSXCPU::TimeInfoTopic::TimeInfoTopic(MSXCPU& parent_)
	: parent(parent_)
{
}

void MSXCPU::TimeInfoTopic::execute(const vector<TclObject*>& /*tokens*/,
                                    TclObject& result) const
{
	EmuDuration dur = parent.getCurrentTime() - parent.reference;
	result.setDouble(dur.toDouble());
}

string MSXCPU::TimeInfoTopic::help(const vector<string>& /*tokens*/) const
{
	return "Prints the time in seconds that the MSX is powered on\n";
}

} // namespace openmsx
