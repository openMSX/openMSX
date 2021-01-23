#ifndef JOYMEGA_HH
#define JOYMEGA_HH

#include "JoystickDevice.hh"
#include "MSXEventListener.hh"
#include "StateChangeListener.hh"
#include <SDL.h>

namespace openmsx {

class MSXEventDistributor;
class StateChangeDistributor;
class PluggingController;

class JoyMega final
#ifndef SDL_JOYSTICK_DISABLED
	: public JoystickDevice, private MSXEventListener, private StateChangeListener
#endif
{
public:
	static void registerAll(MSXEventDistributor& eventDistributor,
	                        StateChangeDistributor& stateChangeDistributor,
	                        PluggingController& controller);

	JoyMega(MSXEventDistributor& eventDistributor,
	         StateChangeDistributor& stateChangeDistributor,
	         SDL_Joystick* joystick);
	~JoyMega()
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
	[[nodiscard]] unsigned calcInitialState();
	void checkTime(EmuTime::param time);
	void createEvent(EmuTime::param time, unsigned press, unsigned release);
	void createEvent(EmuTime::param time, unsigned newStatus);

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
	const std::string name;
	const std::string desc;

	EmuTime lastTime;
	unsigned status;
	byte cycle; // 0-7
	byte cycleMask; // 1 or 7
#endif // SDL_JOYSTICK_DISABLED
};

} // namespace openmsx

#endif
