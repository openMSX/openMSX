// $Id$

#include <cassert>
#include "openmsx.hh"
#include "EventDistributor.hh"
#include "EventListener.hh"

using std::pair;

namespace openmsx {

EventDistributor::EventDistributor()
{
}

EventDistributor::~EventDistributor()
{
}

EventDistributor& EventDistributor::instance()
{
	static EventDistributor oneInstance;
	return oneInstance;
}

void EventDistributor::registerEventListener(
	EventType type, EventListener& listener, int priority)
{
	ListenerMap &map = (priority == 0) ? highMap : lowMap;
	map.insert(ListenerMap::value_type(type, &listener));
}

void EventDistributor::unregisterEventListener(
	EventType type, EventListener& listener, int priority)
{
	ListenerMap &map = (priority == 0) ? highMap : lowMap;
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

void EventDistributor::distributeEvent(const Event& event)
{
	bool cont = true;
	EventType type = event.getType();
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		highMap.equal_range(type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		cont &= it->second->signalEvent(event);
	}
	if (!cont) {
		return;
	}
	bounds = lowMap.equal_range(type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		it->second->signalEvent(event);
	}
}

} // namespace openmsx
