// This class implements the Intel 8255 chip
//
// * Only the 8255 is emulated, no surrounding hardware.
//   Use the class I8255Interface to do that.
// * Only mode 0 (basic input/output) is implemented

#ifndef I8255_HH
#define I8255_HH

#include "EmuTime.hh"
#include "TclCallback.hh"
#include "openmsx.hh"
#include "serialize_meta.hh"

namespace openmsx {

class I8255Interface;
class StringSetting;

class I8255
{
public:
	I8255(I8255Interface& interf, EmuTime::param time,
	      StringSetting& invalidPpiModeSetting);

	// CPU side
	void reset(EmuTime::param time);
	[[nodiscard]] byte read(byte port, EmuTime::param time);
	[[nodiscard]] byte peek(byte port, EmuTime::param time) const;
	void write(byte port, byte value, EmuTime::param time);

	// Peripheral side, pull-interface
	// (the I8255Interface class implements the push-interface)
	byte getPortA() const;
	byte getPortB() const;
	byte getPortC() const;

	void serialize(Archive auto& ar, unsigned version);

private:
	[[nodiscard]] byte readPortA(EmuTime::param time);
	[[nodiscard]] byte readPortB(EmuTime::param time);
	[[nodiscard]] byte readPortC(EmuTime::param time);
	[[nodiscard]] byte readControlPort(EmuTime::param time) const;
	[[nodiscard]] byte peekPortA(EmuTime::param time) const;
	[[nodiscard]] byte peekPortB(EmuTime::param time) const;
	[[nodiscard]] byte peekPortC(EmuTime::param time) const;
	void writePortA(byte value, EmuTime::param time);
	void writePortB(byte value, EmuTime::param time);
	void writePortC(byte value, EmuTime::param time);
	void writeControlPort(byte value, EmuTime::param time);

	[[nodiscard]] byte readC0(EmuTime::param time);
	[[nodiscard]] byte readC1(EmuTime::param time);
	[[nodiscard]] byte peekC0(EmuTime::param time) const;
	[[nodiscard]] byte peekC1(EmuTime::param time) const;
	void outputPortA(byte value, EmuTime::param time);
	void outputPortB(byte value, EmuTime::param time);
	void outputPortC(byte value, EmuTime::param time);

private:
	I8255Interface& interface;

	byte control;
	byte latchPortA;
	byte latchPortB;
	byte latchPortC;

	TclCallback ppiModeCallback;
};

} // namespace openmsx

#endif
