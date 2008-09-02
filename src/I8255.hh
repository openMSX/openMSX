// $Id$

// This class implements the Intel 8255 chip
//
// * Only the 8255 is emulated, no surrounding hardware.
//   Use the class I8255Interface to do that.
// * Only mode 0 (basic input/output) is implemented

#ifndef I8255_HH
#define I8255_HH

#include "openmsx.hh"
#include "noncopyable.hh"

namespace openmsx {

class I8255Interface;
class EmuTime;
class CliComm;

class I8255 : private noncopyable
{
public:
	I8255(I8255Interface& interf, const EmuTime& time,
	      CliComm& cliComm);

	void reset(const EmuTime& time);

	byte readPortA(const EmuTime& time);
	byte readPortB(const EmuTime& time);
	byte readPortC(const EmuTime& time);
	byte readControlPort(const EmuTime& time) const;
	byte peekPortA(const EmuTime& time) const;
	byte peekPortB(const EmuTime& time) const;
	byte peekPortC(const EmuTime& time) const;
	void writePortA(byte value, const EmuTime& time);
	void writePortB(byte value, const EmuTime& time);
	void writePortC(byte value, const EmuTime& time);
	void writeControlPort(byte value, const EmuTime& time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte readC0(const EmuTime& time);
	byte readC1(const EmuTime& time);
	byte peekC0(const EmuTime& time) const;
	byte peekC1(const EmuTime& time) const;
	void outputPortA(byte value, const EmuTime& time);
	void outputPortB(byte value, const EmuTime& time);
	void outputPortC(byte value, const EmuTime& time);

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
