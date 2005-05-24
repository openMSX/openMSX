// $Id$

#include <cassert>
#include "openmsx.hh"
#include "HotKey.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "InputEvents.hh"
#include "SettingsConfig.hh"

using std::string;
using std::vector;


namespace openmsx {

const bool META_HOT_KEYS =
#ifdef __APPLE__
	true;
#else
	false;
#endif

HotKey::HotKey()
	: bindCmd(*this), unbindCmd(*this)
	, bindingsElement(SettingsConfig::instance().getCreateChild("bindings"))
{
	initBindings();

	EventDistributor::instance().registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::NATIVE);
	EventDistributor::instance().registerEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::NATIVE);

	CommandController::instance().registerCommand(&bindCmd,   "bind");
	CommandController::instance().registerCommand(&unbindCmd, "unbind");

	bindingsElement.addListener(*this);
}

void HotKey::initBindings()
{
	if (bindingsElement.getChildren().empty()) {
		// no (or empty) bindings section, use defaults
		if (META_HOT_KEYS) {
			// Hot key combos using Mac's Command key.
			registerHotKeyCommand("D+META", "screenshot");
			registerHotKeyCommand("P+META", "toggle pause");
			registerHotKeyCommand("T+META", "toggle throttle");
			registerHotKeyCommand("L+META", "toggle console");
			registerHotKeyCommand("U+META", "toggle mute");
			registerHotKeyCommand("F+META", "toggle fullscreen");
			registerHotKeyCommand("Q+META", "quit");
		} else {
			// Hot key combos for typical PC keyboards.
			registerHotKeyCommand("PRINT",      "screenshot");
			registerHotKeyCommand("PAUSE",      "toggle pause");
			registerHotKeyCommand("F9",         "toggle throttle");
			registerHotKeyCommand("F10",        "toggle console");
			registerHotKeyCommand("F11",        "toggle mute");
			registerHotKeyCommand("F12",        "toggle fullscreen");
			registerHotKeyCommand("RETURN+ALT", "toggle fullscreen");
			registerHotKeyCommand("F4+ALT",     "quit");
			registerHotKeyCommand("PAUSE+CTRL", "quit");
		}
	} else {
		// there is a bindings section, use those bindings
		const XMLElement::Children& children = bindingsElement.getChildren();
		for (XMLElement::Children::const_iterator it = children.begin();
		     it != children.end(); ++it) {
			registerHotKeyCommand(**it);
		}
	}
}

HotKey::~HotKey()
{
	bindingsElement.removeListener(*this);

	CommandController::instance().unregisterCommand(&bindCmd,   "bind");
	CommandController::instance().unregisterCommand(&unbindCmd, "unbind");

	for (CommandMap::const_iterator it = cmdMap.begin();
	     it != cmdMap.end(); ++it) {
		delete it->second;
	}

	EventDistributor::instance().unregisterEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::NATIVE);
	EventDistributor::instance().unregisterEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::NATIVE);
}

void HotKey::registerHotKeyCommand(const string& key, const string& command)
{
	XMLElement& elem = bindingsElement.getCreateChildWithAttribute(
		"bind", "key", key);
	elem.setData(command);
	registerHotKeyCommand(elem);
}

void HotKey::registerHotKeyCommand(const XMLElement& elem)
{
	Keys::KeyCode key = Keys::getCode(elem.getAttribute("key", ""));
	if (key == Keys::K_NONE) {
		return;
	}
	CommandMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		delete it->second;
	}
	cmdMap[key] = new HotKeyCmd(elem);
}


void HotKey::unregisterHotKeyCommand(Keys::KeyCode key)
{
	CommandMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		bindingsElement.removeChild(it->second->getElement());
		delete it->second;
		cmdMap.erase(it);
	}
}

void HotKey::childAdded(const XMLElement& parent,
                        const XMLElement& child)
{
	if (&parent); // avoid warning
	assert(&parent == &bindingsElement);
	if (child.getName() != "bind") {
		return;
	}
	registerHotKeyCommand(child);
}

bool HotKey::signalEvent(const Event& event)
{
	assert(dynamic_cast<const KeyEvent*>(&event));
	Keys::KeyCode key = static_cast<const KeyEvent&>(event).getKeyCode();
	CommandMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		it->second->execute();
		return false;
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
		CliComm::instance().printWarning(
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
		parent.registerHotKeyCommand(tokens[1], command);
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
