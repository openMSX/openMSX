#include "SC3000PPI.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "CassettePort.hh"
#include "JoystickPort.hh"
#include "XMLElement.hh"
#include "serialize.hh"

// Keyboard Matrix
//
//  Column/ |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
//  Line    |     |     |     |     |     |     |     |     |
//  --------+-----+-----+-----+-----+-----+-----+-----+-----|
//   0      |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
//   1      |  8  |  9  |  :  |  '  |  ,  |  =  |  .  |  /  |
//   2      |  -  |  A  |  B  |  C  |  D  |  E  |  F  |  G  |
//   3      |  H  |  I  |  J  |  K  |  L  |  M  |  N  |  O  |
//   4      |  P  |  Q  |  R  |  S  |  T  |  U  |  V  |  W  |
//   5      |  X  |  Y  |  Z  |  [  |  \  |  ]  | BS  | UP  |
//   6      |SHIFT|CTRL |LGRAP|RGRAP| ESC |STOP |ENTER|LEFT |
//   7      | F1  | F2  | F3  | F4  | F5  | CLS | INS |DOWN |
//   8      |SPACE| TAB | DEL |CAPS | SEL |PRINT|     |RIGHT|
//   9*     |  0  |  1  |  2  |  3  |  4  |  5  |  6  |  7  |
//  10*     |  8  |  9  |  +  |  -  |  *  |  /  |  .  |  ,  |
//  ---------------------------------------------------------
//  * Numerical keypad (SVI-328 only)

//  PPI Port A Input (98H)
//  Bit Name      Description
//   0  TA        Paddle or tablet 1, /SENSE
//   1  TB        Paddle or tablet 1, EOC
//   2  TC        Paddle or tablet 2, /SENSE
//   3  TD        Paddle or tablet 2, EOC
//   4  /TRIGGER1 Joystick 1, Trigger
//   5  /TRIGGER2 Joystick 2, Trigger
//   6  /READY    Cassette, Ready
//   7  CASR      Cassette, Read data
//
//  PPI Port B Input (99H)
//  Bit Name Description
//   0  IN0  Keyboard, Column status of selected line
//   1  IN1  Keyboard, Column status of selected line
//   2  IN2  Keyboard, Column status of selected line
//   3  IN3  Keyboard, Column status of selected line
//   4  IN4  Keyboard, Column status of selected line
//   5  IN5  Keyboard, Column status of selected line
//   6  IN6  Keyboard, Column status of selected line
//   7  IN7  Keyboard, Column status of selected line
//
//  PPI Port C Output (96H)
//  Bit Name   Description
//   0  KB0    Keyboard, Line select 0
//   1  KB1    Keyboard, Line select 1
//   2  KB2    Keyboard, Line select 2
//   3  KB3    Keyboard, Line select 3
//   4  CASON  Cassette, Motor relay control (0=on, 1=off)
//   5  CASW   Cassette, Write data
//   6  CASAUD Cassette, Audio control (1=enable other channel in, 0=disable channel)
//   7  SOUND  Keyboard, Click sound bit (pulse)
//
//  PPI Mode control, Output (97H)
//
//  PPI Port C, Input (9AH)

namespace openmsx {

// MSXDevice

SC3000PPI::SC3000PPI(const DeviceConfig& config)
	: MSXDevice(config)
	, cassettePort(getMotherBoard().getCassettePort())
	, i8255(*this, getCurrentTime(), getCliComm())
	, keyboard(
		config.getMotherBoard(),
		config.getMotherBoard().getScheduler(),
		config.getMotherBoard().getCommandController(),
		config.getMotherBoard().getReactor().getEventDistributor(),
		config.getMotherBoard().getMSXEventDistributor(),
		config.getMotherBoard().getStateChangeDistributor(),
		Keyboard::MATRIX_SEGA, config)
	, prevBits(15)
	, selectedRow(0)
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
	if (ar.isLoader()) {
		selectedRow = (portC >> 0) & 0xF;
		nibble bits = (portC >> 4) & 0xF;
		writeC1(bits, getCurrentTime());
	}
	ar.serialize("keyboard", keyboard);
}
INSTANTIATE_SERIALIZE_METHODS(SC3000PPI);
REGISTER_MSXDEVICE(SC3000PPI, "SC-3000 PPI");

} // namespace openmsx
