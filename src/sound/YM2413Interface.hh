// $Id$

#ifndef YM2413INTERFACE_HH
#define YM2413INTERFACE_HH

#include "openmsx.hh"

namespace openmsx {

class EmuTime;

class YM2413Interface
{
public:
	virtual ~YM2413Interface() {}
	virtual void reset(const EmuTime& time) = 0;
	virtual void writeReg(byte reg, byte value, const EmuTime& time) = 0;
};

} // namespace openmsx

#endif
