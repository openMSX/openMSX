// $Id$

#include "MSXS1990.hh"
#include "MSXCPU.hh"
#include "PanasonicMemory.hh"

namespace openmsx {

MSXS1990::MSXS1990(const XMLElement& config, const EmuTime& time)
	: MSXDevice(config, time)
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

byte MSXS1990::readIO(byte port, const EmuTime& /*time*/)
{
	switch (port & 0x01) {
	case 0:
		return registerSelect;
	case 1:
		PRT_DEBUG("S1990: read reg "<<(int)registerSelect);
		switch (registerSelect) {
		case 5:
			return firmwareSwitch.getStatus() ? 0x40 : 0x00;
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
	default: // unreachable, avoid warning
		assert(false);
		return 0;
	}
}

void MSXS1990::writeIO(byte port, byte value, const EmuTime& /*time*/)
{
	switch (port & 0x01) {
	case 0:
		registerSelect = value;
		break;
	case 1:
		switch (registerSelect) {
		case 6:
			setCPUStatus(value);
			break;
		}
		break;
	default:
		assert(false);
	}
}

void MSXS1990::setCPUStatus(byte value)
{
	cpuStatus = value & 0x60;
	MSXCPU::instance().setActiveCPU((cpuStatus & 0x20) ? MSXCPU::CPU_Z80 :
	                                                     MSXCPU::CPU_R800);
	PanasonicMemory::instance().setDRAM((cpuStatus & 0x40) ? false : true);
	// TODO bit 7 -> reset MSX ?????
}

} // namespace openmsx
