// $Id$

#include "HotKey.hh"
#include "CommandController.hh"
#include "CommandException.hh"
#include "UserInputEventDistributor.hh"
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

HotKey::HotKey(CommandController& commandController_,
               UserInputEventDistributor& userInputEventDistributor_)
	: bindCmd         (commandController_, *this)
	, unbindCmd       (commandController_, *this)
	, bindDefaultCmd  (commandController_, *this)
	, unbindDefaultCmd(commandController_, *this)
	, commandController(commandController_)
	, userInputEventDistributor(userInputEventDistributor_)
	, loading(false)
{
	initDefaultBindings();

	userInputEventDistributor.registerEventListener(
		UserInputEventDistributor::HOTKEY, *this);
}

HotKey::~HotKey()
{
	userInputEventDistributor.unregisterEventListener(
		UserInputEventDistributor::HOTKEY, *this);
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
		bindDefault(Keys::combine(Keys::K_Q, Keys::KM_META), "exit");
	} else {
		// Hot key combos for typical PC keyboards.
		bindDefault(Keys::K_PRINT, "screenshot");
		bindDefault(Keys::K_PAUSE, "toggle pause");
		bindDefault(Keys::K_F9,    "toggle throttle");
		bindDefault(Keys::K_F10,   "toggle console");
		bindDefault(Keys::K_F11,   "toggle mute");
		bindDefault(Keys::K_F12,   "toggle fullscreen");
		bindDefault(Keys::combine(Keys::K_F4, Keys::KM_ALT),     "exit");
		bindDefault(Keys::combine(Keys::K_PAUSE, Keys::KM_CTRL), "exit");
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
			commandController.getCliComm().printWarning(
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
		saveBindings(commandController.getSettingsConfig());
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
		saveBindings(commandController.getSettingsConfig());
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

bool HotKey::signalEvent(const UserInputEvent& event)
{
	// In the future we might support joystick buttons as hot keys as well.
	if (event.getType() != OPENMSX_KEY_DOWN_EVENT) {
		return true;
	}

	assert(dynamic_cast<const KeyEvent*>(&event));
	Keys::KeyCode key = static_cast<const KeyEvent&>(event).getKeyCode();
	BindMap::iterator it = cmdMap.find(key);
	if (it != cmdMap.end()) {
		try {
			// ignore return value
			commandController.executeCommand(it->second);
		} catch (CommandException& e) {
			commandController.getCliComm().printWarning(
				"Error executing hot key command: " + e.getMessage());
		}
		return false; // deny event to other key listeners
	}
	return true;
}


// class BindCmd

HotKey::BindCmd::BindCmd(CommandController& commandController, HotKey& hotKey_)
	: SimpleCommand(commandController, "bind")
	, hotKey(hotKey_)
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
		for (BindMap::iterator it = hotKey.cmdMap.begin();
		     it != hotKey.cmdMap.end(); it++) {
			result += Keys::getName(it->first) + ":  " +
			          it->second + '\n';
		}
		break;
	case 2: {
		// show bindings for this key
		BindMap::const_iterator it =
			hotKey.cmdMap.find(getCode(tokens[1]));
		if (it == hotKey.cmdMap.end()) {
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
		hotKey.bind(getCode(tokens[1]), command);
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

HotKey::UnbindCmd::UnbindCmd(CommandController& commandController,
                             HotKey& hotKey_)
	: SimpleCommand(commandController, "unbind")
	, hotKey(hotKey_)
{
}

string HotKey::UnbindCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	hotKey.unbind(getCode(tokens[1]));
	return "";
}
string HotKey::UnbindCmd::help(const vector<string>& /*tokens*/) const
{
	return "unbind <key> : unbind this key\n";
}

// class BindDefaultCmd

HotKey::BindDefaultCmd::BindDefaultCmd(CommandController& commandController,
                                       HotKey& hotKey_)
	: SimpleCommand(commandController, "bind_default")
	, hotKey(hotKey_)
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
		for (BindMap::iterator it = hotKey.defaultMap.begin();
		     it != hotKey.defaultMap.end(); it++) {
			result += Keys::getName(it->first) + ":  " +
			          it->second + '\n';
		}
		break;
	case 2: {
		// show bindings for this key
		BindMap::const_iterator it =
			hotKey.defaultMap.find(getCode(tokens[1]));
		if (it == hotKey.defaultMap.end()) {
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
		hotKey.bindDefault(getCode(tokens[1]), command);
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

HotKey::UnbindDefaultCmd::UnbindDefaultCmd(CommandController& commandController,
                                           HotKey& hotKey_)
	: SimpleCommand(commandController, "unbind_default")
	, hotKey(hotKey_)
{
}

string HotKey::UnbindDefaultCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	hotKey.unbindDefault(getCode(tokens[1]));
	return "";
}
string HotKey::UnbindDefaultCmd::help(const vector<string>& /*tokens*/) const
{
	return "unbind_default <key> : unbind this default key\n";
}

} // namespace openmsx
