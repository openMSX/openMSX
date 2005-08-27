// $Id$

#ifndef USERINPUTEVENTDISTRIBUTOR_HH
#define USERINPUTEVENTDISTRIBUTOR_HH

#include "EventListener.hh"
#include "Event.hh"
#include <vector>

namespace openmsx {

class UserInputEventListener;
class EventDistributor;

/**
 * Layered user input event distribution system: high priority listeners
 * can withhold events from lower priority listeners.
 * TODO: This has some parallels with the Display layer system.
 *       Should they be merged?
 * TODO: Withholding a keydown is rather simple to cope with: just ignore the
 *       keyup when it comes eventually.
 *       Withholding a keyup is not simple to cope with: the key will keep
 *       hanging.
 *       Should this distinction be part of the design?
 *       It explains why HotKey does not cause trouble for lower layers
 *       while Console does.
 * TODO: Actually, why does Console withhold keyup from the MSX?
 */
class UserInputEventDistributor : private EventListener
{
public:
	/**
	 * This distributor has specifically allocated slots for its listeners.
	 * This is a simple solution that works,
	 * until we know the definitive design.
	 * The top prioriy listener is placed first.
	 */
	enum ListenerSlot {
		HOTKEY,
		CONSOLE,
		MSX,
		NUM_SLOTS, // not an actual slot
	};

	UserInputEventDistributor(EventDistributor& eventDistributor);
	virtual ~UserInputEventDistributor();

	/**
	 * Registers a given object to receive certain events.
	 * @param listener Listener that will be notified when an event arrives.
	 */
	void registerEventListener(
		ListenerSlot slot, UserInputEventListener& listener );

	/**
	 * Unregisters a previously registered event listener.
	 * @param listener Listener to unregister.
	 */
	void unregisterEventListener(
		ListenerSlot slot, UserInputEventListener& listener );

private:
	virtual void signalEvent(const Event& event);

	typedef std::vector<UserInputEventListener*> Listeners;
	Listeners listeners[NUM_SLOTS];
	EventDistributor& eventDistributor;
};

} // namespace openmsx

#endif // USERINPUTEVENTDISTRIBUTOR_HH
