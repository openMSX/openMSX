// $Id$

#ifndef __JOYSTICKPORT_HH__
#define __JOYSTICKPORT_HH__

#include "openmsx.hh"
#include "Connector.hh"


namespace openmsx {

class JoystickPort : public Connector {
public:
	JoystickPort(const string& name);
	virtual ~JoystickPort();

	// Connector
	virtual const string& getDescription() const;
	virtual const string& getClass() const;
	virtual void plug(Pluggable* device, const EmuTime& time)
		throw(PlugException);

	byte read(const EmuTime& time);
	void write(byte value, const EmuTime& time);

private:
	byte lastValue;
};

} // namespace openmsx

#endif // __JOYSTICKPORT_HH__
