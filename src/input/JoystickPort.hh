// $Id$

#ifndef JOYSTICKPORT_HH
#define JOYSTICKPORT_HH

#include "Connector.hh"
#include "openmsx.hh"

namespace openmsx {

class JoystickDevice;
class PluggingController;

class JoystickPortIf
{
public:
	virtual ~JoystickPortIf() {}
	virtual byte read(EmuTime::param time) = 0;
	virtual void write(byte value, EmuTime::param time) = 0;
protected:
	JoystickPortIf() {}
};

class JoystickPort : public JoystickPortIf, public Connector
{
public:
	JoystickPort(PluggingController& pluggingController,
	             string_ref name, const std::string& description);
	virtual ~JoystickPort();

	JoystickDevice& getPluggedJoyDev() const;

	// Connector
	virtual const std::string getDescription() const;
	virtual string_ref getClass() const;
	virtual void plug(Pluggable& device, EmuTime::param time);

	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte lastValue;
	const std::string description;
};

class DummyJoystickPort : public JoystickPortIf
{
public:
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);
};

} // namespace openmsx

#endif
