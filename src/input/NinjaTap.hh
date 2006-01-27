// $Id$

#ifndef NINJATAP_HH
#define NINJATAP_HH

#include "JoyTap.hh"
#include "openmsx.hh"

namespace openmsx {

class PluggingController;
class CommandController;
class NinjaTapPort;

class NinjaTap : public JoyTap
{
public:
	NinjaTap(CommandController& commandController,
		     PluggingController& pluggingController_,
		     const std::string& name_
	             );
	virtual ~NinjaTap();

	//Pluggable
	virtual const std::string& getDescription() const; 

	//JoystickDevice
	byte read(const EmuTime& time);
	void write(byte value, const EmuTime& time);

private:
	byte lastValue;
};

} // namespace openmsx

#endif
