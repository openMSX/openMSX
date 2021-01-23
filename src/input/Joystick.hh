#ifndef JOYSTICK_HH
#define JOYSTICK_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include "StringSetting.hh"
#include "serialize_meta.hh"
#include <SDL.h>

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;
class CommandController;
class PluggingController;
class GlobalSettings;
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
	                        GlobalSettings& globalSettings,
	                        PluggingController& controller);

	Joystick(MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         CommandController& commandController,
	         GlobalSettings& globalSettings,
	         SDL_Joystick* joystick);
	~Joystick()
#ifndef SDL_JOYSTICK_DISABLED
		override
#endif
		;

#ifndef SDL_JOYSTICK_DISABLED
	// Pluggable
	[[nodiscard]] const std::string& getName() const override;
	[[nodiscard]] std::string_view getDescription() const override;
	void plugHelper(Connector& connector, EmuTime::param time) override;
	void unplugHelper(EmuTime::param time) override;

	// JoystickDevice
	[[nodiscard]] byte read(EmuTime::param time) override;
	void write(byte value, EmuTime::param time) override;

	template<typename Archive>
	void serialize(Archive& ar, unsigned version);

private:
	void plugHelper2();
	[[nodiscard]] byte calcState();
	[[nodiscard]] bool getState(Interpreter& interp, const TclObject& dict, std::string_view key,
	                            int threshold);
	void createEvent(EmuTime::param time, byte newStatus);

	// MSXEventListener
	void signalMSXEvent(const std::shared_ptr<const Event>& event,
	                    EmuTime::param time) override;
	// StateChangeListener
	void signalStateChange(const std::shared_ptr<StateChange>& event) override;
	void stopReplay(EmuTime::param time) override;

private:
	MSXEventDistributor& eventDistributor;
	StateChangeDistributor& stateChangeDistributor;

	SDL_Joystick* const joystick;
	const unsigned joyNum;
	IntegerSetting& deadSetting; // must come after joyNum
	const std::string name;
	const std::string desc;
	StringSetting configSetting;

	byte status;
	bool pin8;
#endif // SDL_JOYSTICK_DISABLED
};
SERIALIZE_CLASS_VERSION(Joystick, 2);

} // namespace openmsx

#endif
