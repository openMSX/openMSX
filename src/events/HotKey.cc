// $Id$

#include "openmsx.hh"
#include "HotKey.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"


HotKey::HotKey()
{
	CommandController::instance()->registerCommand(bindCmd,   "bind");
	CommandController::instance()->registerCommand(unbindCmd, "unbind");
}

HotKey::~HotKey()
{
	CommandController::instance()->unregisterCommand("bind");
	CommandController::instance()->unregisterCommand("unbind");
}

HotKey* HotKey::instance()
{
	static HotKey* oneInstance = NULL;
	if (oneInstance == NULL)
		oneInstance = new HotKey();
	return oneInstance;
}


void HotKey::registerHotKey(Keys::KeyCode key, HotKeyListener *listener)
{
	PRT_DEBUG("HotKey registration for key " << Keys::getName(key));
	if (map.empty()) {
		EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
		EventDistributor::instance()->registerEventListener(SDL_KEYUP, this);
	}
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
	if (map.empty()) {
		EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this);
		EventDistributor::instance()->unregisterEventListener(SDL_KEYUP, this);
	}
}


void HotKey::registerHotKeyCommand(Keys::KeyCode key, const std::string &command)
{
	//PRT_DEBUG("HotKey command registration for key " << Keys::getName(key));
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


bool HotKey::signalEvent(SDL_Event &event, const EmuTime &time)
{
	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
	if (event.type == SDL_KEYUP)
		key = (Keys::KeyCode)(key | Keys::KD_UP);
	PRT_DEBUG("HotKey event " << Keys::getName(key));
	std::multimap<Keys::KeyCode, HotKeyListener*>::iterator it;
	for (it = map.lower_bound(key);
	     (it != map.end()) && (it->first == key);
	     it++) {
		it->second->signalHotKey(key, time);
	}
	return true;
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
void HotKey::HotKeyCmd::signalHotKey(Keys::KeyCode key, const EmuTime &time)
{
	CommandController::instance()->executeCommand(command, time);
}


void HotKey::BindCmd::execute(const std::vector<std::string> &tokens,
                              const EmuTime &time)
{
	HotKey *hk = HotKey::instance();
	switch (tokens.size()) {
	case 1: {
		// show all bounded keys
		std::multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
		for (it = hk->cmdMap.begin(); it != hk->cmdMap.end(); it++) {
			print(Keys::getName(it->first) + ":  " +
			      it->second->getCommand());
		}
		break;
	}
	case 2: {
		// show bindings for this key
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_UNKNOWN)
			throw CommandException("Unknown key");
		std::multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
		for (it = hk->cmdMap.lower_bound(key);
				(it != hk->cmdMap.end()) && (it->first == key);
				it++) {
			print(Keys::getName(it->first) + ":  " +
			      it->second->getCommand());
		}
		break;
	}
	case 3: {
		// make a new binding
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_UNKNOWN)
			throw CommandException("Unknown key");
		hk->registerHotKeyCommand(key, tokens[2]);
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
}
void HotKey::BindCmd::help(const std::vector<std::string> &tokens) const
{
	print("bind             : show all bounded keys");
	print("bind <key>       : show all bindings for this key");
	print("bind <key> <cmd> : bind key to command");
}

void HotKey::UnbindCmd::execute(const std::vector<std::string> &tokens,
                                const EmuTime &time)
{
	HotKey *hk = HotKey::instance();
	switch (tokens.size()) {
	case 2: {
		// unbind all for this key
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_UNKNOWN)
			throw CommandException("Unknown key");
		bool changed;
		do {	changed = false;
			std::multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
			it = hk->cmdMap.lower_bound(key);
			if ((it != hk->cmdMap.end()) && (it->first == key)) {
				hk->unregisterHotKeyCommand(key, it->second->getCommand());
				changed = true;
			}
		} while (changed);
		break;
	}
	case 3: {
		// unbind a specific command
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_UNKNOWN)
			throw CommandException("Unknown key");
		hk->unregisterHotKeyCommand(key, tokens[2]);
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
}
void HotKey::UnbindCmd::help(const std::vector<std::string> &tokens) const
{
	print("unbind <key>       : unbind all for this key");
	print("unbind <key> <cmd> : unbind a specific command");
}
