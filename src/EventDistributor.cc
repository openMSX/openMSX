// $Id$

#include "EventDistributor.hh"
#include "openmsx.hh"

#include <iostream>

EventDistributor::EventDistributor()
{
	asyncMutex = SDL_CreateMutex();
	syncMutex  = SDL_CreateMutex();
	queueMutex = SDL_CreateMutex();
	registerAsyncListener(SDL_QUIT, this);
}

EventDistributor::~EventDistributor()
{
	SDL_DestroyMutex(queueMutex);
	SDL_DestroyMutex(syncMutex);
	SDL_DestroyMutex(asyncMutex);
}

EventDistributor *EventDistributor::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new EventDistributor();
	}
	return oneInstance;
}
EventDistributor *EventDistributor::oneInstance = NULL;


// note: this method runs in a different thread!!
void EventDistributor::run()
{
	SDL_Event event;
	while (SDL_WaitEvent(&event)) {
		PRT_DEBUG("SDL event received");
		SDL_mutexP(asyncMutex);
		std::multimap<int, EventListener*>::iterator it;
		for (it = asyncMap.find(event.type); it!=asyncMap.end(); it++) {
			((*it).second)->signalEvent(event);
		}
		SDL_mutexV(asyncMutex);
	}
	PRT_ERROR("Error while waiting for event");
}

void EventDistributor::registerAsyncListener(int type, EventListener *listener)
{
	SDL_mutexP(asyncMutex);
	asyncMap.insert(std::pair<int, EventListener*>(type, listener));
	SDL_mutexV(asyncMutex);
}

void EventDistributor::registerSyncListener (int type, EventListener *listener)
{
	SDL_mutexP(syncMutex);
	syncMap.insert(std::pair<int, EventListener*>(type, listener));
	SDL_mutexV(syncMutex);
	registerAsyncListener(type, this); // sync events are asynchronously queued
}

// note: this method runs in a different thread!!
void EventDistributor::signalEvent(SDL_Event &event)
{
	// TODO handling of SDL_QUIT should be done in a different class
	if (event.type == SDL_QUIT) {
		//TODO: more clean shutdown
		exit(1);
	}
	// all (other) events are later handled synchronously, now just queue them
	SDL_mutexP(queueMutex);
	queue.push(event);
	SDL_mutexV(queueMutex);
}

void EventDistributor::pollSyncEvents()
{
	SDL_mutexP(queueMutex);
	while (!queue.empty()) {
		SDL_Event event = queue.front();
		queue.pop();
		SDL_mutexV(queueMutex);	//temporary release queue mutex
		SDL_mutexP(syncMutex);
		std::multimap<int, EventListener*>::iterator it;
		for (it = syncMap.find(event.type); it!=syncMap.end(); it++) {
			((*it).second)->signalEvent(event);
		}
		SDL_mutexV(syncMutex);
		SDL_mutexP(queueMutex);	//retake queue mutex
	}
	SDL_mutexV(queueMutex);
}

