// $Id$

#ifndef MOUSE_HH
#define MOUSE_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "Clock.hh"

namespace openmsx {

class MSXEventDistributor;

class Mouse : public JoystickDevice, private MSXEventListener
{
public:
	explicit Mouse(MSXEventDistributor& eventDistributor);
	virtual ~Mouse();

	//Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	//JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

	//MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event);

private:
	void emulateJoystick();

	MSXEventDistributor& eventDistributor;
	byte status;
	int faze;
	int xrel, yrel;
	int curxrel, curyrel;
	Clock<1000> lastTime; // ms
	bool mouseMode;
};

} // namespace openmsx

#endif
