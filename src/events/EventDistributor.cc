// $Id$

#include <cassert>
#include "openmsx.hh"
#include "EventDistributor.hh"
#include "EventListener.hh"
#include "Timer.hh"
#include "RealTime.hh"
#include "Scheduler.hh"

using std::pair;

namespace openmsx {

EventDistributor::EventDistributor()
	: delaySetting("inputdelay", "EXPERIMENTAL: delay input to avoid keyskips",
	               0.03, 0.0, 10.0),
	  realTime(RealTime::instance()),
	  scheduler(Scheduler::instance())
{
	prevReal = Timer::getTime();
}

EventDistributor::~EventDistributor()
{
	for (vector<EventTime>::iterator it = toBeScheduledEvents.begin();
	     it != toBeScheduledEvents.end(); ++it) {
		delete it->event;
	}
	for (deque<Event*>::iterator it = scheduledEvents.begin();
	     it != scheduledEvents.end(); ++it) {
		delete *it;
	}
}

EventDistributor& EventDistributor::instance()
{
	static EventDistributor oneInstance;
	return oneInstance;
}

EventDistributor::ListenerMap& EventDistributor::getListeners(
	ListenerType listenerType )
{
	switch(listenerType) {
	case NATIVE:
		return nativeListeners;
	case DETACHED:
		return detachedListeners;
	case EMU:
		return emuListeners;
	default:
		assert(false);
		return detachedListeners; // any ListenerMap will do
	}
}

void EventDistributor::registerEventListener(
	EventType type, EventListener& listener, ListenerType listenerType)
{
	ListenerMap& map = getListeners(listenerType);
	map.insert(ListenerMap::value_type(type, &listener));
}

void EventDistributor::unregisterEventListener(
	EventType type, EventListener& listener, ListenerType listenerType)
{
	ListenerMap& map = getListeners(listenerType);
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		map.equal_range(type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		if (it->second == &listener) {
			map.erase(it);
			break;
		}
	}
}

void EventDistributor::distributeEvent(Event* event)
{
	assert(event);
	// TODO: Use semaphores to guard access to the maps.
	//       This is needed because this method can be called from any thread.

	// Deliver event to NATIVE listeners.
	bool cont = true;
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		nativeListeners.equal_range(event->getType());
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		cont &= it->second->signalEvent(*event);
	}
	if (!cont) {
		// Block event from other listeners.
		delete event;
		return;
	}

	// Deliver event to DETACHED listeners.
	// TODO: Is there an easier way to search a map?
	// TODO: Is it guaranteed that the Nth entry in scheduledEvents always
	//       corresponds to the Nth call to executeUntil?
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds2 =
		detachedListeners.equal_range(event->getType());
	if (bounds2.first != bounds2.second) {
		scheduledEvents.push_front(event);
		scheduler.setAsyncPoint(this, DETACHED);
		// TODO: We cannot deliver the event to an EMU listener as well,
		//       because the object will be deleted after the DETACHED
		//       listener gets it.
		//       If this is acceptable behaviour, document it.
		return;
	}

	// Deliver event to EMU listeners.
	toBeScheduledEvents.push_back(EventTime(event, Timer::getTime()));
}

void EventDistributor::sync(const EmuTime& emuTime)
{
	unsigned long long curRealTime = Timer::getTime();
	unsigned long long realDuration = curRealTime - prevReal;
	EmuDuration emuDuration = emuTime - prevEmu;

	float factor = emuDuration.toFloat() / realDuration;
	EmuDuration extraDelay = realTime.getEmuDuration(delaySetting.getValue());
	EmuTime time = prevEmu + extraDelay;
	for (vector<EventTime>::const_iterator it = toBeScheduledEvents.begin();
	     it != toBeScheduledEvents.end(); ++it) {
		assert(it->time <= curRealTime);
		scheduledEvents.push_back(it->event);
		unsigned long long offset = curRealTime - it->time;
		EmuDuration emuOffset(factor * offset);
		EmuTime schedTime = time + emuOffset;
		if (schedTime < emuTime) {
			//PRT_DEBUG("input delay too short");
			schedTime = emuTime;
		}
		scheduler.setSyncPoint(schedTime, this, EMU);
	}
	toBeScheduledEvents.clear();
	
	prevReal = curRealTime;
	prevEmu = emuTime;
}

void EventDistributor::executeUntil(const EmuTime& /*time*/, int userData)
{
	ListenerMap& listeners = getListeners(static_cast<ListenerType>(userData));
	Event* event = scheduledEvents.front();
	scheduledEvents.pop_front();
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		listeners.equal_range(event->getType());
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		it->second->signalEvent(*event);
	}
	delete event;
}

const string& EventDistributor::schedName() const
{
	static const string name = "EventDistributor";
	return name;
}

} // namespace openmsx
