#ifndef I8255INTERFACE_HH
#define I8255INTERFACE_HH

#include "EmuTime.hh"
#include "openmsx.hh"

namespace openmsx {

class I8255Interface
{
public:
	virtual byte readA(EmuTime::param time) = 0;
	virtual byte readB(EmuTime::param time) = 0;
	virtual nibble readC0(EmuTime::param time) = 0;
	virtual nibble readC1(EmuTime::param time) = 0;
	virtual byte peekA(EmuTime::param time) const = 0;
	virtual byte peekB(EmuTime::param time) const = 0;
	virtual nibble peekC0(EmuTime::param time) const = 0;
	virtual nibble peekC1(EmuTime::param time) const = 0;
	virtual void writeA(byte value, EmuTime::param time) = 0;
	virtual void writeB(byte value, EmuTime::param time) = 0;
	virtual void writeC0(nibble value, EmuTime::param time) = 0;
	virtual void writeC1(nibble value, EmuTime::param time) = 0;

protected:
	~I8255Interface() {}
};

} // namespace openmsx

#endif
