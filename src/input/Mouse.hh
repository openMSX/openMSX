// $Id$

#ifndef MOUSE_HH
#define MOUSE_HH

#include "JoystickDevice.hh"
#include "UserInputEventListener.hh"
#include "Clock.hh"

namespace openmsx {

class UserInputEventDistributor;

class Mouse : public JoystickDevice, private UserInputEventListener
{
public:
	explicit Mouse(UserInputEventDistributor& eventDistributor);
	virtual ~Mouse();

	//Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	//JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	//UserInputEventListener
	virtual void signalEvent(const Event& event);

private:
	void emulateJoystick();

	UserInputEventDistributor& eventDistributor;
	byte status;
	int faze;
	int xrel, yrel;
	int curxrel, curyrel;
	Clock<1000> lastTime; // ms
	bool mouseMode;
};

} // namespace openmsx

#endif
