// $Id$

#include <cassert>
#include "openmsx.hh"
#include "HotKey.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "CliCommunicator.hh"


namespace openmsx {

HotKey::HotKey()
{
	CommandController::instance()->registerCommand(&bindCmd,   "bind");
	CommandController::instance()->registerCommand(&unbindCmd, "unbind");
}

HotKey::~HotKey()
{
	assert(map.empty());
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


void HotKey::registerHotKey(Keys::KeyCode key, HotKeyListener* listener)
{
	PRT_DEBUG("HotKey registration for key " << Keys::getName(key));
	if (map.empty()) {
		EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this);
		EventDistributor::instance()->registerEventListener(SDL_KEYUP, this);
	}
	map.insert(pair<Keys::KeyCode, HotKeyListener*>(key, listener));
}

void HotKey::unregisterHotKey(Keys::KeyCode key, HotKeyListener* listener)
{
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		map.equal_range(key);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
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


void HotKey::registerHotKeyCommand(Keys::KeyCode key, const string& command)
{
	HotKeyCmd* cmd = new HotKeyCmd(command);
	registerHotKey(key, cmd);
	cmdMap.insert(pair<Keys::KeyCode, HotKeyCmd*>(key, cmd));
}

void HotKey::unregisterHotKeyCommand(Keys::KeyCode key, const string& command)
{
	pair<CommandMap::iterator, CommandMap::iterator> bounds =
		cmdMap.equal_range(key);
	for (CommandMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		HotKeyCmd* cmd = it->second;
		if (cmd->getCommand() == command) {
			unregisterHotKey(key, cmd);
			cmdMap.erase(it);
			break;
		}
	}
}


bool HotKey::signalEvent(SDL_Event& event) throw()
{
	Keys::KeyCode key = (Keys::KeyCode)event.key.keysym.sym;
	if (event.type == SDL_KEYUP) {
		key = (Keys::KeyCode)(key | Keys::KD_UP);
	}
	pair<ListenerMap::iterator, ListenerMap::iterator> bounds =
		map.equal_range(key);
	for (ListenerMap::iterator it = bounds.first;
	     it != bounds.second; ++it) {
		it->second->signalHotKey(key);
	}
	return true;
}


HotKey::HotKeyCmd::HotKeyCmd(const string& cmd)
{
	command = cmd;
}
HotKey::HotKeyCmd::~HotKeyCmd()
{
}
const string &HotKey::HotKeyCmd::getCommand() const
{
	return command;
}
void HotKey::HotKeyCmd::signalHotKey(Keys::KeyCode key) throw()
{
	try {
		// ignore return value
		CommandController::instance()->executeCommand(command);
	} catch (CommandException &e) {
		CliCommunicator::instance().printWarning(
		        "Error executing hot key command: " + e.getMessage());
	}
}

string HotKey::BindCmd::execute(const vector<string>& tokens)
	throw (CommandException)
{
	string result;
	HotKey* hk = HotKey::instance();
	switch (tokens.size()) {
	case 0:
		assert(false);
	case 1: 
		// show all bounded keys
		for (CommandMap::iterator it = hk->cmdMap.begin();
		     it != hk->cmdMap.end(); it++) {
			result += Keys::getName(it->first) + ":  " +
			          it->second->getCommand() + '\n';
		}
		break;
	case 2: {
		// show bindings for this key
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_NONE) {
			throw CommandException("Unknown key");
		}
		pair<CommandMap::iterator, CommandMap::iterator> bounds =
			hk->cmdMap.equal_range(key);
		for (CommandMap::iterator it = bounds.first;
		     it != bounds.second; ++it) {
			result += Keys::getName(it->first) + ":  " +
			      it->second->getCommand() + '\n';
		}
		break;
	}
	default: {
		// make a new binding
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_NONE) {
			throw CommandException("Unknown key");
		}
		string command;
		for (unsigned i = 2; i < tokens.size(); i++) {
			if (i != 2) command += ' ';
			command += tokens[i];
		}
		hk->registerHotKeyCommand(key, command);
		break;
	}
	}
	return result;
}
string HotKey::BindCmd::help(const vector<string>& tokens) const throw()
{
	return "bind             : show all bounded keys\n"
	       "bind <key>       : show all bindings for this key\n"
	       "bind <key> <cmd> : bind key to command\n";
}

string HotKey::UnbindCmd::execute(const vector<string>& tokens)
	throw (CommandException)
{
	string result;
	HotKey* hk = HotKey::instance();
	switch (tokens.size()) {
	case 2: {
		// unbind all for this key
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_NONE) {
			throw CommandException("Unknown key");
		}
		pair<CommandMap::iterator, CommandMap::iterator> bounds =
			hk->cmdMap.equal_range(key);
		// cannot iterate over changing container, so make copy
		CommandMap copy(bounds.first, bounds.second);
		for (CommandMap::iterator it = copy.begin();
		     it != copy.end(); ++it) {
			hk->unregisterHotKeyCommand(key,
				it->second->getCommand());
		}
		break;
	}
	case 3: {
		// unbind a specific command
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_NONE) {
			throw CommandException("Unknown key");
		}
		hk->unregisterHotKeyCommand(key, tokens[2]);
		break;
	}
	default:
		throw CommandException("Syntax error");
	}
	return result;
}
string HotKey::UnbindCmd::help(const vector<string>& tokens) const throw()
{
	return "unbind <key>       : unbind all for this key\n"
	       "unbind <key> <cmd> : unbind a specific command\n";
}

} // namespace openmsx
