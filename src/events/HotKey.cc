// $Id$

#include "HotKey.hh"
#include "InputEventFactory.hh"
#include "CommandController.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "InputEvents.hh"
#include "XMLElement.hh"
#include "SettingsConfig.hh"
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

class BindCmd : public SimpleCommand
{
public:
	BindCmd(CommandController& commandController, HotKey& hotKey);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
};

class UnbindCmd : public SimpleCommand
{
public:
	UnbindCmd(CommandController& commandController, HotKey& hotKey);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
};

class BindDefaultCmd : public SimpleCommand
{
public:
	BindDefaultCmd(CommandController& commandController, HotKey& hotKey);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
};

class UnbindDefaultCmd : public SimpleCommand
{
public:
	UnbindDefaultCmd(CommandController& commandController, HotKey& hotKey);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
};


HotKey::HotKey(CommandController& commandController_,
               EventDistributor& eventDistributor_)
	: bindCmd         (new BindCmd         (commandController_, *this))
	, unbindCmd       (new UnbindCmd       (commandController_, *this))
	, bindDefaultCmd  (new BindDefaultCmd  (commandController_, *this))
	, unbindDefaultCmd(new UnbindDefaultCmd(commandController_, *this))
	, commandController(commandController_)
	, eventDistributor(eventDistributor_)
{
	initDefaultBindings();

	eventDistributor.registerEventListener(
		OPENMSX_KEY_DOWN_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_KEY_UP_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_MOUSE_BUTTON_UP_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_DOWN_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_JOY_BUTTON_UP_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_FOCUS_EVENT, *this, EventDistributor::HOTKEY);
}

HotKey::~HotKey()
{
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_BUTTON_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_KEY_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_KEY_DOWN_EVENT, *this);
}

void HotKey::initDefaultBindings()
{
	// TODO move to Tcl script?

	if (META_HOT_KEYS) {
		// Hot key combos using Mac's Command key.
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_D, Keys::KM_META))),
		            "screenshot");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_P, Keys::KM_META))),
		            "toggle pause");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_T, Keys::KM_META))),
		            "toggle throttle");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_L, Keys::KM_META))),
		            "toggle console");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_U, Keys::KM_META))),
		            "toggle mute");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_F, Keys::KM_META))),
		            "toggle fullscreen");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_Q, Keys::KM_META))),
		            "exit");
	} else {
		// Hot key combos for typical PC keyboards.
		bindDefault(EventPtr(new KeyDownEvent(Keys::K_PRINT)),
		            "screenshot");
		bindDefault(EventPtr(new KeyDownEvent(Keys::K_PAUSE)),
		            "toggle pause");
		bindDefault(EventPtr(new KeyDownEvent(Keys::K_F9)),
		            "toggle throttle");
		bindDefault(EventPtr(new KeyDownEvent(Keys::K_F10)),
		            "toggle console");
		bindDefault(EventPtr(new KeyDownEvent(Keys::K_F11)),
		            "toggle mute");
		bindDefault(EventPtr(new KeyDownEvent(Keys::K_F12)),
		            "toggle fullscreen");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_F4, Keys::KM_ALT))),
		            "exit");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_PAUSE, Keys::KM_CTRL))),
		            "exit");
		bindDefault(EventPtr(new KeyDownEvent(
		               Keys::combine(Keys::K_RETURN, Keys::KM_ALT))),
		            "toggle fullscreen");
	}
}

static HotKey::EventPtr createEvent(const string& str)
{
	HotKey::EventPtr event = InputEventFactory::createInputEvent(str);
	if (!dynamic_cast<const KeyEvent*>           (event.get()) &&
	    !dynamic_cast<const MouseButtonEvent*>   (event.get()) &&
	    !dynamic_cast<const JoystickButtonEvent*>(event.get()) &&
	    !dynamic_cast<const FocusEvent*>         (event.get())) {
		throw CommandException("Unsupported event type");
	}
	return event;
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
	XMLElement copy(*bindingsElement); // dont iterate over changing container
	const XMLElement::Children& children = copy.getChildren();
	for (XMLElement::Children::const_iterator it = children.begin();
	     it != children.end(); ++it) {
		XMLElement& elem = **it;
		try {
			if (elem.getName() == "bind") {
				bind(createEvent(elem.getAttribute("key")),
				     elem.getData());
			} else if (elem.getName() == "unbind") {
				unbind(createEvent(elem.getAttribute("key")));
			}
		} catch (MSXException& e) {
			commandController.getCliComm().printWarning(
				"Error while loading key bindings: " + e.getMessage());
		}
	}
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
		elem->addAttribute("key", (*it)->toString());
		bindingsElement.addChild(elem);
	}
	// add explicit unbind's
	for (KeySet::const_iterator it = unboundKeys.begin();
	     it != unboundKeys.end(); ++it) {
		std::auto_ptr<XMLElement> elem(new XMLElement("unbind"));
		elem->addAttribute("key", (*it)->toString());
		bindingsElement.addChild(elem);
	}
}

void HotKey::bind(EventPtr event, const string& command)
{
	unboundKeys.erase(event);
	boundKeys.insert(event);
	defaultMap.erase(event);
	cmdMap[event] = command;

	saveBindings(commandController.getSettingsConfig().getXMLElement());
}

void HotKey::unbind(EventPtr event)
{
	if (boundKeys.find(event) == boundKeys.end()) {
		// only when not a regular bound event
		unboundKeys.insert(event);
	}
	boundKeys.erase(event);
	defaultMap.erase(event);
	cmdMap.erase(event);

	saveBindings(commandController.getSettingsConfig().getXMLElement());
}

void HotKey::bindDefault(EventPtr event, const string& command)
{
	if ((unboundKeys.find(event) == unboundKeys.end()) &&
	    (boundKeys.find(event)   == boundKeys.end())) {
		// not explicity bound or unbound
		cmdMap[event] = command;
	}
	defaultMap[event] = command;
}

void HotKey::unbindDefault(EventPtr event)
{
	if ((unboundKeys.find(event) == unboundKeys.end()) &&
	    (boundKeys.find(event)   == boundKeys.end())) {
		// not explicity bound or unbound
		cmdMap.erase(event);
	}
	defaultMap.erase(event);
}

bool HotKey::signalEvent(EventPtr event)
{
	BindMap::iterator it = cmdMap.find(event);
	if (it == cmdMap.end()) {
		return true;
	}
	try {
		// ignore return value
		commandController.executeCommand(it->second);
	} catch (CommandException& e) {
		commandController.getCliComm().printWarning(
			"Error executing hot key command: " + e.getMessage());
	}
	return false; // deny event to other listeners
}


// class BindCmd

BindCmd::BindCmd(CommandController& commandController, HotKey& hotKey_)
	: SimpleCommand(commandController, "bind")
	, hotKey(hotKey_)
{
}

string BindCmd::execute(const vector<string>& tokens)
{
	string result;
	switch (tokens.size()) {
	case 0:
		assert(false);
	case 1:
		// show all bounded keys
		for (HotKey::BindMap::iterator it = hotKey.cmdMap.begin();
		     it != hotKey.cmdMap.end(); it++) {
			result += it->first->toString() + ":  " +
			          it->second + '\n';
		}
		break;
	case 2: {
		// show bindings for this key
		HotKey::BindMap::const_iterator it =
			hotKey.cmdMap.find(createEvent(tokens[1]));
		if (it == hotKey.cmdMap.end()) {
			throw CommandException("Key not bound");
		}
		result = it->first->toString() + ":  " + it->second + '\n';
		break;
	}
	default: {
		// make a new binding
		string command;
		for (unsigned i = 2; i < tokens.size(); ++i) {
			if (i != 2) command += ' ';
			command += tokens[i];
		}
		hotKey.bind(createEvent(tokens[1]), command);
		break;
	}
	}
	return result;
}
string BindCmd::help(const vector<string>& /*tokens*/) const
{
	return "bind             : show all bounded keys\n"
	       "bind <key>       : show binding for this key\n"
	       "bind <key> <cmd> : bind key to command\n";
}


// class UnbindCmd

UnbindCmd::UnbindCmd(CommandController& commandController,
                             HotKey& hotKey_)
	: SimpleCommand(commandController, "unbind")
	, hotKey(hotKey_)
{
}

string UnbindCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	hotKey.unbind(createEvent(tokens[1]));
	return "";
}
string UnbindCmd::help(const vector<string>& /*tokens*/) const
{
	return "unbind <key> : unbind this key\n";
}

// class BindDefaultCmd

BindDefaultCmd::BindDefaultCmd(CommandController& commandController,
                                       HotKey& hotKey_)
	: SimpleCommand(commandController, "bind_default")
	, hotKey(hotKey_)
{
}

string BindDefaultCmd::execute(const vector<string>& tokens)
{
	string result;
	switch (tokens.size()) {
	case 0:
		assert(false);
	case 1:
		// show all bounded keys
		for (HotKey::BindMap::iterator it = hotKey.defaultMap.begin();
		     it != hotKey.defaultMap.end(); it++) {
			result += it->first->toString() + ":  " +
			          it->second + '\n';
		}
		break;
	case 2: {
		// show bindings for this key
		HotKey::BindMap::const_iterator it =
			hotKey.defaultMap.find(createEvent(tokens[1]));
		if (it == hotKey.defaultMap.end()) {
			throw CommandException("Key not bound");
		}
		result = it->first->toString() + ":  " + it->second + '\n';
		break;
	}
	default: {
		// make a new binding
		string command;
		for (unsigned i = 2; i < tokens.size(); ++i) {
			if (i != 2) command += ' ';
			command += tokens[i];
		}
		hotKey.bindDefault(createEvent(tokens[1]), command);
		break;
	}
	}
	return result;
}
string BindDefaultCmd::help(const vector<string>& /*tokens*/) const
{
	return "bind_default             : show all bounded default keys\n"
	       "bind_default <key>       : show binding for this default key\n"
	       "bind_default <key> <cmd> : bind default key to command\n";
}


// class UnbindDefaultCmd

UnbindDefaultCmd::UnbindDefaultCmd(CommandController& commandController,
                                           HotKey& hotKey_)
	: SimpleCommand(commandController, "unbind_default")
	, hotKey(hotKey_)
{
}

string UnbindDefaultCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	hotKey.unbindDefault(createEvent(tokens[1]));
	return "";
}
string UnbindDefaultCmd::help(const vector<string>& /*tokens*/) const
{
	return "unbind_default <key> : unbind this default key\n";
}

} // namespace openmsx
