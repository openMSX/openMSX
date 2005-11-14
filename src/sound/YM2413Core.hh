// $Id$

#ifndef YM2413CORE_HH
#define YM2413CORE_HH

#include "openmsx.hh"

namespace openmsx {

class EmuTime;

class YM2413Core
{
public:
	virtual ~YM2413Core();
	virtual void reset(const EmuTime& time) = 0;
	virtual void writeReg(byte reg, byte value, const EmuTime& time) = 0;
};

} // namespace openmsx

#endif
