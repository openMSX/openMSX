// $Id$

#include "openmsx.hh"
#include "HotKey.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include <cassert>


namespace openmsx {

HotKey::HotKey()
{
	CommandController::instance()->registerCommand(&bindCmd,   "bind");
	CommandController::instance()->registerCommand(&unbindCmd, "unbind");
}

HotKey::~HotKey()
{
	CommandController::instance()->unregisterCommand(&bindCmd,   "bind");
	CommandController::instance()->unregisterCommand(&unbindCmd, "unbind");
}

HotKey* HotKey::instance()
{
	static HotKey* oneInstance = NULL;
	if (oneInstance == NULL) {
		oneInstance = new HotKey();
	}
	return oneInstance;
}


void HotKey::registerHotKey(Keys::KeyCode key, HotKeyListener *listener)
{
	PRT_DEBUG("HotKey registration for key " << Keys::getName(key));
	if (map.empty()) {
		EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
		EventDistributor::instance()->registerEventListener(SDL_KEYUP, this);
	}
	map.insert(pair<Keys::KeyCode, HotKeyListener*>(key, listener));
}

void HotKey::unregisterHotKey(Keys::KeyCode key, HotKeyListener *listener)
{
	multimap<Keys::KeyCode, HotKeyListener*>::iterator it;
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


void HotKey::registerHotKeyCommand(Keys::KeyCode key, const string &command)
{
	//PRT_DEBUG("HotKey command registration for key " << Keys::getName(key));
	HotKeyCmd *cmd = new HotKeyCmd(command);
	registerHotKey(key, cmd);
	cmdMap.insert(pair<Keys::KeyCode, HotKeyCmd*>(key, cmd));
}

void HotKey::unregisterHotKeyCommand(Keys::KeyCode key, const string &command)
{
	multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
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


bool HotKey::signalEvent(SDL_Event &event)
{
	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
	if (event.type == SDL_KEYUP)
		key = (Keys::KeyCode)(key | Keys::KD_UP);
	PRT_DEBUG("HotKey event " << Keys::getName(key));
	multimap<Keys::KeyCode, HotKeyListener*>::iterator it;
	for (it = map.lower_bound(key);
	     (it != map.end()) && (it->first == key);
	     it++) {
		it->second->signalHotKey(key);
	}
	return true;
}


HotKey::HotKeyCmd::HotKeyCmd(const string &cmd)
{
	command = cmd;
}
HotKey::HotKeyCmd::~HotKeyCmd()
{
}
const string &HotKey::HotKeyCmd::getCommand()
{
	return command;
}
void HotKey::HotKeyCmd::signalHotKey(Keys::KeyCode key)
{
	try {
		CommandController::instance()->executeCommand(command);
	} catch (CommandException &e) {
		PRT_INFO("Error executing hot key command: " << e.getMessage());
	}
}

void HotKey::BindCmd::execute(const vector<string> &tokens)
{
	HotKey *hk = HotKey::instance();
	switch (tokens.size()) {
	case 0:
		assert(false);
	case 1: {
		// show all bounded keys
		multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
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
		multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
		for (it = hk->cmdMap.lower_bound(key);
				(it != hk->cmdMap.end()) && (it->first == key);
				it++) {
			print(Keys::getName(it->first) + ":  " +
			      it->second->getCommand());
		}
		break;
	}
	default: {
		// make a new binding
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_UNKNOWN) {
			throw CommandException("Unknown key");
		}
		string command;
		for (uint i = 2; i < tokens.size(); i++) {
			if (i != 2) command += ' ';
			command += tokens[i];
		}
		hk->registerHotKeyCommand(key, command);
		break;
	}
	}
}
void HotKey::BindCmd::help(const vector<string> &tokens) const
{
	print("bind             : show all bounded keys");
	print("bind <key>       : show all bindings for this key");
	print("bind <key> <cmd> : bind key to command");
}

void HotKey::UnbindCmd::execute(const vector<string> &tokens)
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
			multimap<Keys::KeyCode, HotKeyCmd*>::iterator it;
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
void HotKey::UnbindCmd::help(const vector<string> &tokens) const
{
	print("unbind <key>       : unbind all for this key");
	print("unbind <key> <cmd> : unbind a specific command");
}

} // namespace openmsx
