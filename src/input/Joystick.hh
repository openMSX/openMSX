// $Id$

#ifndef JOYSTICK_HH
#define JOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "serialize_meta.hh"
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

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	Joystick(MSXEventDistributor& eventDistributor, unsigned joyNum);
	virtual ~Joystick();

	void calcInitialState();

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         const EmuTime& time);

	MSXEventDistributor& eventDistributor;

	const std::string name;
	const std::string desc;
	SDL_Joystick* const joystick;
	const unsigned joyNum;

	byte status;
};

REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Joystick, "Joystick");

} // namespace openmsx

#endif
