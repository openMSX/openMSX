// $Id$

#include <cassert>
#include <sstream>
#include "MSXCPU.hh"
#include "MSXCPUInterface.hh"
#include "MSXConfig.hh"
#include "CPU.hh"
#include "InfoCommand.hh"
#include "Debugger.hh"

using std::ostringstream;

namespace openmsx {

MSXCPU::MSXCPU()
	: z80 (EmuTime::zero),
	  r800(EmuTime::zero),
	  timeInfo(*this),
	  infoCmd(InfoCommand::instance()),
	  debugger(Debugger::instance())
{
	activeCPU = &z80;	// setActiveCPU(CPU_Z80);
	reset(EmuTime::zero);

	infoCmd.registerTopic("time", &timeInfo);
	debugger.setCPU(this);
	debugger.registerDebuggable("cpu-regs", *this);
}

MSXCPU::~MSXCPU()
{
	debugger.unregisterDebuggable("cpu-regs", *this);
	debugger.setCPU(0);
	infoCmd.unregisterTopic("time", &timeInfo);
}

MSXCPU& MSXCPU::instance()
{
	static MSXCPU oneInstance;
	return oneInstance;
}

void MSXCPU::init(Scheduler* scheduler)
{
	z80 .init(scheduler);
	r800.init(scheduler);
}

void MSXCPU::setInterface(MSXCPUInterface* interface)
{
	z80 .setInterface(interface); 
	r800.setInterface(interface); 
}

void MSXCPU::reset(const EmuTime &time)
{
	z80 .reset(time);
	r800.reset(time);

	reference = time;
}


void MSXCPU::setActiveCPU(CPUType cpu)
{
	CPU *newCPU;
	switch (cpu) {
		case CPU_Z80:
			PRT_DEBUG("Active CPU: Z80");
			newCPU = &z80;
			break;
		case CPU_R800:
			PRT_DEBUG("Active CPU: R800");
			newCPU = &r800;
			break;
		default:
			assert(false);
			newCPU = NULL;	// prevent warning
	}
	if (newCPU != activeCPU) {
		const EmuTime &currentTime = activeCPU->getCurrentTime();
		const EmuTime &targetTime  = activeCPU->getTargetTime();
		activeCPU->setTargetTime(currentTime);	// stop current CPU
		newCPU->setCurrentTime(currentTime);
		newCPU->setTargetTime(targetTime);
		newCPU->invalidateCache(0x0000, 0x10000/CPU::CACHE_LINE_SIZE);
		activeCPU = newCPU;
	}
}

void MSXCPU::executeUntilTarget(const EmuTime &time)
{
	activeCPU->executeUntilTarget(time);
}

void MSXCPU::setTargetTime(const EmuTime &time)
{
	activeCPU->setTargetTime(time);
}

const EmuTime &MSXCPU::getTargetTime() const
{
	return activeCPU->getTargetTime();
}

const EmuTime &MSXCPU::getCurrentTimeUnsafe() const
{
	return activeCPU->getCurrentTime();
}


void MSXCPU::invalidateCache(word start, int num)
{
	activeCPU->invalidateCache(start, num);
}

void MSXCPU::raiseIRQ()
{
	z80 .raiseIRQ();
	r800.raiseIRQ();
}
void MSXCPU::lowerIRQ()
{
	z80 .lowerIRQ();
	r800.lowerIRQ();
}

bool MSXCPU::isR800Active()
{
	return activeCPU == &r800;
}

void MSXCPU::wait(const EmuTime &time)
{
	activeCPU->wait(time);
}


// DebugInterface

/*
static string regNames[] = {
	"AF",  "BC",  "DE",  "HL",
	"AF2", "BC2", "DE2", "HL2",
	"IX",  "IY",  "PC",  "SP",
	"IR"
};

const string MSXCPU::getRegisterName(unsigned regNr) const
{
	assert(regNr < getSize());
	return regNames[regNr / 2];
}

unsigned MSXCPU::getRegisterNumber(const string& regName) const
{
	for (int i = 0; i < (26 / 2); ++i) {
		if (regName == regNames[i]) {
			return i * 2;
		}
	}
	return 0;
}
*/

unsigned MSXCPU::getSize() const
{
	return 26; // number of 8 bits registers (16 bits = 2 registers)
}

const string& MSXCPU::getDescription() const
{
	static const string desc = 
		"Registers of the active CPU (Z80 or R800)";
	return desc;
}

byte MSXCPU::read(unsigned address)
{
	CPU::CPURegs* regs = &activeCPU->R; 
	const CPU::z80regpair* registers[] = {
		&regs->AF,  &regs->BC,  &regs->DE,  &regs->HL, 
		&regs->AF2, &regs->BC2, &regs->DE2, &regs->HL2, 
		&regs->IX,  &regs->IY,  &regs->PC,  &regs->SP
	};

	assert(address < getSize());
	switch (address) {
	case 24:
		return regs->I;
	case 25:
		return regs->R;
	default:
		if (address & 1) {
			return registers[address / 2]->B.l;
		} else {
			return registers[address / 2]->B.h;
		}
	}
}

void MSXCPU::write(unsigned address, byte value)
{
	CPU::CPURegs* regs = &activeCPU->R; 
	CPU::z80regpair* registers[] = {
		&regs->AF,  &regs->BC,  &regs->DE,  &regs->HL, 
		&regs->AF2, &regs->BC2, &regs->DE2, &regs->HL2, 
		&regs->IX,  &regs->IY,  &regs->PC,  &regs->SP
	};

	assert(address < getSize());
	switch (address) {
	case 24:
		regs->I = value;
		break;
	case 25:
		regs->R = value;
		break;
	default:
		if (address & 1) {
			registers[address / 2]->B.l = value;
		} else {
			registers[address / 2]->B.h = value;
		}
		break;
	}
}


// Command

static word getAddress(const string& str)
{
	char* endPtr;
	unsigned long addr = strtoul(str.c_str(), &endPtr, 0);
	if ((*endPtr != '\0') || (addr >= 0x10000)) {
		throw CommandException("Invalid address");
	}
	return addr;
}

string MSXCPU::doStep()
{
	activeCPU->doStep();
	return "";
}

string MSXCPU::doContinue()
{
	activeCPU->doContinue();
	return "";
}

string MSXCPU::doBreak()
{
	activeCPU->doBreak();
	return "";
}

string MSXCPU::setBreakPoint(const vector<string>& tokens)
{
	activeCPU->breakPoints.insert(getAddress(tokens[2]));
	return "";
}

string MSXCPU::removeBreakPoint(const vector<string>& tokens)
{
	word addr = getAddress(tokens[2]);
	multiset<word>::iterator it = activeCPU->breakPoints.find(addr);
	if (it != activeCPU->breakPoints.end()) {
		activeCPU->breakPoints.erase(it);
	}
	return "";
}

string MSXCPU::listBreakPoints() const
{
	ostringstream os;
	for (multiset<word>::const_iterator it = activeCPU->breakPoints.begin();
	     it != activeCPU->breakPoints.end(); ++it) {
		os << hex << *it << '\n';
	}
	return os.str();
}


// class TimeInfoTopic

MSXCPU::TimeInfoTopic::TimeInfoTopic(MSXCPU& parent_)
	: parent(parent_)
{
}

string MSXCPU::TimeInfoTopic::execute(const vector<string>& tokens) const
	throw()
{
	EmuDuration dur = parent.getCurrentTimeUnsafe() - parent.reference;
	ostringstream str;
	str.precision(3);
	str << std::fixed << std::showpoint << dur.toFloat() << '\n';
	return str.str();
}

string MSXCPU::TimeInfoTopic::help(const vector<string>& tokens) const
	throw()
{
	return "Prints the time in seconds that the MSX is powered on\n";
}

} // namespace openmsx
