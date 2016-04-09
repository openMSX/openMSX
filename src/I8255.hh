// This class implements the Intel 8255 chip
//
// * Only the 8255 is emulated, no surrounding hardware.
//   Use the class I8255Interface to do that.
// * Only mode 0 (basic input/output) is implemented

#ifndef I8255_HH
#define I8255_HH

#include "EmuTime.hh"
#include "openmsx.hh"

namespace openmsx {

class I8255Interface;
class CliComm;

class I8255
{
public:
	I8255(I8255Interface& interf, EmuTime::param time,
	      CliComm& cliComm);

	void reset(EmuTime::param time);

	byte readPortA(EmuTime::param time);
	byte readPortB(EmuTime::param time);
	byte readPortC(EmuTime::param time);
	byte readControlPort(EmuTime::param time) const;
	byte peekPortA(EmuTime::param time) const;
	byte peekPortB(EmuTime::param time) const;
	byte peekPortC(EmuTime::param time) const;
	void writePortA(byte value, EmuTime::param time);
	void writePortB(byte value, EmuTime::param time);
	void writePortC(byte value, EmuTime::param time);
	void writeControlPort(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readC0(EmuTime::param time);
	byte readC1(EmuTime::param time);
	byte peekC0(EmuTime::param time) const;
	byte peekC1(EmuTime::param time) const;
	void outputPortA(byte value, EmuTime::param time);
	void outputPortB(byte value, EmuTime::param time);
	void outputPortC(byte value, EmuTime::param time);

	I8255Interface& interface;
	CliComm& cliComm;

	byte control;
	byte latchPortA;
	byte latchPortB;
	byte latchPortC;

	bool warningPrinted;
};

} // namespace openmsx

#endif
