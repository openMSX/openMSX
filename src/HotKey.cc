// $Id$

#include "openmsx.hh"
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


void HotKey::registerAsyncHotKey(SDLKey key, HotKeyListener *listener)
{
	PRT_DEBUG("HotKey registration for key " << ((int)key));
	SDL_mutexP(mapMutex);
	map.insert(std::pair<SDLKey, HotKeyListener*>(key, listener));
	SDL_mutexV(mapMutex);
	if (nbListeners == 0)
		EventDistributor::instance()->registerAsyncListener(SDL_KEYDOWN, this);
	nbListeners++;
}


// EventListener
//  note: runs in different thread
void HotKey::signalEvent(SDL_Event &event)
{
	SDLKey key = event.key.keysym.sym;
	PRT_DEBUG("HotKey event " << ((int)key));
	SDL_mutexP(mapMutex);
	std::multimap<SDLKey, HotKeyListener*>::iterator it;
	for (it = map.lower_bound(key);
	     (it != map.end()) && (it->first == key);
	     it++) {
		it->second->signalHotKey(key);
	}
	SDL_mutexV(mapMutex);
}
