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

	scheduler.setEventDistributor(this);
}

EventDistributor::~EventDistributor()
{
	scheduler.unsetEventDistributor(this);
}

EventDistributor& EventDistributor::instance()
{
	static EventDistributor oneInstance;
	return oneInstance;
}

void EventDistributor::registerEventListener(
	EventType type, EventListener& listener, ListenerType listenerType)
{
	ListenerMap &map = (listenerType == EMU) ? emuListeners : nativeListeners;
	map.insert(ListenerMap::value_type(type, &listener));
}

void EventDistributor::unregisterEventListener(
	EventType type, EventListener& listener, ListenerType listenerType)
{
	ListenerMap &map = (listenerType == EMU) ? emuListeners : nativeListeners;
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
	bool cont = true;
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		nativeListeners.equal_range(event->getType());
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		cont &= it->second->signalEvent(*event);
	}
	if (!cont) {
		delete event;
	} else {
		toBeScheduledEvents.push_back(
			EventTime(event, Timer::getTime()));
	}
}

void EventDistributor::sync(const EmuTime& emuTime)
{
	unsigned curRealTime = Timer::getTime();
	unsigned realDuration = curRealTime - prevReal;
	EmuDuration emuDuration = emuTime - prevEmu;

	float factor = emuDuration.toFloat() / realDuration;
	EmuDuration extraDelay = realTime.getEmuDuration(delaySetting.getValue());
	EmuTime time = prevEmu + extraDelay;
	for (vector<EventTime>::const_iterator it = toBeScheduledEvents.begin();
	     it != toBeScheduledEvents.end(); ++it) {
		assert(it->time <= curRealTime);
		scheduledEvents.push_back(it->event);
		unsigned offset = curRealTime - it->time;
		EmuDuration emuOffset(factor * offset);
		EmuTime schedTime = time + emuOffset;
		if (schedTime < emuTime) {
			//PRT_DEBUG("input delay too short");
			schedTime = emuTime;
		}
		scheduler.setSyncPoint(schedTime, this);
	}
	toBeScheduledEvents.clear();
	
	prevReal = curRealTime;
	prevEmu = emuTime;
}

void EventDistributor::executeUntil(const EmuTime& time, int userData) throw()
{
	Event* event = scheduledEvents.front();
	scheduledEvents.pop_front();

	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		emuListeners.equal_range(event->getType());
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
