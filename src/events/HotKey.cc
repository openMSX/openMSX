// $Id$

#include "HotKey.hh"
#include "InputEventFactory.hh"
#include "GlobalCommandController.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "InputEvents.hh"
#include "XMLElement.hh"
#include "SettingsConfig.hh"
#include "AlarmEvent.hh"
#include "memory.hh"
#include "unreachable.hh"
#include <cassert>

using std::string;
using std::vector;
using std::make_shared;

namespace openmsx {

const bool META_HOT_KEYS =
#ifdef __APPLE__
	true;
#else
	false;
#endif

class BindCmd : public Command
{
public:
	BindCmd(CommandController& commandController, HotKey& hotKey,
	        bool defaultCmd);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	string formatBinding(HotKey::BindMap::const_iterator it);

	HotKey& hotKey;
	const bool defaultCmd;
};

class UnbindCmd : public Command
{
public:
	UnbindCmd(CommandController& commandController, HotKey& hotKey,
	          bool defaultCmd);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
	const bool defaultCmd;
};


HotKey::HotKey(GlobalCommandController& commandController_,
               EventDistributor& eventDistributor_)
	: bindCmd(make_unique<BindCmd>(
		commandController_, *this, false))
	, unbindCmd(make_unique<UnbindCmd>(
		commandController_, *this, false))
	, bindDefaultCmd(make_unique<BindCmd>(
		commandController_, *this, true))
	, unbindDefaultCmd(make_unique<UnbindCmd>(
		commandController_, *this, true))
	, repeatAlarm(make_unique<AlarmEvent>(
		eventDistributor_, *this, OPENMSX_REPEAT_HOTKEY,
		EventDistributor::HOTKEY))
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
		OPENMSX_JOY_AXIS_MOTION_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_FOCUS_EVENT, *this, EventDistributor::HOTKEY);
}

HotKey::~HotKey()
{
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_BUTTON_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_AXIS_MOTION_EVENT, *this);
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
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_D, Keys::KM_META)),
		            HotKeyInfo("screenshot -guess-name"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_P, Keys::KM_META)),
		            HotKeyInfo("toggle pause"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_T, Keys::KM_META)),
		            HotKeyInfo("toggle throttle"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_L, Keys::KM_META)),
		            HotKeyInfo("toggle console"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_U, Keys::KM_META)),
		            HotKeyInfo("toggle mute"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_F, Keys::KM_META)),
		            HotKeyInfo("toggle fullscreen"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_Q, Keys::KM_META)),
		            HotKeyInfo("exit"));
	} else {
		// Hot key combos for typical PC keyboards.
		bindDefault(make_shared<KeyDownEvent>(Keys::K_PRINT),
		            HotKeyInfo("screenshot -guess-name"));
		bindDefault(make_shared<KeyDownEvent>(Keys::K_PAUSE),
		            HotKeyInfo("toggle pause"));
		bindDefault(make_shared<KeyDownEvent>(Keys::K_F9),
		            HotKeyInfo("toggle throttle"));
		bindDefault(make_shared<KeyDownEvent>(Keys::K_F10),
		            HotKeyInfo("toggle console"));
		bindDefault(make_shared<KeyDownEvent>(Keys::K_F11),
		            HotKeyInfo("toggle mute"));
		bindDefault(make_shared<KeyDownEvent>(Keys::K_F12),
		            HotKeyInfo("toggle fullscreen"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_F4, Keys::KM_ALT)),
		            HotKeyInfo("exit"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_PAUSE, Keys::KM_CTRL)),
		            HotKeyInfo("exit"));
		bindDefault(make_shared<KeyDownEvent>(
		            Keys::combine(Keys::K_RETURN, Keys::KM_ALT)),
		            HotKeyInfo("toggle fullscreen"));
	}
}

static HotKey::EventPtr createEvent(const string& str)
{
	HotKey::EventPtr event = InputEventFactory::createInputEvent(str);
	if (!dynamic_cast<const KeyEvent*>         (event.get()) &&
	    !dynamic_cast<const MouseButtonEvent*> (event.get()) &&
	    !dynamic_cast<const JoystickEvent*>    (event.get()) &&
	    !dynamic_cast<const FocusEvent*>       (event.get())) {
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
				     HotKeyInfo(elem.getData(),
				                elem.getAttributeAsBool("repeat", false)));
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
		const HotKeyInfo& info = it2->second;
		auto elem = make_unique<XMLElement>("bind", info.command);
		elem->addAttribute("key", (*it)->toString());
		if (info.repeat) {
			elem->addAttribute("repeat", "true");
		}
		bindingsElement.addChild(std::move(elem));
	}
	// add explicit unbind's
	for (KeySet::const_iterator it = unboundKeys.begin();
	     it != unboundKeys.end(); ++it) {
		auto elem = make_unique<XMLElement>("unbind");
		elem->addAttribute("key", (*it)->toString());
		bindingsElement.addChild(std::move(elem));
	}
}

void HotKey::bind(const EventPtr& event, const HotKeyInfo& info)
{
	unboundKeys.erase(event);
	boundKeys.insert(event);
	defaultMap.erase(event);
	cmdMap[event] = info;

	saveBindings(commandController.getSettingsConfig().getXMLElement());
}

void HotKey::unbind(const EventPtr& event)
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

void HotKey::bindDefault(const EventPtr& event, const HotKeyInfo& info)
{
	if ((unboundKeys.find(event) == unboundKeys.end()) &&
	    (boundKeys.find(event)   == boundKeys.end())) {
		// not explicity bound or unbound
		cmdMap[event] = info;
	}
	defaultMap[event] = info;
}

void HotKey::unbindDefault(const EventPtr& event)
{
	if ((unboundKeys.find(event) == unboundKeys.end()) &&
	    (boundKeys.find(event)   == boundKeys.end())) {
		// not explicity bound or unbound
		cmdMap.erase(event);
	}
	defaultMap.erase(event);
}

int HotKey::signalEvent(const EventPtr& event_)
{
	EventPtr event = event_;
	if (event->getType() == OPENMSX_REPEAT_HOTKEY) {
		if (!lastEvent.get()) return true;
		event = lastEvent;
	} else if (lastEvent.get() && (*lastEvent != *event)) {
		stopRepeat();
	}
	BindMap::iterator it = cmdMap.find(event);
	if (it == cmdMap.end()) {
		return 0;
	}
	const HotKeyInfo& info = it->second;
	if (info.repeat) {
		startRepeat(event);
	}
	try {
		// ignore return value
		commandController.executeCommand(info.command);
	} catch (CommandException& e) {
		commandController.getCliComm().printWarning(
			"Error executing hot key command: " + e.getMessage());
	}
	return EventDistributor::MSX; // deny event to MSX listeners
}

void HotKey::startRepeat(const EventPtr& event)
{
	// I initially thought about using the builtin SDL key-repeat feature,
	// but that won't work for example on joystick buttons. So we have to
	// code it ourselves.
	unsigned delay = (lastEvent.get() ? 30 : 500) * 1000;
	lastEvent = event;
	repeatAlarm->schedule(delay);
}

void HotKey::stopRepeat()
{
	lastEvent.reset();
	repeatAlarm->cancel();
}


// class BindCmd

static string getBindCmdName(bool defaultCmd)
{
	return defaultCmd ? "bind_default" : "bind";
}

BindCmd::BindCmd(CommandController& commandController, HotKey& hotKey_,
                 bool defaultCmd_)
	: Command(commandController, getBindCmdName(defaultCmd_))
	, hotKey(hotKey_)
	, defaultCmd(defaultCmd_)
{
}

string BindCmd::formatBinding(HotKey::BindMap::const_iterator it)
{
	const HotKey::HotKeyInfo& info = it->second;
	return it->first->toString() + (info.repeat ? " [repeat]" : "") +
	       ":  " + info.command + '\n';
}

string BindCmd::execute(const vector<string>& tokens)
{
	HotKey::BindMap& cmdMap = defaultCmd ? hotKey.defaultMap
	                                     : hotKey.cmdMap;
	string result;
	switch (tokens.size()) {
	case 0:
		UNREACHABLE;
	case 1:
		// show all bounded keys
		for (HotKey::BindMap::const_iterator it = cmdMap.begin();
		     it != cmdMap.end(); ++it) {
			result += formatBinding(it);
		}
		break;
	case 2: {
		// show bindings for this key
		HotKey::BindMap::const_iterator it =
			cmdMap.find(createEvent(tokens[1]));
		if (it == cmdMap.end()) {
			throw CommandException("Key not bound");
		}
		result = formatBinding(it);
		break;
	}
	default: {
		// make a new binding
		string command;
		bool repeat = false;
		unsigned start = 2;
		if (tokens[2] == "-repeat") {
			repeat = true;
			++start;
		}
		for (unsigned i = start; i < tokens.size(); ++i) {
			if (i != start) command += ' ';
			command += tokens[i];
		}
		HotKey::HotKeyInfo info(command, repeat);
		if (defaultCmd) {
			hotKey.bindDefault(createEvent(tokens[1]), info);
		} else {
			hotKey.bind       (createEvent(tokens[1]), info);
		}
		break;
	}
	}
	return result;
}
string BindCmd::help(const vector<string>& /*tokens*/) const
{
	string cmd = getBindCmdName(defaultCmd);
	return cmd + "                       : show all bounded keys\n" +
	       cmd + " <key>                 : show binding for this key\n" +
	       cmd + " <key> [-repeat] <cmd> : bind key to command, optionally repeat command while key remains pressed\n";
}


// class UnbindCmd

static string getUnbindCmdName(bool defaultCmd)
{
	return defaultCmd ? "unbind_default" : "unbind";
}

UnbindCmd::UnbindCmd(CommandController& commandController,
                     HotKey& hotKey_, bool defaultCmd_)
	: Command(commandController, getUnbindCmdName(defaultCmd_))
	, hotKey(hotKey_)
	, defaultCmd(defaultCmd_)
{
}

string UnbindCmd::execute(const vector<string>& tokens)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	if (defaultCmd) {
		hotKey.unbindDefault(createEvent(tokens[1]));
	} else {
		hotKey.unbind       (createEvent(tokens[1]));
	}
	return "";
}
string UnbindCmd::help(const vector<string>& /*tokens*/) const
{
	string cmd = getUnbindCmdName(defaultCmd);
	return cmd + " <key> : unbind this key\n";
}

} // namespace openmsx
