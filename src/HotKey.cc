// $Id$

#include "openmsx.hh"
#include "HotKey.hh"
#include "ConsoleSource/CommandController.hh"
#include "EventDistributor.hh"


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


void HotKey::registerHotKey(SDLKey key, HotKeyListener *listener)
{
	PRT_DEBUG("HotKey registration for key " << ((int)key));
	map.insert(std::pair<SDLKey, HotKeyListener*>(key, listener));
	if (nbListeners == 0)
		EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
	nbListeners++;
}

void HotKey::registerHotKeyCommand(SDLKey key, const std::string &command)
{
	PRT_DEBUG("HotKey command registration for key " << ((int)key));
	HotKeyCmd *cmd = new HotKeyCmd(command);
	registerHotKey(key, cmd);
}

void HotKey::signalEvent(SDL_Event &event)
{
	SDLKey key = event.key.keysym.sym;
	PRT_DEBUG("HotKey event " << ((int)key));
	std::multimap<SDLKey, HotKeyListener*>::iterator it;
	for (it = map.lower_bound(key);
	     (it != map.end()) && (it->first == key);
	     it++) {
		it->second->signalHotKey(key);
	}
}


HotKey::HotKeyCmd::HotKeyCmd(const std::string &cmd)
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
