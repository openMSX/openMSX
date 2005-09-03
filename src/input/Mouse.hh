// $Id$

#ifndef MOUSE_HH
#define MOUSE_HH

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include "Clock.hh"

namespace openmsx {

class EventDistributor;

class Mouse : public JoystickDevice, private EventListener
{
public:
	Mouse(EventDistributor& eventDistributor);
	virtual ~Mouse();

	//Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector* connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	//JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	//EventListener
	virtual void signalEvent(const Event& event);

private:
	void emulateJoystick();

	EventDistributor& eventDistributor;
	byte status;
	int faze;
	int xrel, yrel;
	int curxrel, curyrel;
	Clock<1000> lastTime; // ms
	bool mouseMode;
};

} // namespace openmsx

#endif
