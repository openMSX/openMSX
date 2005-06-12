// $Id$

#ifndef MC6850_HH
#define MC6850_HH

#include "MSXDevice.hh"

namespace openmsx {

class MC6850 : public MSXDevice
{
public:
	MC6850(const XMLElement& config, const EmuTime& time);
	virtual ~MC6850();

	virtual void reset(const EmuTime& time);
	virtual byte readIO(byte port, const EmuTime& time);
	virtual byte peekIO(byte port, const EmuTime& time) const;
	virtual void writeIO(byte port, byte value, const EmuTime& time);
};

} // namespace openmsx

#endif
