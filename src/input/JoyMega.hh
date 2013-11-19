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

class JoyMega
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
	virtual ~JoyMega();

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
	unsigned calcInitialState();
	void checkTime(EmuTime::param time);
	void createEvent(EmuTime::param time, unsigned press, unsigned release);
	void createEvent(EmuTime::param time, unsigned newStatus);

	// MSXEventListener
	virtual void signalEvent(const std::shared_ptr<const Event>& event,
	                         EmuTime::param time);
	// StateChangeListener
	virtual void signalStateChange(const std::shared_ptr<StateChange>& event);
	virtual void stopReplay(EmuTime::param time);

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
