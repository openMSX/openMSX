// $Id$

#include "openmsx.hh"
#include "HotKey.hh"
#include "ConsoleSource/CommandController.hh"
#include "EventDistributor.hh"


HotKey::HotKey()
{
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


void HotKey::registerHotKey(Keys::KeyCode key, HotKeyListener *listener)
{
	PRT_DEBUG("HotKey registration for key " << Keys::getName(key));
	if (map.empty())
		EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
	map.insert(std::pair<Keys::KeyCode, HotKeyListener*>(key, listener));
}

void HotKey::unregisterHotKey(Keys::KeyCode key, HotKeyListener *listener)
{
	std::multimap<Keys::KeyCode, HotKeyListener*>::iterator it;
	for (it = map.lower_bound(key);
			(it != map.end()) && (it->first == key);
			it++) {
		if (it->second == listener) {
			map.erase(it);
			break;
		}
	}
	if (map.empty())
		EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this);
}


void HotKey::registerHotKeyCommand(Keys::KeyCode key, const std::string &command)
{
	PRT_DEBUG("HotKey command registration for key " << Keys::getName(key));
	HotKeyCmd *cmd = new HotKeyCmd(command);
	registerHotKey(key, cmd);
	cmdMap.insert(std::pair<Keys::KeyCode, HotKeyCmd*>(key, cmd));
}

void HotKey::unregisterHotKeyCommand(Keys::KeyCode key, const std::string &command)
{
	std::multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
	for (it = cmdMap.lower_bound(key);
			(it != cmdMap.end()) && (it->first == key);
			it++) {
		HotKeyCmd *cmd = it->second;
		if (cmd->getCommand() == command) {
			unregisterHotKey(key, cmd);
			cmdMap.erase(it);
			break;
		}
	}
}


void HotKey::signalEvent(SDL_Event &event)
{
	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
	PRT_DEBUG("HotKey event " << Keys::getName(key));
	std::multimap<Keys::KeyCode, HotKeyListener*>::iterator it;
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
const std::string &HotKey::HotKeyCmd::getCommand()
{
	return command;
}
void HotKey::HotKeyCmd::signalHotKey(Keys::KeyCode key)
{
	CommandController::instance()->executeCommand(command);
}
