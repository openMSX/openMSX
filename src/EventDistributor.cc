
#include "EventDistributor.hh"
#include "openmsx.hh"


EventDistributor::EventDistributor()
{
	registerListener(SDL_QUIT, this);
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


void EventDistributor::run()
{
	while (SDL_WaitEvent(&event)) {
		PRT_DEBUG("SDL event received");
		//TODO lock map
		multimap<int, EventListener*>::iterator it;
		for (it = map.find(event.type); it!=map.end(); it++) {
			((*it).second)->signalEvent(event);
		}
	}
	PRT_ERROR("Error while waiting for event");
}

void EventDistributor::registerListener(int type, EventListener *listener)
{
	//TODO lock map
	map.insert(pair<int, EventListener*>(type, listener));
}


//EventListener

void EventDistributor::signalEvent(SDL_Event &event)
{
	//TODO: more clean shutdown
	exit(1);
}

