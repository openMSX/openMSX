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

void EventDistributor::run()
{
	SDL_Event event;
	while (SDL_PollEvent(&event) == 1) {
		PRT_DEBUG("SDL event received");
		if (event.type == SDL_QUIT) {
			quit();
		} else {
			handleInEmu(event);
		}
	}
}

void EventDistributor::quit()
{
	Scheduler::instance()->stopScheduling();
}

void EventDistributor::handleInEmu(SDL_Event &event)
{
	multimap<int, EventListener*>::iterator it;
	bool cont = true;
	for (it = highMap.lower_bound(event.type);
	     (it != highMap.end()) && (it->first == event.type);
	     ++it) {
		cont &= it->second->signalEvent(event);
	}
	if (!cont) return;
	for (it = lowMap.lower_bound(event.type);
	     (it != lowMap.end()) && (it->first == event.type);
	     ++it) {
		it->second->signalEvent(event);
	}
}

void EventDistributor::registerEventListener(
	int type, EventListener *listener, int priority)
{
	multimap <int, EventListener*> &map = (priority == 0) ? highMap : lowMap;
	map.insert(pair<int, EventListener*>(type, listener));
}

void EventDistributor::unregisterEventListener(
	int type, EventListener *listener, int priority)
{
	multimap <int, EventListener*> &map = (priority == 0) ? highMap : lowMap;

	multimap<int, EventListener*>::iterator it;
	for (it = map.lower_bound(type);
	     (it != map.end()) && (it->first == type);
	     ++it) {
		if (it->second == listener) {
			map.erase(it);
			break;
		}
	}
}

void EventDistributor::QuitCommand::execute(const vector<string> &tokens)
{
	EventDistributor::instance()->quit();
}

void EventDistributor::QuitCommand::help(const vector<string> &tokens) const
{
	print("Use this command to stop the emulator");
}

void EventDistributor::update(const SettingLeafNode *setting)
{
	assert(setting == &grabInput);
	SDL_WM_GrabInput(grabInput.getValue() ? SDL_GRAB_ON : SDL_GRAB_OFF);
}
