// $Id$

#include <cassert>
#include "openmsx.hh"
#include "HotKey.hh"
#include "CommandController.hh"
#include "EventDistributor.hh"
#include "CliCommOutput.hh"
#include "InputEvents.hh"
#include "SettingsConfig.hh"

namespace openmsx {

HotKey::HotKey()
	: bindCmd(*this), unbindCmd(*this)
	, bindingsElement(SettingsConfig::instance().getCreateChild("bindings"))
{
	XMLElement::Children children(bindingsElement.getChildren()); // copy
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		Keys::KeyCode key = Keys::getCode((*it)->getAttribute("key", ""));
		if (key != Keys::K_NONE) {
			registerHotKeyCommand(key, (*it)->getData());
		}
	}
	
	EventDistributor::instance().registerEventListener(
		KEY_DOWN_EVENT, *this, EventDistributor::NATIVE);
	EventDistributor::instance().registerEventListener(
		KEY_UP_EVENT, *this, EventDistributor::NATIVE);

	CommandController::instance().registerCommand(&bindCmd,   "bind");
	CommandController::instance().registerCommand(&unbindCmd, "unbind");
}

HotKey::~HotKey()
{
	CommandController::instance().unregisterCommand(&bindCmd,   "bind");
	CommandController::instance().unregisterCommand(&unbindCmd, "unbind");

	for (CommandMap::const_iterator it = cmdMap.begin();
	     it != cmdMap.end(); ++it) {
		delete it->second;
	}

	EventDistributor::instance().unregisterEventListener(
		KEY_UP_EVENT, *this, EventDistributor::NATIVE);
	EventDistributor::instance().unregisterEventListener(
		KEY_DOWN_EVENT, *this, EventDistributor::NATIVE);
}

void HotKey::registerHotKeyCommand(Keys::KeyCode key, const string& command)
{
	CommandMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		unregisterHotKeyCommand(key);
	}
	XMLElement& elem = bindingsElement.getCreateChildWithAttribute(
		"bind", "key", Keys::getName(key));
	elem.setData(command);
	cmdMap[key] = new HotKeyCmd(elem);
}

void HotKey::unregisterHotKeyCommand(Keys::KeyCode key)
{
	CommandMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		bindingsElement.deleteChild(it->second->getElement());
		delete it->second;
		cmdMap.erase(it);
	}
}

bool HotKey::signalEvent(const Event& event)
{
	assert(dynamic_cast<const KeyEvent*>(&event));
	Keys::KeyCode key = static_cast<const KeyEvent&>(event).getKeyCode();
	CommandMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		it->second->execute();
	}
	return true;
}


// class HotKeyCmd

HotKey::HotKeyCmd::HotKeyCmd(const XMLElement& elem_)
	: elem(elem_)
{
}

const string& HotKey::HotKeyCmd::getCommand() const
{
	return elem.getData();
}

const XMLElement& HotKey::HotKeyCmd::getElement() const
{
	return elem;
}

void HotKey::HotKeyCmd::execute()
{
	try {
		// ignore return value
		CommandController::instance().executeCommand(getCommand());
	} catch (CommandException &e) {
		CliCommOutput::instance().printWarning(
		        "Error executing hot key command: " + e.getMessage());
	}
}


// class BindCmd

HotKey::BindCmd::BindCmd(HotKey& parent_)
	: parent(parent_)
{
}

string HotKey::BindCmd::execute(const vector<string>& tokens)
{
	string result;
	switch (tokens.size()) {
	case 0:
		assert(false);
	case 1: 
		// show all bounded keys
		for (CommandMap::iterator it = parent.cmdMap.begin();
		     it != parent.cmdMap.end(); it++) {
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
		CommandMap::const_iterator it = parent.cmdMap.find(key);
		if (it == parent.cmdMap.end()) {
			throw CommandException("Key not bound");
		}
		result = Keys::getName(it->first) + ":  " +
		      it->second->getCommand() + '\n';
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
		parent.registerHotKeyCommand(key, command);
		break;
	}
	}
	return result;
}
string HotKey::BindCmd::help(const vector<string>& /*tokens*/) const
{
	return "bind             : show all bounded keys\n"
	       "bind <key>       : show binding for this key\n"
	       "bind <key> <cmd> : bind key to command\n";
}


// class UnbindCmd

HotKey::UnbindCmd::UnbindCmd(HotKey& parent_)
	: parent(parent_)
{
}

string HotKey::UnbindCmd::execute(const vector<string>& tokens)
{
	string result;
	switch (tokens.size()) {
	case 2: {
		// unbind this key
		Keys::KeyCode key = Keys::getCode(tokens[1]);
		if (key == Keys::K_NONE) {
			throw CommandException("Unknown key");
		}
		parent.unregisterHotKeyCommand(key);
		break;
	}
	default:
		throw SyntaxError();
	}
	return result;
}
string HotKey::UnbindCmd::help(const vector<string>& /*tokens*/) const
{
	return "unbind <key> : unbind this key\n";
}

} // namespace openmsx
