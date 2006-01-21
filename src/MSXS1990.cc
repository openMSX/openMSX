// $Id$

#include "MSXS1990.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "PanasonicMemory.hh"
#include "FirmwareSwitch.hh"
#include "SimpleDebuggable.hh"

namespace openmsx {

class S1990Debuggable : public SimpleDebuggable
{
public:
	S1990Debuggable(MSXMotherBoard& motherBoard, MSXS1990& s1990);
	virtual byte read(unsigned address);
	virtual void write(unsigned address, byte value);
private:
	MSXS1990& s1990;
};


MSXS1990::MSXS1990(MSXMotherBoard& motherBoard, const XMLElement& config,
                   const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, firmwareSwitch(new FirmwareSwitch(motherBoard.getCommandController()))
	, debuggable(new S1990Debuggable(motherBoard, *this))
{
	reset(time);
}

MSXS1990::~MSXS1990()
{
}

void MSXS1990::reset(const EmuTime& /*time*/)
{
	registerSelect = 0;	// TODO check this
	setCPUStatus(96);
}

byte MSXS1990::readIO(word port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXS1990::peekIO(word port, const EmuTime& /*time*/) const
{
	switch (port & 0x01) {
	case 0:
		return registerSelect;
	case 1:
		return readRegister(registerSelect);
	default: // unreachable, avoid warning
		assert(false);
		return 0;
	}
}

void MSXS1990::writeIO(word port, byte value, const EmuTime& /*time*/)
{
	switch (port & 0x01) {
	case 0:
		registerSelect = value;
		break;
	case 1:
		writeRegister(registerSelect, value);
		break;
	default:
		assert(false);
	}
}

byte MSXS1990::readRegister(byte reg) const
{
	PRT_DEBUG("S1990: read reg " << (int)reg);
	switch (reg) {
		case 5:
			return firmwareSwitch->getStatus() ? 0x40 : 0x00;
		case 6:
			return cpuStatus;
		case 13:
			return 0x03;	//TODO
		case 14:
			return 0x2F;	//TODO
		case 15:
			return 0x8B;	//TODO
		default:
			return 0xFF;
	}
}

void MSXS1990::writeRegister(byte reg, byte value)
{
	switch (reg) {
		case 6:
			setCPUStatus(value);
			break;
	}
}

void MSXS1990::setCPUStatus(byte value)
{
	cpuStatus = value & 0x60;
	MSXCPU& cpu = getMotherBoard().getCPU();
	cpu.setActiveCPU((cpuStatus & 0x20) ? MSXCPU::CPU_Z80 :
	                                      MSXCPU::CPU_R800);
	bool dram = (cpuStatus & 0x40) ? false : true;
	cpu.setDRAMmode(dram);
	getMotherBoard().getPanasonicMemory().setDRAM(dram);
	// TODO bit 7 -> reset MSX ?????
}


S1990Debuggable::S1990Debuggable(MSXMotherBoard& motherBoard, MSXS1990& s1990_)
	: SimpleDebuggable(motherBoard, s1990_.getName() + " regs",
	                   "S19990 registers", 16)
	, s1990(s1990_)
{
}

byte S1990Debuggable::read(unsigned address)
{
	return s1990.readRegister(address);
}

void S1990Debuggable::write(unsigned address, byte value)
{
	s1990.writeRegister(address, value);
}

} // namespace openmsx
