// $Id$

#include <cassert>
#include "openmsx.hh"
#include "EventDistributor.hh"
#include "EventListener.hh"

namespace openmsx {

EventDistributor::EventDistributor()
	: grabInput("grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false)
{
	grabInput.addListener(this);
}

EventDistributor::~EventDistributor()
{
	grabInput.removeListener(this);
}

EventDistributor& EventDistributor::instance()
{
	static EventDistributor oneInstance;
	return oneInstance;
}

void EventDistributor::wait()
{
	if (SDL_WaitEvent(NULL)) {
		poll();
	}
}

void EventDistributor::poll()
{
	SDL_Event event;
	while (SDL_PollEvent(&event) == 1) {
		PRT_DEBUG("SDL event received");
		handle(event);
	}
}

void EventDistributor::notify()
{
	static SDL_Event event;
	event.type = SDL_USEREVENT;
	SDL_PushEvent(&event);
}

void EventDistributor::handle(SDL_Event& event)
{
	bool cont = true;
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds;
	bounds = highMap.equal_range(event.type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		cont &= it->second->signalEvent(event);
	}
	if (!cont) {
		return;
	}
	bounds = lowMap.equal_range(event.type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		it->second->signalEvent(event);
	}
}

void EventDistributor::registerEventListener(
	int type, EventListener* listener, int priority)
{
	ListenerMap &map = (priority == 0) ? highMap : lowMap;
	map.insert(pair<int, EventListener*>(type, listener));
}

void EventDistributor::unregisterEventListener(
	int type, EventListener* listener, int priority)
{
	ListenerMap &map = (priority == 0) ? highMap : lowMap;
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		map.equal_range(type);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		if (it->second == listener) {
			map.erase(it);
			break;
		}
	}
}

void EventDistributor::update(const SettingLeafNode *setting) throw()
{
	assert(setting == &grabInput);
	SDL_WM_GrabInput(grabInput.getValue() ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

} // namespace openmsx
