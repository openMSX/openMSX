// $Id: 

#include "HotKey.hh"


HotKey::HotKey()
{
	nbListeners = 0;
	mapMutex = SDL_CreateMutex();
}

HotKey::~HotKey()
{
	SDL_DestroyMutex(mapMutex);
}

HotKey* HotKey::instance()
{
	if (oneInstance == NULL) {
		oneInstance = new HotKey();
	}
	return oneInstance;
}
HotKey* HotKey::oneInstance = NULL;


void HotKey::registerAsyncHotKey(SDLKey key, EventListener *listener)
{
	SDL_mutexP(mapMutex);
	map.insert(std::pair<SDLKey, EventListener*>(key, listener));
	SDL_mutexV(mapMutex);
	if (nbListeners++ == 0)
		EventDistributor::instance()->registerAsyncListener(SDL_KEYDOWN, this);
}


// EventListener
//  note: runs in different thread
void HotKey::signalEvent(SDL_Event &event)
{
	SDL_mutexP(mapMutex);
	std::multimap<SDLKey, EventListener*>::iterator it;
	for (it = map.find(event.key.keysym.sym); it!=map.end(); it++) {
		((*it).second)->signalEvent(event);
	}
	SDL_mutexV(mapMutex);
}
