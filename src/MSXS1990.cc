// $Id$

#include "MSXS1990.hh"
#include "MSXCPU.hh"
#include "MSXMotherBoard.hh"
#include "PanasonicMemory.hh"
#include "FirmwareSwitch.hh"

namespace openmsx {

MSXS1990::MSXS1990(MSXMotherBoard& motherBoard, const XMLElement& config,
                   const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
	, SimpleDebuggable(motherBoard, getName() + " regs",
	                   "S19990 registers", 16)
	, firmwareSwitch(new FirmwareSwitch(motherBoard.getCommandController()))
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


byte MSXS1990::read(unsigned address)
{
	return readRegister(address);
}

void MSXS1990::write(unsigned address, byte value)
{
	writeRegister(address, value);
}

} // namespace openmsx
