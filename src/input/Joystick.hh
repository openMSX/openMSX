// $Id$

#ifndef JOYSTICK_HH
#define JOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "serialize_meta.hh"
#include <SDL.h>

namespace openmsx {

class MSXEventDistributor;
class PluggingController;

/** Uses an SDL joystick to emulate an MSX joystick.
  */
class Joystick
#ifndef SDL_JOYSTICK_DISABLED
	: public JoystickDevice, private MSXEventListener
#endif
{
public:
	/** Register all available SDL joysticks.
	  * @param eventDistributor ref to the eventDistributor.
	  * @param controller Register here.
	  */
	static void registerAll(MSXEventDistributor& eventDistributor,
	                        PluggingController& controller);

#ifndef SDL_JOYSTICK_DISABLED
	// Pluggable
	virtual const std::string& getName() const;
	virtual const std::string& getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// JoystickDevice
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

protected:
	virtual ~Joystick();

private:
	Joystick(MSXEventDistributor& eventDistributor, unsigned joyNum);

	void plugHelper2();
	void calcInitialState();

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         EmuTime::param time);

	MSXEventDistributor& eventDistributor;

	const std::string name;
	const std::string desc;
	SDL_Joystick* const joystick;
	const unsigned joyNum;

	byte status;
#endif // SDL_JOYSTICK_DISABLED
};
SERIALIZE_CLASS_VERSION(Joystick, 2);

} // namespace openmsx

#endif
