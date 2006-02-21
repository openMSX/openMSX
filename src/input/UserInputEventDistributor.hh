// $Id$

#ifndef USERINPUTEVENTDISTRIBUTOR_HH
#define USERINPUTEVENTDISTRIBUTOR_HH

#include "EventListener.hh"
#include "Schedulable.hh"
#include "EmuTime.hh"
#include <vector>
#include <deque>
#include <memory>

namespace openmsx {

class UserInputEventListener;
class Scheduler;
class CommandController;
class EventDistributor;
class FloatSetting;

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
class UserInputEventDistributor : private EventListener, private Schedulable
{
public:
	UserInputEventDistributor(Scheduler& scheduler,
	                          CommandController& commandController,
	                          EventDistributor& eventDistributor);
	virtual ~UserInputEventDistributor();

	/**
	 * Registers a given object to receive certain events.
	 * @param listener Listener that will be notified when an event arrives.
	 */
	void registerEventListener(UserInputEventListener& listener);

	/**
	 * Unregisters a previously registered event listener.
	 * @param listener Listener to unregister.
	 */
	void unregisterEventListener(UserInputEventListener& listener);

	void sync(const EmuTime& time);

private:
	void queueEvent(Event* event);

	// EventListener
	virtual void signalEvent(const Event& event);

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const std::string& schedName() const;

	typedef std::vector<UserInputEventListener*> Listeners;
	Listeners listeners;
	EventDistributor& eventDistributor;

	bool console;

	struct EventTime {
		EventTime(Event* event_, unsigned long long time_)
			: event(event_), time(time_) {}
		Event* event;
		unsigned long long time;
	};
	std::vector<EventTime> toBeScheduledEvents;
	std::deque<Event*> scheduledEvents;

	EmuTime prevEmu;
	unsigned long long prevReal;
	std::auto_ptr<FloatSetting> delaySetting;
};

} // namespace openmsx

#endif // USERINPUTEVENTDISTRIBUTOR_HH
