// $Id$

#ifndef JOYSTICKPORT_HH
#define JOYSTICKPORT_HH

#include "openmsx.hh"
#include "Connector.hh"
#include "JoystickDevice.hh"

namespace openmsx {

class JoystickPort : public Connector
{
public:
	JoystickPort(const std::string& name);
	virtual ~JoystickPort();

	// Connector
	virtual const std::string& getDescription() const;
	virtual const std::string& getClass() const;
	virtual void plug(Pluggable* device, const EmuTime& time);
	virtual JoystickDevice& getPlugged() const;

	byte read(const EmuTime& time);
	void write(byte value, const EmuTime& time);

private:
	byte lastValue;
};

} // namespace openmsx

#endif
