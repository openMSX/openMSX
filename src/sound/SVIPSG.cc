#include "SVIPSG.hh"

#include "JoystickPort.hh"
#include "LedStatus.hh"
#include "MSXCPUInterface.hh"
#include "MSXMotherBoard.hh"
#include "serialize.hh"

#include "stl.hh"

//        Slot 0    Slot 1    Slot 2    Slot 3
// FFFF +---------+---------+---------+---------+
//      | Bank 2  | Bank 12 | Bank 22 | Bank 32 | Page 3
//      |   RAM   |ROM Cart |   RAM   |   RAM   |
// 8000 |00000000 |01010000 |10100000 |11110000 | Page 2
//      +---------+---------+---------+---------+
// 7FFF | Bank 1  | Bank 11 | Bank 21 | Bank 31 | Page 1
//      |ROM BASIC|ROM Cart |   RAM   |   RAM   |
//      |00000000 |00000101 |00001010 |00001111 | Page 0
// 0000 +---------+---------+---------+---------+
//
// PSG Port A Input
// Bit Name   Description
//  0  FWD1   Joystick 1, Forward
//  1  BACK1  Joystick 1, Back
//  2  LEFT1  Joystick 1, Left
//  3  RIGHT1 Joystick 1, Right
//  4  FWD2   Joystick 2, Forward
//  5  BACK2  Joystick 2, Back
//  6  LEFT2  Joystick 2, Left
//  7  RIGHT2 Joystick 2, Right
//
// PSG Port B Output
// Bit Name    Description
// 0   /CART   Memory bank 11, ROM 0000-7FFF (cartridge /CCS1, /CCS2)
// 1   /BK21   Memory bank 21, RAM 0000-7FFF
// 2   /BK22   Memory bank 22, RAM 8000-FFFF
// 3   /BK31   Memory bank 31, RAM 0000-7FFF
// 4   /BK32   Memory bank 32, RAM 8000-7FFF
// 5   CAPS    Caps-Lock LED
// 6   /ROMEN0 Memory bank 12, ROM 8000-BFFF* (cartridge /CCS3)
// 7   /ROMEN1 Memory bank 12, ROM C000-FFFF* (cartridge /CCS4)
//
// * The /CART signal must be active for any effect,
//   then all banks of RAM are disabled.

namespace openmsx {

// MSXDevice

SVIPSG::SVIPSG(const DeviceConfig& config)
	: MSXDevice(config)
	, ports(generate_array<2>([&](auto i) { return &getMotherBoard().getJoystickPort(unsigned(i)); }))
	, ay8910("PSG", *this, config, getCurrentTime())
{
	reset(getCurrentTime());
}

SVIPSG::~SVIPSG()
{
	powerDown(EmuTime::dummy());
}

void SVIPSG::reset(EmuTime time)
{
	registerLatch = 0;
	ay8910.reset(time);
}

void SVIPSG::powerDown(EmuTime /*time*/)
{
	getMotherBoard().getLedStatus().setLed(LedStatus::CAPS, false);
}

byte SVIPSG::readIO(uint16_t /*port*/, EmuTime time)
{
	return ay8910.readRegister(registerLatch, time);
}

byte SVIPSG::peekIO(uint16_t /*port*/, EmuTime time) const
{
	return ay8910.peekRegister(registerLatch, time);
}

void SVIPSG::writeIO(uint16_t port, byte value, EmuTime time)
{
	switch (port & 0x07) {
	case 0:
		registerLatch = value & 0x0F;
		break;
	case 4:
		ay8910.writeRegister(registerLatch, value, time);
		break;
	}
}

// AY8910Periphery

byte SVIPSG::readA(EmuTime time)
{
	return byte(((ports[1]->read(time) & 0x0F) << 4) |
	            ((ports[0]->read(time) & 0x0F) << 0));
}

void SVIPSG::writeB(byte value, EmuTime /*time*/)
{
	getMotherBoard().getLedStatus().setLed(LedStatus::CAPS, (value & 0x20) != 0);

	// Default to bank 1 and 2
	byte psReg = 0;
	switch (~value & 0x14) {
	case 0x04: // bk22
		psReg = 0xa0;
		break;
	case 0x10: // bk32
		psReg = 0xf0;
		break;
	}
	switch (~value & 0x0B) {
	case 1: // bk12 (cart)?
		if ((~value & 0x80) || (~value & 0x40)) {
			psReg = 0x50;
		}
		// bk11 (cart)
		psReg |= 0x05;
		break;
	case 2: // bk21
		psReg |= 0x0a;
		break;
	case 8: // bk31
		psReg |= 0x0f;
		break;
	}
	getMotherBoard().getCPUInterface().setPrimarySlots(psReg);

	prev = value;
}

template<typename Archive>
void SVIPSG::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ay8910",        ay8910,
	             "registerLatch", registerLatch);
	byte portB = prev;
	ar.serialize("portB", portB);
	if constexpr (Archive::IS_LOADER) {
		writeB(portB, getCurrentTime());
	}
}
INSTANTIATE_SERIALIZE_METHODS(SVIPSG);
REGISTER_MSXDEVICE(SVIPSG, "SVI-328 PSG");

} // namespace openmsx
