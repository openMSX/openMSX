// $Id$

#include <iostream>
#include "EventDistributor.hh"
#include "EventListener.hh"
#include "CommandController.hh"
#include "openmsx.hh"
#include "config.h"
#include "Scheduler.hh"
#include "HotKey.hh"
#include "UserEvents.hh"


EventDistributor::EventDistributor()
{
	// Make sure HotKey is instantiated
	HotKey::instance();	// TODO is there a better place for this?

	CommandController::instance()->registerCommand(&quitCommand, "quit");
}

EventDistributor::~EventDistributor()
{
	CommandController::instance()->unregisterCommand(&quitCommand, "quit");
}

EventDistributor *EventDistributor::instance()
{
	static EventDistributor* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new EventDistributor();
	return oneInstance;
}

void EventDistributor::run()
{
	Scheduler *scheduler = Scheduler::instance();
	while (scheduler->isEmulationRunning()) {
		SDL_Event event;
		if (!SDL_WaitEvent(&event)) PRT_ERROR("Error waiting for events");
		PRT_DEBUG("SDL event received");
		switch (event.type) {
		case SDL_QUIT:
			quit();
			break;
		case SDL_USEREVENT:
			UserEvents::handle(event.user);
			break;
		default:
			handleInEmu(event);
			break;
		}
	}
}

void EventDistributor::quit()
{
	// Pushing the QUIT event into the queue makes sure SDL_WaitEvent returns.
	// The handler for the QUIT event then stops the Scheduler.
	UserEvents::push(UserEvents::QUIT);
}

void EventDistributor::handleInEmu(SDL_Event &event)
{
	mutex.grab();
	std::multimap<int, EventListener*>::iterator it;
	for (it = highMap.lower_bound(event.type);
		(it != highMap.end()) && (it->first == event.type);
		it++) {
		highQueue.push(std::pair<SDL_Event, EventListener*>(event, it->second));
	}
	for (it = lowMap.lower_bound(event.type);
		(it != lowMap.end()) && (it->first == event.type);
		it++) {
		lowQueue.push(std::pair<SDL_Event, EventListener*>(event, it->second));
	}
	if (!highQueue.empty() || !lowQueue.empty()) {
		Scheduler::instance()->removeSyncPoint(this);
		Scheduler::instance()->setSyncPoint(Scheduler::ASAP, this);
	}
	mutex.release();
}

void EventDistributor::executeUntilEmuTime(const EmuTime &time, int userdata)
{
	mutex.grab();
	bool cont = true;
	while (!highQueue.empty()) {
		std::pair<SDL_Event, EventListener*> pair = highQueue.front();
		highQueue.pop();
		cont = pair.second->signalEvent(pair.first, time) && cont;
	}
	while (!lowQueue.empty()) {
		std::pair<SDL_Event, EventListener*> pair = lowQueue.front();
		lowQueue.pop();
		if (cont)
			pair.second->signalEvent(pair.first, time);
	}
	mutex.release();
}

const std::string &EventDistributor::schedName() const
{
	static const std::string name("EventDistributor");
	return name;
}

void EventDistributor::registerEventListener(int type, EventListener *listener, int priority)
{
	std::multimap <int, EventListener*> &map = (priority == 0) ? highMap : lowMap;
	
	mutex.grab();
	map.insert(std::pair<int, EventListener*>(type, listener));
	mutex.release();
}

void EventDistributor::unregisterEventListener(int type, EventListener *listener, int priority)
{
	std::multimap <int, EventListener*> &map = (priority == 0) ? highMap : lowMap;
	
	mutex.grab();
	std::multimap<int, EventListener*>::iterator it;
	for (it = map.lower_bound(type);
	     (it != map.end()) && (it->first == type);
	     it++) {
		if (it->second == listener) {
			map.erase(it);
			break;
		}
	}
	mutex.release();
}

void EventDistributor::QuitCommand::execute(
	const std::vector<std::string> &tokens, const EmuTime &time )
{
	EventDistributor::instance()->quit();
}

void EventDistributor::QuitCommand::help(
	const std::vector<std::string> &tokens) const
{
	print("Use this command to stop the emulator");
}
