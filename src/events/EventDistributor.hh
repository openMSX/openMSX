// $Id$

#ifndef __EVENTDISTRIBUTOR_HH__
#define __EVENTDISTRIBUTOR_HH__

#include <map>
#include <vector>
#include <deque>
#include "Event.hh"
#include "EmuTime.hh"
#include "Schedulable.hh"
#include "FloatSetting.hh"

using std::multimap;
using std::vector;
using std::deque;

namespace openmsx {

class EventListener;
class RealTime;
class Scheduler;

class EventDistributor : private Schedulable
{
public:
	static EventDistributor& instance();

	/**
	 * Use this method to (un)register a given class to receive certain
	 * events.
	 * @param type The type of the events you want to receive
	 * @param listener Object that will be notified when the events arrives
	 * @param listenerType Is the event ment for the MSX or just to control
	 *        openMSX (console for example). Native events are delivered
	 *        immediatly, emu events can get queued. Native event listeners
	 *        can block events for emu event listeners.
	 * The delivery of the event is done by the 'main-emulation-thread',
	 * so there is no need for extra synchronization.
	 */
	enum ListenerType { EMU, NATIVE };
	void   registerEventListener(EventType type, EventListener& listener,
	                             ListenerType listenerType = EMU);
	void unregisterEventListener(EventType type, EventListener& listener,
	                             ListenerType listenerType = EMU);

	void distributeEvent(Event* event);
	void sync(const EmuTime& emuTime);
	
private:
	EventDistributor();
	virtual ~EventDistributor();

	// Schedulable
	virtual void executeUntil(const EmuTime& time, int userData);
	virtual const string& schedName() const;

	typedef multimap<EventType, EventListener*> ListenerMap;
	ListenerMap nativeListeners;
	ListenerMap emuListeners;

	struct EventTime {
		EventTime(Event* event_, unsigned long long time_)
			: event(event_), time(time_) {}
		Event* event;
		unsigned long long time;
	};
	vector<EventTime> toBeScheduledEvents;
	deque<Event*> scheduledEvents;

	EmuTime prevEmu;
	unsigned long long prevReal;
	FloatSetting delaySetting;

	RealTime& realTime;
	Scheduler& scheduler;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__
