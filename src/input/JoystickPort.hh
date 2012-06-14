// $Id$

#ifndef JOYSTICKPORT_HH
#define JOYSTICKPORT_HH

#include "Connector.hh"
#include "openmsx.hh"

namespace openmsx {

class JoystickDevice;
class PluggingController;

class JoystickPort : public Connector
{
public:
	JoystickPort(PluggingController& pluggingController,
	             const std::string& name, const std::string& description);
	virtual ~JoystickPort();

	JoystickDevice& getPluggedJoyDev() const;

	// Connector
	virtual const std::string getDescription() const;
	virtual string_ref getClass() const;
	virtual void plug(Pluggable& device, EmuTime::param time);

	byte read(EmuTime::param time);
	void write(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	byte lastValue;
	const std::string description;
};

} // namespace openmsx

#endif
