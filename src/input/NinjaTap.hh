// $Id$

#ifndef NINJATAP_HH
#define NINJATAP_HH

#include "JoyTap.hh"

namespace openmsx {

class NinjaTap : public JoyTap
{
public:
	NinjaTap(PluggingController& pluggingController,
	         const std::string& name);

	//Pluggable
	virtual const std::string& getDescription() const; 

	//JoystickDevice
	byte read(const EmuTime& time);
	void write(byte value, const EmuTime& time);
};

} // namespace openmsx

#endif
