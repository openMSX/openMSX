// $Id$

#ifndef JOYSTICK_HH
#define JOYSTICK_HH

#include "JoystickDevice.hh"
#include "EventListener.hh"
#include <SDL.h> // TODO move this

namespace openmsx {

class EventDistributor;
class PluggingController;

/** Uses an SDL joystick to emulate an MSX joystick.
  */
class Joystick : public JoystickDevice, private EventListener
{
public:
	/** Register all available SDL joysticks.
	  * @param controller Register here.
	  */
	static void registerAll(EventDistributor& eventDistributor,
	                        PluggingController& controller);

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
	Joystick(EventDistributor& eventDistributor, unsigned joyNum);
	virtual ~Joystick();

	static const int THRESHOLD = 32768/10;

	EventDistributor& eventDistributor;

	std::string name;
	std::string desc;

	unsigned joyNum;
	SDL_Joystick* joystick;
	byte status;
};

} // namespace openmsx

#endif
