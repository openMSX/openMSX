// $Id$

#include <iostream>
#include "EventDistributor.hh"
#include "EventListener.hh"
#include "openmsx.hh"
#include "config.h"
#include "Scheduler.hh"
#include "HotKey.hh"


EventDistributor::EventDistributor()
{
	// Make sure HotKey is instantiated
	HotKey::instance();	// TODO is there a better place for this?
}

EventDistributor::~EventDistributor()
{
}

EventDistributor *EventDistributor::instance()
{
	static EventDistributor* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new EventDistributor();
	return oneInstance;
}


/**
 * This is the main-loop. It waits for events and 
 *  queues them for later synchronous delivery
 * Note: this method runs in a different thread!!
 */
void EventDistributor::run()
{
	SDL_Event event;
	while (SDL_WaitEvent(&event)) {
		PRT_DEBUG("SDL event received");
	
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
	// this loop never ends
	assert(false);
}

void EventDistributor::executeUntilEmuTime(const EmuTime &time, int userdata)
{
	mutex.grab();
	bool cont = true;
	while (!highQueue.empty()) {
		std::pair<SDL_Event, EventListener*> pair = highQueue.front();
		highQueue.pop();
		cont = pair.second->signalEvent(pair.first) && cont;
	}
	while (!lowQueue.empty()) {
		std::pair<SDL_Event, EventListener*> pair = lowQueue.front();
		lowQueue.pop();
		if (cont)
			pair.second->signalEvent(pair.first);
	}
	mutex.release();
}

const std::string &EventDistributor::schedName()
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
