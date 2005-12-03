// $Id$

#ifndef MC6850_HH
#define MC6850_HH

#include "MSXDevice.hh"

namespace openmsx {

class MC6850 : public MSXDevice
{
public:
	MC6850(MSXMotherBoard& motherBoard, const XMLElement& config,
	       const EmuTime& time);

	virtual void reset(const EmuTime& time);
	virtual byte readIO(word port, const EmuTime& time);
	virtual byte peekIO(word port, const EmuTime& time) const;
	virtual void writeIO(word port, byte value, const EmuTime& time);
};

} // namespace openmsx

#endif
