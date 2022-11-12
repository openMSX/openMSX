#include "MSXPSG.hh"
#include "LedStatus.hh"
#include "CassettePort.hh"
#include "MSXMotherBoard.hh"
#include "JoystickPort.hh"
#include "RenShaTurbo.hh"
#include "StringOp.hh"
#include "serialize.hh"
#include "checked_cast.hh"
#include "stl.hh"

namespace openmsx {

[[nodiscard]] static byte getKeyboardLayout(const MSXPSG& psg)
{
	// As many (mostly European) configs do not specify the layout
	// in the PSG config, assume 50on for these cases.
	auto value = psg.getDeviceConfig().getChildData("keyboardlayout", "50on");
	StringOp::casecmp cmp; // case-insensitive
	if (cmp(value, "50on")) {
		return 0x00;
	} else if (cmp(value, "jis")) {
		return 0x40;
	}
	throw MSXException(
		"Illegal keyboard layout configuration in '", psg.getName(),
	        "' device configuration: '", value,
		"', expected 'jis' or '50on'.");
}

// MSXDevice
MSXPSG::MSXPSG(const DeviceConfig& config)
	: MSXDevice(config)
	, cassette(getMotherBoard().getCassettePort())
	, renShaTurbo(getMotherBoard().getRenShaTurbo())
	, ports(generate_array<2>([&](auto i) { return &getMotherBoard().getJoystickPort(unsigned(i)); }))
	, keyLayout(getKeyboardLayout(*this))
	, addressMask(config.getChildDataAsBool("mirrored_registers", true) ? 0x0f : 0xff)
	, ay8910(getName(), *this, config, getCurrentTime())
{
	reset(getCurrentTime());
}

MSXPSG::~MSXPSG()
{
	powerDown(EmuTime::dummy());
}

void MSXPSG::reset(EmuTime::param time)
{
	registerLatch = 0;
	ay8910.reset(time);
}

void MSXPSG::powerDown(EmuTime::param /*time*/)
{
	getLedStatus().setLed(LedStatus::KANA, false);
}

byte MSXPSG::readIO(word /*port*/, EmuTime::param time)
{
	return ay8910.readRegister(registerLatch, time);
}

byte MSXPSG::peekIO(word /*port*/, EmuTime::param time) const
{
	return ay8910.peekRegister(registerLatch, time);
}

void MSXPSG::writeIO(word port, byte value, EmuTime::param time)
{
	switch (port & 0x03) {
	case 0:
		registerLatch = value & addressMask;
		break;
	case 1:
		ay8910.writeRegister(registerLatch, value, time);
		break;
	}
}


// AY8910Periphery
byte MSXPSG::readA(EmuTime::param time)
{
	byte joystick = ports[selectedPort]->read(time) |
	                ((renShaTurbo.getSignal(time)) ? 0x10 : 0x00);

#if 0
	This breaks NinjaTap test programs.
	Perhaps this behavior is different between MSX machines with a discrete
	AY8910 chip versus machines with the PSG integrated in the MSX-engine?

	// pin 6,7 input is ANDed with pin 6,7 output
	byte pin67 = prev << (4 - 2 * selectedPort);
	joystick &= (pin67| 0xCF);
#endif

	byte cassetteInput = cassette.cassetteIn(time) ? 0x80 : 0x00;
	return joystick | keyLayout | cassetteInput;
}

void MSXPSG::writeB(byte value, EmuTime::param time)
{
	byte val0 =  (value & 0x03)       | ((value & 0x10) >> 2);
	byte val1 = ((value & 0x0C) >> 2) | ((value & 0x20) >> 3);
	ports[0]->write(val0, time);
	ports[1]->write(val1, time);
	selectedPort = (value & 0x40) >> 6;

	if ((prev ^ value) & 0x80) {
		getLedStatus().setLed(LedStatus::KANA, !(value & 0x80));
	}
	prev = value;
}

// version 1: initial version
// version 2: joystickportA/B moved from here to MSXMotherBoard
template<typename Archive>
void MSXPSG::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("ay8910", ay8910);
	if (ar.versionBelow(version, 2)) {
		assert(Archive::IS_LOADER);
		// in older versions there were always 2 real joystick ports
		ar.serialize("joystickportA", *checked_cast<JoystickPort*>(ports[0]),
		             "joystickportB", *checked_cast<JoystickPort*>(ports[1]));
	}
	ar.serialize("registerLatch", registerLatch);
	byte portB = prev;
	ar.serialize("portB", portB);
	if constexpr (Archive::IS_LOADER) {
		writeB(portB, getCurrentTime());
	}
	// selectedPort is derived from portB
}
INSTANTIATE_SERIALIZE_METHODS(MSXPSG);
REGISTER_MSXDEVICE(MSXPSG, "PSG");

} // namespace openmsx
