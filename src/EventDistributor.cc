// $Id$

#include "EventDistributor.hh"
#include "openmsx.hh"
#include <iostream>

#include "config.h"

EventDistributor::EventDistributor()
{
}

EventDistributor::~EventDistributor()
{
}

EventDistributor *EventDistributor::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new EventDistributor();
	}
	return oneInstance;
}
EventDistributor *EventDistributor::oneInstance = NULL;


/**
 * This is the main-loop, it waits for events and delivers them to asynchronous
 * listeners.
 * Note: this method runs in a different thread!!
 */
void EventDistributor::run()
{
	SDL_Event event;
	while (SDL_WaitEvent(&event)) {
		PRT_DEBUG("SDL event received");
		asyncMutex.grab();
		std::multimap<int, EventListener*>::iterator it;
		for (it = asyncMap.lower_bound(event.type);
		     (it != asyncMap.end()) && (it->first == event.type);
		     it++) {
			it->second->signalEvent(event);
		}
		asyncMutex.release();
	}
	PRT_ERROR("Error while waiting for event");
}

void EventDistributor::registerAsyncListener(int type, EventListener *listener)
{
	asyncMutex.grab();
	asyncMap.insert(std::pair<int, EventListener*>(type, listener));
	asyncMutex.release();
}

void EventDistributor::registerSyncListener (int type, EventListener *listener)
{
	syncMutex.grab();
	syncMap.insert(std::pair<int, EventListener*>(type, listener));
	syncMutex.release();
	registerAsyncListener(type, this); // sync events are asynchronously queued
}

// note: this method runs in a different thread!!
void EventDistributor::signalEvent(SDL_Event &event)
{
	// requeue events, they are handled later
	queueMutex.grab();
	queue.push(event);
	queueMutex.release();
}

void EventDistributor::pollSyncEvents()
{
	queueMutex.grab();
	while (!queue.empty()) {
		SDL_Event event = queue.front();
		queue.pop();
		queueMutex.release();	//temporary release queue mutex
		syncMutex.grab();
		std::multimap<int, EventListener*>::iterator it;
		for (it = syncMap.lower_bound(event.type);
		     (it != syncMap.end()) && (it->first == event.type);
		     it++) {
			it->second->signalEvent(event);
		}
		syncMutex.release();
		queueMutex.grab();	//retake queue mutex
	}
	queueMutex.release();
}

