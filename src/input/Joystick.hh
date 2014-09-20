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
class IntegerSetting;
class TclObject;
class Interpreter;

/** Uses an SDL joystick to emulate an MSX joystick.
  */
class Joystick final
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
	~Joystick();

#ifndef SDL_JOYSTICK_DISABLED
	// Pluggable
	const std::string& getName() const override;
	string_ref getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void plugHelper2();
	byte calcState();
	bool getState(Interpreter& interp, const TclObject& dict, string_ref key,
	              int threshold);
	void createEvent(EmuTime::param time, byte newStatus);

	// MSXEventListener
	void signalEvent(const std::shared_ptr<const Event>& event,
	                 EmuTime::param time) override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) override;

	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	std::unique_ptr<StringSetting> configSetting;
	std::unique_ptr<IntegerSetting> deadSetting;
	SDL_Joystick* const joystick;
	const unsigned joyNum;
	const std::string name;
	const std::string desc;

	byte status;
	bool pin8;
#endif // SDL_JOYSTICK_DISABLED
};
SERIALIZE_CLASS_VERSION(Joystick, 2);

} // namespace openmsx

#endif
