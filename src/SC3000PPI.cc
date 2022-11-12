#include "SC3000PPI.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "CassettePort.hh"
#include "JoystickPort.hh"
#include "XMLElement.hh"
#include "GlobalSettings.hh"
#include "serialize.hh"

namespace openmsx {

// MSXDevice

SC3000PPI::SC3000PPI(const DeviceConfig& config)
	: MSXDevice(config)
	, cassettePort(getMotherBoard().getCassettePort())
	, i8255(*this, getCurrentTime(), config.getGlobalSettings().getInvalidPpiModeSetting())
	, keyboard(
		config.getMotherBoard(),
		config.getMotherBoard().getScheduler(),
		config.getMotherBoard().getCommandController(),
		config.getMotherBoard().getReactor().getEventDistributor(),
		config.getMotherBoard().getMSXEventDistributor(),
		config.getMotherBoard().getStateChangeDistributor(),
		Keyboard::MATRIX_SEGA, config)
{
	MSXMotherBoard& motherBoard = getMotherBoard();
	auto time = getCurrentTime();

	for (unsigned i = 0; i < 2; i++) {
		ports[i] = &motherBoard.getJoystickPort(i);
		// Clear strobe bit, or MSX joysticks will stay silent.
		ports[i]->write(0xFB, time);
	}

	reset(time);
}

void SC3000PPI::reset(EmuTime::param time)
{
	i8255.reset(time);
}

byte SC3000PPI::readIO(word port, EmuTime::param time)
{
	return i8255.read(port & 0x03, time);
}

byte SC3000PPI::peekIO(word port, EmuTime::param time) const
{
	return i8255.peek(port & 0x03, time);
}

void SC3000PPI::writeIO(word port, byte value, EmuTime::param time)
{
	i8255.write(port & 0x03, value, time);
}


// I8255Interface

byte SC3000PPI::readA(EmuTime::param time)
{
	return peekA(time);
}
byte SC3000PPI::peekA(EmuTime::param time) const
{
	if (selectedRow == 7) {
		// Joystick hardware cannot detect when it's being read, so using the
		// read method for peeking should be safe.
		byte joy1 = ports[0]->read(time) & 0x3F;
		byte joy2 = ports[1]->read(time) & 0x3F;
		return joy1 | (joy2 << 6);
	} else {
		return keyboard.getKeys()[selectedRow];
	}
}
void SC3000PPI::writeA(byte /*value*/, EmuTime::param /*time*/)
{
}

byte SC3000PPI::readB(EmuTime::param time)
{
	return peekB(time);
}
byte SC3000PPI::peekB(EmuTime::param time) const
{
	// TODO: Are bits 4-7 available regardless of which keyboard row is selected?
	//       That would make sense, but check the schematics.
	if (selectedRow == 7) {
		// Joystick hardware cannot detect when it's being read, so using the
		// read method for peeking should be safe.
		byte joy2 = ports[1]->read(time) & 0x3F;
		return 0xF0 | (joy2 >> 2);
	} else {
		/*
			Signal  Description

			PB0     Keyboard input
			PB1     Keyboard input
			PB2     Keyboard input
			PB3     Keyboard input
			PB4     /CONT input from cartridge terminal B-11
			PB5     FAULT input from printer
			PB6     BUSY input from printer
			PB7     Cassette tape input
		*/
		auto& keyb = const_cast<Keyboard&>(keyboard);
		auto keys = keyb.getKeys()[selectedRow + 7];
		return 0xF0 | keys;
	}
}
void SC3000PPI::writeB(byte /*value*/, EmuTime::param /*time*/)
{
}

nibble SC3000PPI::readC1(EmuTime::param time)
{
	return peekC1(time);
}
nibble SC3000PPI::peekC1(EmuTime::param /*time*/) const
{
	// TODO: Check.
	return 15;
}
nibble SC3000PPI::readC0(EmuTime::param time)
{
	return peekC0(time);
}
nibble SC3000PPI::peekC0(EmuTime::param /*time*/) const
{
	// TODO: Is this selection readable at all? And if so, what is the value
	//       of bit 3?
	return selectedRow;
}
void SC3000PPI::writeC1(nibble value, EmuTime::param time)
{
	if ((prevBits ^ value) & 1) {
		cassettePort.setMotor((value & 1) == 0, time); // 0=0n, 1=Off
	}
	if ((prevBits ^ value) & 2) {
		cassettePort.cassetteOut((value & 2) != 0, time);
	}
	//if ((prevBits ^ value) & 4) {
	//	cassetteDevice.Mute(); // CASAUD, mute case speker (1=enable, 0=disable)
	//}
	//if ((prevBits ^ value) & 8) {
	//	click.setClick((value & 8) != 0, time);
	//}
	prevBits = value;
}
void SC3000PPI::writeC0(nibble value, EmuTime::param /*time*/)
{
	selectedRow = value & 7;
	//fprintf(stderr, "SC3000PPI: selected row %d\n", selectedRow);
}

template<typename Archive>
void SC3000PPI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("i8255", i8255);

	// merge prevBits and selectedRow into one byte
	byte portC = (prevBits << 4) | (selectedRow << 0);
	ar.serialize("portC", portC);
	if constexpr (Archive::IS_LOADER) {
		selectedRow = (portC >> 0) & 0xF;
		nibble bits = (portC >> 4) & 0xF;
		writeC1(bits, getCurrentTime());
	}
	ar.serialize("keyboard", keyboard);
}
INSTANTIATE_SERIALIZE_METHODS(SC3000PPI);
REGISTER_MSXDEVICE(SC3000PPI, "SC-3000 PPI");

} // namespace openmsx
