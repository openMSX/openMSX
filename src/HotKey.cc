// $Id$

#include "openmsx.hh"
#include "HotKey.hh"
#include "ConsoleSource/CommandController.hh"


HotKey::HotKey()
{
	nbListeners = 0;
}

HotKey::~HotKey()
{
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
	mapMutex.grab();
	map.insert(std::pair<SDLKey, HotKeyListener*>(key, listener));
	mapMutex.release();
	if (nbListeners == 0)
		EventDistributor::instance()->registerAsyncListener(SDL_KEYDOWN, this);
	nbListeners++;
}

void HotKey::registerHotKeyCommand(SDLKey key, std::string command)
{
	PRT_DEBUG("HotKey command registration for key " << ((int)key));
	HotKeyCmd *cmd = new HotKeyCmd(command);
	registerAsyncHotKey(key, cmd);
}

// EventListener
//  note: runs in different thread
void HotKey::signalEvent(SDL_Event &event)
{
	SDLKey key = event.key.keysym.sym;
	PRT_DEBUG("HotKey event " << ((int)key));
	mapMutex.grab();
	std::multimap<SDLKey, HotKeyListener*>::iterator it;
	for (it = map.lower_bound(key);
	     (it != map.end()) && (it->first == key);
	     it++) {
		it->second->signalHotKey(key);
	}
	mapMutex.release();
}


HotKey::HotKeyCmd::HotKeyCmd(std::string cmd)
{
	command = cmd;
}
HotKey::HotKeyCmd::~HotKeyCmd()
{
}
void HotKey::HotKeyCmd::signalHotKey(SDLKey key)
{
	CommandController::instance()->executeCommand(command);
}
