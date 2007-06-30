// $Id$

#ifndef JOYSTICK_HH
#define JOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include <SDL.h> // TODO move this

namespace openmsx {

class MSXEventDistributor;
class PluggingController;

/** Uses an SDL joystick to emulate an MSX joystick.
  */
class Joystick : public JoystickDevice, private MSXEventListener
{
public:
	/** Register all available SDL joysticks.
	  * @param eventDistributor ref to the eventDistributor.
	  * @param controller Register here.
	  */
	static void registerAll(MSXEventDistributor& eventDistributor,
	                        PluggingController& controller);

	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, const EmuTime& time);
	virtual void unplugHelper(const EmuTime& time);

	// JoystickDevice
	virtual byte read(const EmuTime& time);
	virtual void write(byte value, const EmuTime& time);

private:
	Joystick(MSXEventDistributor& eventDistributor, unsigned joyNum);
	virtual ~Joystick();

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	static const int THRESHOLD = 32768/10;

	MSXEventDistributor& eventDistributor;

	const unsigned joyNum;
	const std::string name;
	const std::string desc;
	SDL_Joystick* const joystick;

	byte status;
};

} // namespace openmsx

#endif
