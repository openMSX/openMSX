#ifndef JOYSTICK_HH
#define JOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "serialize_meta.hh"
#include <memory>
#include <SDL.h>

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;
class CommandController;
class PluggingController;
class StringSetting;
class TclObject;

/** Uses an SDL joystick to emulate an MSX joystick.
  */
class Joystick
#ifndef SDL_JOYSTICK_DISABLED
	: public JoystickDevice, private MSXEventListener, private StateChangeListener
#endif
{
public:
	/** Register all available SDL joysticks.
	  */
	static void registerAll(MSXEventDistributor& eventDistributor,
	                        StateChangeDistributor& stateChangeDistributor,
	                        CommandController& commandController,
	                        PluggingController& controller);

	Joystick(MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         CommandController& commandController,
	         SDL_Joystick* joystick);
	virtual ~Joystick();

#ifndef SDL_JOYSTICK_DISABLED
	// Pluggable
	virtual const std::string& getName() const;
	virtual string_ref getDescription() const;
	virtual void plugHelper(Connector& connector, EmuTime::param time);
	virtual void unplugHelper(EmuTime::param time);

	// JoystickDevice
	virtual byte read(EmuTime::param time);
	virtual void write(byte value, EmuTime::param time);

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void plugHelper2();
	byte calcState();
	bool getState(const TclObject& dict, string_ref key);
	void createEvent(EmuTime::param time, byte newStatus);

	// MSXEventListener
	virtual void signalEvent(const std::shared_ptr<const Event>& event,
	                         EmuTime::param time);
	// StateChangeListener
	virtual void signalStateChange(const std::shared_ptr<StateChange>& event);
	virtual void stopReplay(EmuTime::param time);

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	std::unique_ptr<StringSetting> configSetting;
	SDL_Joystick* const joystick;
	const unsigned joyNum;
	const std::string name;
	const std::string desc;

	byte status;
#endif // SDL_JOYSTICK_DISABLED
};
SERIALIZE_CLASS_VERSION(Joystick, 2);

} // namespace openmsx

#endif
