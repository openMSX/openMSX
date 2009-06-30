// $Id$

#ifndef ALARMEVENT_HH
#define ALARMEVENT_HH

#include "Alarm.hh"
#include "Event.hh"

namespace openmsx {

class EventDistributor;
class EventListener;

/** Convenience wrapper around the Alarm class.
  * An expired alarm callback runs in the timer thread. Very often you instead
  * want a callback in the main thread. This class takes care of that, it
  * will make sure the signalEvent() method of the given EventListener gets
  * called, in the main thread, when the alarm expires.
  */
class AlarmEvent : public Alarm
{
public:
	AlarmEvent(EventDistributor& distributor, EventListener& listener,
	           EventType type);
	~AlarmEvent();

private:
	// Alarm
	virtual bool alarm();

	EventDistributor& distributor;
	EventListener& listener;
	const EventType type;
};

} // namespace openmsx

#endif
