#include "SVIPPI.hh"
#include "MSXMotherBoard.hh"
#include "Reactor.hh"
#include "CassettePort.hh"
#include "JoystickPort.hh"
#include "GlobalSettings.hh"
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

SVIPPI::SVIPPI(const DeviceConfig& config)
	: MSXDevice(config)
	, cassettePort(getMotherBoard().getCassettePort())
	, i8255(*this, getCurrentTime(), config.getGlobalSettings().getInvalidPpiModeSetting())
	, click(config)
	, keyboard(
		config.getMotherBoard(),
		config.getMotherBoard().getScheduler(),
		config.getMotherBoard().getCommandController(),
		config.getMotherBoard().getReactor().getEventDistributor(),
		config.getMotherBoard().getMSXEventDistributor(),
		config.getMotherBoard().getStateChangeDistributor(),
		Keyboard::Matrix::SVI, config)
{
	ports[0] = &getMotherBoard().getJoystickPort(0);
	ports[1] = &getMotherBoard().getJoystickPort(1);

	auto time = getCurrentTime();
	ports[0]->write(0, time); // TODO correct?  Bit2 must be 0 otherwise
	ports[1]->write(0, time); //      e.g. KeyJoystick doesn't work

	reset(time);
}

void SVIPPI::reset(EmuTime::param time)
{
	i8255.reset(time);
	click.reset(time);
}

byte SVIPPI::readIO(word port, EmuTime::param time)
{
	return i8255.read(port & 0x03, time);
}

byte SVIPPI::peekIO(word port, EmuTime::param time) const
{
	return i8255.peek(port & 0x03, time);
}

void SVIPPI::writeIO(word port, byte value, EmuTime::param time)
{
	i8255.write(port & 0x03, value, time);
}


// I8255Interface

byte SVIPPI::readA(EmuTime::param time)
{
	byte triggers = ((ports[0]->read(time) & 0x10) ? 0x10 : 0) |
	                ((ports[1]->read(time) & 0x10) ? 0x20 : 0);

	//byte cassetteReady = cassettePort.Ready() ? 0 : 0x40;
	byte cassetteReady = 0; // ready

	byte cassetteInput = cassettePort.cassetteIn(time) ? 0x80 : 0x00;

	return triggers | cassetteReady | cassetteInput;
}
byte SVIPPI::peekA(EmuTime::param /*time*/) const
{
	return 0; // TODO
}
void SVIPPI::writeA(byte /*value*/, EmuTime::param /*time*/)
{
}

byte SVIPPI::readB(EmuTime::param time)
{
	return peekB(time);
}
byte SVIPPI::peekB(EmuTime::param /*time*/) const
{
	return keyboard.getKeys()[selectedRow];
}
void SVIPPI::writeB(byte /*value*/, EmuTime::param /*time*/)
{
}

nibble SVIPPI::readC1(EmuTime::param time)
{
	return peekC1(time);
}
nibble SVIPPI::peekC1(EmuTime::param /*time*/) const
{
	return 15; // TODO check this
}
nibble SVIPPI::readC0(EmuTime::param time)
{
	return peekC0(time);
}
nibble SVIPPI::peekC0(EmuTime::param /*time*/) const
{
	return selectedRow;
}
void SVIPPI::writeC1(nibble value, EmuTime::param time)
{
	if ((prevBits ^ value) & 1) {
		cassettePort.setMotor((value & 1) == 0, time); // 0=0n, 1=Off
	}
	if ((prevBits ^ value) & 2) {
		cassettePort.cassetteOut((value & 2) != 0, time);
	}
	//if ((prevBits ^ value) & 4) {
	//	cassetteDevice.Mute(); // CASAUD, mute case speaker (1=enable, 0=disable)
	//}
	if ((prevBits ^ value) & 8) {
		click.setClick((value & 8) != 0, time);
	}
	prevBits = value;
}
void SVIPPI::writeC0(nibble value, EmuTime::param /*time*/)
{
	selectedRow = value;
}

template<typename Archive>
void SVIPPI::serialize(Archive& ar, unsigned /*version*/)
{
	ar.template serializeBase<MSXDevice>(*this);
	ar.serialize("i8255", i8255);

	// merge prevBits and selectedRow into one byte
	auto portC = byte((prevBits << 4) | (selectedRow << 0));
	ar.serialize("portC", portC);
	if constexpr (Archive::IS_LOADER) {
		selectedRow = (portC >> 0) & 0xF;
		nibble bits = (portC >> 4) & 0xF;
		writeC1(bits, getCurrentTime());
	}
	ar.serialize("keyboard", keyboard);
}
INSTANTIATE_SERIALIZE_METHODS(SVIPPI);
REGISTER_MSXDEVICE(SVIPPI, "SVI-328 PPI");

} // namespace openmsx
