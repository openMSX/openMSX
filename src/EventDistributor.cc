
#include "EventDistributor.hh"
#include "openmsx.hh"

#include <iostream>

EventDistributor::EventDistributor()
{
	mut = SDL_CreateMutex();
	registerListener(SDL_QUIT, this);
}

EventDistributor::~EventDistributor()
{
	SDL_DestroyMutex(mut);
}

EventDistributor *EventDistributor::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new EventDistributor();
	}
	return oneInstance;
}
EventDistributor *EventDistributor::oneInstance = NULL;


void EventDistributor::run()
{
	while (SDL_WaitEvent(&event)) {
		SDL_mutexP(mut);
		PRT_DEBUG("SDL event received");
		std::multimap<int, EventListener*>::iterator it;
		for (it = map.find(event.type); it!=map.end(); it++) {
			((*it).second)->signalEvent(event);
		}
		SDL_mutexV(mut);
	}
	PRT_ERROR("Error while waiting for event");
}

void EventDistributor::registerListener(int type, EventListener *listener)
{
	SDL_mutexP(mut);
	map.insert(std::pair<int, EventListener*>(type, listener));
	SDL_mutexV(mut);
}


//EventListener
// note: this method runs in a different thread!!
void EventDistributor::signalEvent(SDL_Event &event)
{
	//TODO: more clean shutdown
	exit(1);
}

