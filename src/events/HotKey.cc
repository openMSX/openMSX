// $Id$

#include "HotKey.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "InputEvents.hh"
#include "XMLElement.hh"
#include "SettingsConfig.hh"
#include "openmsx.hh"
#include <cassert>

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
	, bindDefaultCmd(*this), unbindDefaultCmd(*this)
	, loading(false)
{
	initDefaultBindings();

	EventDistributor::instance().registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::NATIVE);
	EventDistributor::instance().registerEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::NATIVE);

	CommandController::instance().registerCommand(&bindCmd,   "bind");
	CommandController::instance().registerCommand(&unbindCmd, "unbind");
	CommandController::instance().registerCommand(&bindDefaultCmd,   "bind_default");
	CommandController::instance().registerCommand(&unbindDefaultCmd, "unbind_default");
}

HotKey::~HotKey()
{
	CommandController::instance().unregisterCommand(&bindCmd,   "bind");
	CommandController::instance().unregisterCommand(&unbindCmd, "unbind");
	CommandController::instance().unregisterCommand(&bindDefaultCmd,   "bind_default");
	CommandController::instance().unregisterCommand(&unbindDefaultCmd, "unbind_default");

	EventDistributor::instance().unregisterEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::NATIVE);
	EventDistributor::instance().unregisterEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::NATIVE);
}

void HotKey::initDefaultBindings()
{
	// TODO move to TCL script?
	
	if (META_HOT_KEYS) {
		// Hot key combos using Mac's Command key.
		bindDefault(Keys::combine(Keys::K_D, Keys::KM_META), "screenshot");
		bindDefault(Keys::combine(Keys::K_P, Keys::KM_META), "toggle pause");
		bindDefault(Keys::combine(Keys::K_T, Keys::KM_META), "toggle throttle");
		bindDefault(Keys::combine(Keys::K_L, Keys::KM_META), "toggle console");
		bindDefault(Keys::combine(Keys::K_U, Keys::KM_META), "toggle mute");
		bindDefault(Keys::combine(Keys::K_F, Keys::KM_META), "toggle fullscreen");
		bindDefault(Keys::combine(Keys::K_Q, Keys::KM_META), "quit");
	} else {
		// Hot key combos for typical PC keyboards.
		bindDefault(Keys::K_PRINT, "screenshot");
		bindDefault(Keys::K_PAUSE, "toggle pause");
		bindDefault(Keys::K_F9,    "toggle throttle");
		bindDefault(Keys::K_F10,   "toggle console");
		bindDefault(Keys::K_F11,   "toggle mute");
		bindDefault(Keys::K_F12,   "toggle fullscreen");
		bindDefault(Keys::combine(Keys::K_F4, Keys::KM_ALT),     "quit");
		bindDefault(Keys::combine(Keys::K_PAUSE, Keys::KM_CTRL), "quit");
		bindDefault(Keys::combine(Keys::K_RETURN, Keys::KM_ALT), "toggle fullscreen");
	}
}

static Keys::KeyCode getCode(const std::string& str)
{
	Keys::KeyCode key = Keys::getCode(str);
	if (key == Keys::K_NONE) {
		throw CommandException("Unknown key");
	}
	return key;
}

void HotKey::loadBindings(const XMLElement& config)
{
	// restore default bindings
	unboundKeys.clear();
	boundKeys.clear();
	cmdMap.clear();
	cmdMap.insert(defaultMap.begin(), defaultMap.end());
	
	// load bindings
	const XMLElement* bindingsElement = config.findChild("bindings");
	if (!bindingsElement) return;
	loading = true;
	const XMLElement::Children& children = bindingsElement->getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		XMLElement& elem = **it;
		try {
			if (elem.getName() == "bind") {
				bind(getCode(elem.getAttribute("key")),
				     elem.getData());
			} else if (elem.getName() == "unbind") {
				unbind(getCode(elem.getAttribute("key")));
			}
		} catch (MSXException& e) {
			CliComm::instance().printWarning(
				"Error while loading key bindings: " + e.getMessage());
		}
	}
	loading = false;
}


void HotKey::saveBindings(XMLElement& config) const
{
	XMLElement& bindingsElement = config.getCreateChild("bindings");
	bindingsElement.removeAllChildren();

	// add explicit bind's
	for (KeySet::const_iterator it = boundKeys.begin();
	     it != boundKeys.end(); ++it) {
		BindMap::const_iterator it2 = cmdMap.find(*it);
		assert(it2 != cmdMap.end());
		std::auto_ptr<XMLElement> elem(
			new XMLElement("bind", it2->second));
		elem->addAttribute("key", Keys::getName(*it));
		bindingsElement.addChild(elem);
	}
	// add explicit unbind's
	for (KeySet::const_iterator it = unboundKeys.begin();
	     it != unboundKeys.end(); ++it) {
		std::auto_ptr<XMLElement> elem(new XMLElement("unbind"));
		elem->addAttribute("key", Keys::getName(*it));
		bindingsElement.addChild(elem);
	}
}

void HotKey::bind(Keys::KeyCode key, const string& command)
{
	unboundKeys.erase(key);
	boundKeys.insert(key);
	defaultMap.erase(key);
	cmdMap[key] = command;
	
	// TODO hack, remove when singleton stuff is cleaned up
	if (!loading) {
		saveBindings(SettingsConfig::instance());
	}
}

void HotKey::unbind(Keys::KeyCode key)
{
	if (boundKeys.find(key) == boundKeys.end()) {
		// only when not a regular bound key
		unboundKeys.insert(key);
	}
	boundKeys.erase(key);
	defaultMap.erase(key);
	cmdMap.erase(key);
	
	// TODO hack, remove when singleton stuff is cleaned up
	if (!loading) {
		saveBindings(SettingsConfig::instance());
	}
}

void HotKey::bindDefault(Keys::KeyCode key, const std::string& command)
{
	if ((unboundKeys.find(key) == unboundKeys.end()) &&
	    (boundKeys.find(key)   == boundKeys.end())) {
		// not explicity bound or unbound
		cmdMap[key] = command;
	}
	defaultMap[key] = command;
}

void HotKey::unbindDefault(Keys::KeyCode key)
{
	if ((unboundKeys.find(key) == unboundKeys.end()) &&
	    (boundKeys.find(key)   == boundKeys.end())) {
		// not explicity bound or unbound
		cmdMap.erase(key);
	}
	defaultMap.erase(key);
}

bool HotKey::signalEvent(const Event& event)
{
	assert(dynamic_cast<const KeyEvent*>(&event));
	Keys::KeyCode key = static_cast<const KeyEvent&>(event).getKeyCode();
	BindMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		try {
			// ignore return value
			CommandController::instance().executeCommand(it->second);
		} catch (CommandException& e) {
			CliComm::instance().printWarning(
				"Error executing hot key command: " + e.getMessage());
		}
		return false;
	}
	return true;
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
		for (BindMap::iterator it = parent.cmdMap.begin();
		     it != parent.cmdMap.end(); it++) {
			result += Keys::getName(it->first) + ":  " +
			          it->second + '\n';
		}
		break;
	case 2: {
		// show bindings for this key
		BindMap::const_iterator it =
			parent.cmdMap.find(getCode(tokens[1]));
		if (it == parent.cmdMap.end()) {
			throw CommandException("Key not bound");
		}
		result = Keys::getName(it->first) + ":  " + it->second + '\n';
		break;
	}
	default: {
		// make a new binding
		string command;
		for (unsigned i = 2; i < tokens.size(); ++i) {
			if (i != 2) command += ' ';
			command += tokens[i];
		}
		parent.bind(getCode(tokens[1]), command);
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
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	parent.unbind(getCode(tokens[1]));
	return "";
}
string HotKey::UnbindCmd::help(const vector<string>& /*tokens*/) const
{
	return "unbind <key> : unbind this key\n";
}

// class BindCmd

HotKey::BindDefaultCmd::BindDefaultCmd(HotKey& parent_)
	: parent(parent_)
{
}

string HotKey::BindDefaultCmd::execute(const vector<string>& tokens)
{
	string result;
	switch (tokens.size()) {
	case 0:
		assert(false);
	case 1:
		// show all bounded keys
		for (BindMap::iterator it = parent.defaultMap.begin();
		     it != parent.defaultMap.end(); it++) {
			result += Keys::getName(it->first) + ":  " +
			          it->second + '\n';
		}
		break;
	case 2: {
		// show bindings for this key
		BindMap::const_iterator it =
			parent.defaultMap.find(getCode(tokens[1]));
		if (it == parent.defaultMap.end()) {
			throw CommandException("Key not bound");
		}
		result = Keys::getName(it->first) + ":  " + it->second + '\n';
		break;
	}
	default: {
		// make a new binding
		string command;
		for (unsigned i = 2; i < tokens.size(); ++i) {
			if (i != 2) command += ' ';
			command += tokens[i];
		}
		parent.bindDefault(getCode(tokens[1]), command);
		break;
	}
	}
	return result;
}
string HotKey::BindDefaultCmd::help(const vector<string>& /*tokens*/) const
{
	return "bind_default             : show all bounded default keys\n"
	       "bind_default <key>       : show binding for this default key\n"
	       "bind_default <key> <cmd> : bind default key to command\n";
}


// class UnbindDefaultCmd

HotKey::UnbindDefaultCmd::UnbindDefaultCmd(HotKey& parent_)
	: parent(parent_)
{
}

string HotKey::UnbindDefaultCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	parent.unbindDefault(getCode(tokens[1]));
	return "";
}
string HotKey::UnbindDefaultCmd::help(const vector<string>& /*tokens*/) const
{
	return "unbind_default <key> : unbind this default key\n";
}

} // namespace openmsx
