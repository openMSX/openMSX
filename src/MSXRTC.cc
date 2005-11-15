// $Id$

#include "MSXRTC.hh"
#include "SRAM.hh"
#include "RP5C01.hh"
#include "MSXMotherBoard.hh"
#include <cassert>

namespace openmsx {

MSXRTC::MSXRTC(MSXMotherBoard& motherBoard, const XMLElement& config,
               const EmuTime& time)
	: MSXDevice(motherBoard, config, time)
{
	sram.reset(new SRAM(motherBoard, getName() + " SRAM", 4 * 13, config));
	rp5c01.reset(new RP5C01(motherBoard.getCommandController(), *sram, time));
	registerLatch = 0; // TODO verify on real hardware
}

MSXRTC::~MSXRTC()
{
}

void MSXRTC::reset(const EmuTime& time)
{
	rp5c01->reset(time);
}

byte MSXRTC::readIO(byte port, const EmuTime& time)
{
	return peekIO(port, time);
}

byte MSXRTC::peekIO(byte /*port*/, const EmuTime& time) const
{
	return rp5c01->readPort(registerLatch, time) | 0xF0;
}

void MSXRTC::writeIO(byte port, byte value, const EmuTime& time)
{
	switch (port & 0x01) {
	case 0:
		registerLatch = value & 0x0F;
		break;
	case 1:
		rp5c01->writePort(registerLatch, value & 0x0F, time);
		break;
	default:
		assert(false);
	}
}

} // namespace openmsx

