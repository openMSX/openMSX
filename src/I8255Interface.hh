// $Id$

#ifndef I8255INTERFACE_HH
#define I8255INTERFACE_HH

#include "openmsx.hh"

namespace openmsx {

class EmuTime;

class I8255Interface
{
public:
	virtual byte readA(const EmuTime& time) = 0;
	virtual byte readB(const EmuTime& time) = 0;
	virtual nibble readC0(const EmuTime& time) = 0;
	virtual nibble readC1(const EmuTime& time) = 0;
	virtual byte peekA(const EmuTime& time) const = 0;
	virtual byte peekB(const EmuTime& time) const = 0;
	virtual nibble peekC0(const EmuTime& time) const = 0;
	virtual nibble peekC1(const EmuTime& time) const = 0;
	virtual void writeA(byte value, const EmuTime& time) = 0;
	virtual void writeB(byte value, const EmuTime& time) = 0;
	virtual void writeC0(nibble value, const EmuTime& time) = 0;
	virtual void writeC1(nibble value, const EmuTime& time) = 0;

protected:
	virtual ~I8255Interface() {}
};

} // namespace openmsx

#endif
