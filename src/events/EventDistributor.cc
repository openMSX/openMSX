// $Id$

#include <cassert>
#include <iostream>
#include "EventDistributor.hh"
#include "EventListener.hh"
#include "CommandController.hh"
#include "openmsx.hh"
#include "config.h"
#include "Scheduler.hh"
#include "HotKey.hh"


namespace openmsx {

EventDistributor::EventDistributor()
	: grabInput("grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false)
{
	// Make sure HotKey is instantiated
	HotKey::instance();	// TODO is there a better place for this?
	grabInput.addListener(this);
	CommandController::instance()->registerCommand(&quitCommand, "quit");
}

EventDistributor::~EventDistributor()
{
	CommandController::instance()->unregisterCommand(&quitCommand, "quit");
	grabInput.removeListener(this);
}

EventDistributor *EventDistributor::instance()
{
	static EventDistributor* oneInstance = NULL;
	if (oneInstance == NULL) {
		oneInstance = new EventDistributor();
	}
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
		if (event.type == SDL_QUIT) {
			quit();
		} else {
			handle(event);
		}
	}
}

void EventDistributor::notify()
{
	static SDL_Event event;
	event.type = SDL_USEREVENT;
	SDL_PushEvent(&event);
}

void EventDistributor::quit()
{
	Scheduler::instance()->stopScheduling();
}

void EventDistributor::handle(SDL_Event &event)
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
	int type, EventListener *listener, int priority)
{
	ListenerMap &map = (priority == 0) ? highMap : lowMap;
	map.insert(pair<int, EventListener*>(type, listener));
}

void EventDistributor::unregisterEventListener(
	int type, EventListener *listener, int priority)
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

string EventDistributor::QuitCommand::execute(const vector<string> &tokens)
	throw()
{
	EventDistributor::instance()->quit();
	return "";
}

string EventDistributor::QuitCommand::help(const vector<string> &tokens) const
	throw()
{
	return "Use this command to stop the emulator\n";
}

void EventDistributor::update(const SettingLeafNode *setting)
{
	assert(setting == &grabInput);
	SDL_WM_GrabInput(grabInput.getValue() ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

} // namespace openmsx
