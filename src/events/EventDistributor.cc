// $Id$

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
	grabInput.registerListener(this);
	CommandController::instance()->registerCommand(&quitCommand, "quit");
}

EventDistributor::~EventDistributor()
{
	CommandController::instance()->unregisterCommand(&quitCommand, "quit");
	grabInput.unregisterListener(this);
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
	std::multimap<int, EventListener*>::iterator it;
	bool cont = true;
	for (it = highMap.lower_bound(event.type);
		(it != highMap.end()) && (it->first == event.type);
		it++
	) {
		cont &= it->second->signalEvent(event);
	}
	if (!cont) return;
	for (it = lowMap.lower_bound(event.type);
		(it != lowMap.end()) && (it->first == event.type);
		it++
	) {
		it->second->signalEvent(event);
	}
}

void EventDistributor::registerEventListener(int type, EventListener *listener, int priority)
{
	std::multimap <int, EventListener*> &map = (priority == 0) ? highMap : lowMap;
	map.insert(std::pair<int, EventListener*>(type, listener));
}

void EventDistributor::unregisterEventListener(int type, EventListener *listener, int priority)
{
	std::multimap <int, EventListener*> &map = (priority == 0) ? highMap : lowMap;

	std::multimap<int, EventListener*>::iterator it;
	for (it = map.lower_bound(type);
	     (it != map.end()) && (it->first == type);
	     it++) {
		if (it->second == listener) {
			map.erase(it);
			break;
		}
	}
}

void EventDistributor::QuitCommand::execute(
	const std::vector<std::string> &tokens )
{
	EventDistributor::instance()->quit();
}

void EventDistributor::QuitCommand::help(
	const std::vector<std::string> &tokens) const
{
	print("Use this command to stop the emulator");
}

void EventDistributor::notify(Setting *setting)
{
	if (grabInput.getValue()) {
		SDL_WM_GrabInput(SDL_GRAB_ON);
	} else {
		SDL_WM_GrabInput(SDL_GRAB_OFF);
	}
}
