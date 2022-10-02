#include "ColecoJoystickIO.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "JoystickPort.hh"
#include "Math.hh"
#include "enumerate.hh"
#include "serialize.hh"
#include <array>

namespace openmsx {

ColecoJoystickIO::ColecoJoystickIO(const DeviceConfig& config)
	: MSXDevice(config)
	, keyboard(
		config.getMotherBoard(),
		config.getMotherBoard().getScheduler(),
		config.getMotherBoard().getCommandController(),
		config.getMotherBoard().getReactor().getEventDistributor(),
		config.getMotherBoard().getMSXEventDistributor(),
		config.getMotherBoard().getStateChangeDistributor(),
		Keyboard::MATRIX_CVJOY,
		config)
{
	MSXMotherBoard& motherBoard = getMotherBoard();
	auto time = getCurrentTime();
	for (auto [i, port] : enumerate(ports)) {
		port = &motherBoard.getJoystickPort(unsigned(i));
		// Clear strobe bit, or MSX joysticks will stay silent.
		port->write(0xFB, time);
	}
	reset(time);
}

void ColecoJoystickIO::reset(EmuTime::param /*time*/)
{
	joyMode = 0;
}

byte ColecoJoystickIO::peekIO(word port, EmuTime::param time) const
{
	const int joyPort = (port >> 1) & 1;
	const byte joyStatus = ports[joyPort]->read(time);
	auto keys = keyboard.getKeys();

	if (joyMode == 0) {
		// Combine keypad rows, convert to high-active and drop unused bits.
		// Note: The keypad can only return one button press, but I don't know
		//       what happens if you press two buttons at once.
		unsigned keypadStatus = ~(
			keys[joyPort * 2 + 2] | (keys[joyPort * 2 + 3] << 8)
			) & 0xFFF;
		static constexpr std::array<byte, 13> keyEnc = {
			0xF, 0xA, 0xD, 0x7, 0xC, 0x2, 0x3, 0xE, 0x5, 0x1, 0xB, 0x9, 0x6
		};
		return keyEnc[Math::findFirstSet(keypadStatus)]
		     | 0x30 // not connected
		     | ((joyStatus << 1) & (keys[joyPort] >> 1) & 0x40); // trigger B
	} else {
		// Map MSX joystick direction bits to Coleco bits.
		byte value =
			  ( joyStatus       & 0x01) // up
			| ((joyStatus >> 2) & 0x02) // right
			| ((joyStatus << 1) & 0x04) // down
			| ((joyStatus << 1) & 0x08) // left
			| 0x30 // not connected
			| ((joyStatus << 2) & 0x40) // trigger A
			;
			// Note: ColEm puts 0 in bit 7.
			//       I don't know why this is different from bit 4 and 5.
		// Combine with row 0/1 of keyboard matrix.
		return value & (keys[joyPort] | ~0x4F);
	}
}

byte ColecoJoystickIO::readIO(word port, EmuTime::param time)
{
	return peekIO(port, time);
}

void ColecoJoystickIO::writeIO(word port, byte /*value*/, EmuTime::param /*time*/)
{
	joyMode = (port >> 6) & 1;
}

template<typename Archive>
void ColecoJoystickIO::serialize(Archive& ar, unsigned version)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("joyMode", joyMode);
	if (ar.versionAtLeast(version, 2)) {
		ar.serialize("keyboard", keyboard);
	}
}
INSTANTIATE_SERIALIZE_METHODS(ColecoJoystickIO);
REGISTER_MSXDEVICE(ColecoJoystickIO, "ColecoJoystick");

} // namespace openmsx
