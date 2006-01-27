// $Id$

#ifndef JOYTAPPORT_HH
#define JOYTAPPORT_HH

#include "Connector.hh"
#include "JoystickDevice.hh"
#include "openmsx.hh"

namespace openmsx {

class PluggingController;

class JoyTapPort : public Connector
{
public:
	JoyTapPort(PluggingController& pluggingController,
	             const std::string& name);
	virtual ~JoyTapPort();

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;
	virtual void plug(Pluggable& device, const EmuTime& time);
	virtual JoystickDevice& getPlugged() const;

	byte read(const EmuTime& time);
	void write(byte value, const EmuTime& time);

private:
	PluggingController& pluggingController;
	byte lastValue;
};

} // namespace openmsx

#endif
