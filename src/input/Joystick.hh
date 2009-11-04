// $Id$

#ifndef JOYSTICK_HH
#define JOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"
#include <SDL.h>

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;
class PluggingController;

/** Uses an SDL joystick to emulate an MSX joystick.
  */
class Joystick
#ifndef SDL_JOYSTICK_DISABLED
	: public JoystickDevice, private MSXEventListener, private StateChangeListener
#endif
{
public:
	/** Register all available SDL joysticks.
	  * @param eventDistributor ref to the MSXEventDistributor.
	  * @param stateChangeDistributor ref to the StateChangeDistributor.
	  * @param controller Register here.
	  */
	static void registerAll(MSXEventDistributor& eventDistributor,
	                        StateChangeDistributor& stateChangeDistributor,
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
	Joystick(MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         unsigned joyNum);

	void plugHelper2();
	void calcInitialState();
	void createEvent(EmuTime::param time, int joyNum, byte press, byte release);

	// MSXEventListener
	virtual void signalEvent(shared_ptr<const Event> event,
	                         EmuTime::param time);
	// StateChangeListener
	virtual void signalStateChange(shared_ptr<const StateChange> event);

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

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
