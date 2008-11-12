// $Id$

#ifndef YM2413INTERFACE_HH
#define YM2413INTERFACE_HH

#include "EmuTime.hh"
#include "openmsx.hh"

namespace openmsx {

class YM2413Interface
{
public:
	virtual ~YM2413Interface() {}
	virtual void reset(EmuTime::param time) = 0;
	virtual void writeReg(byte reg, byte value, EmuTime::param time) = 0;
};

} // namespace openmsx

#endif
