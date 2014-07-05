#include "HotKey.hh"
#include "InputEventFactory.hh"
#include "GlobalCommandController.hh"
#include "Command.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "InputEvents.hh"
#include "XMLElement.hh"
#include "TclObject.hh"
#include "SettingsConfig.hh"
#include "AlarmEvent.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <algorithm>
#include <cassert>

using std::string;
using std::vector;
using std::make_shared;

// This file implements all Tcl key bindings. These are the 'classical' hotkeys
// (e.g. F11 to (un)mute sound) and the more recent input layers. The idea
// behind an input layer is something like an OSD widget that (temporarily)
// takes semi-exclusive access to the input. So while the widget is active
// keyboard (and joystick) input is no longer passed to the emulated MSX.
// However the classical hotkeys or the openMSX console still receive input.

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
	virtual void execute(array_ref<TclObject> tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	string formatBinding(const HotKey::BindMap::value_type& p);

	HotKey& hotKey;
	const bool defaultCmd;
};

class UnbindCmd : public Command
{
public:
	UnbindCmd(CommandController& commandController, HotKey& hotKey,
	          bool defaultCmd);
	virtual void execute(array_ref<TclObject> tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
	const bool defaultCmd;
};

class ActivateCmd : public Command
{
public:
	ActivateCmd(CommandController& commandController, HotKey& hotKey);
	virtual void execute(array_ref<TclObject> tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
};

class DeactivateCmd : public Command
{
public:
	DeactivateCmd(CommandController& commandController, HotKey& hotKey);
	virtual void execute(array_ref<TclObject> tokens, TclObject& result);
	virtual string help(const vector<string>& tokens) const;
private:
	HotKey& hotKey;
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
	, activateCmd(make_unique<ActivateCmd>(
		commandController_, *this))
	, deactivateCmd(make_unique<DeactivateCmd>(
		commandController_, *this))
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
		OPENMSX_MOUSE_MOTION_EVENT, *this, EventDistributor::HOTKEY);
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
	eventDistributor.registerEventListener(
		OPENMSX_OSD_CONTROL_RELEASE_EVENT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		OPENMSX_OSD_CONTROL_PRESS_EVENT, *this, EventDistributor::HOTKEY);
}

HotKey::~HotKey()
{
	eventDistributor.unregisterEventListener(OPENMSX_OSD_CONTROL_PRESS_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_OSD_CONTROL_RELEASE_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_BUTTON_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_JOY_AXIS_MOTION_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_MOUSE_BUTTON_UP_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_MOUSE_BUTTON_DOWN_EVENT, *this);
	eventDistributor.unregisterEventListener(OPENMSX_MOUSE_MOTION_EVENT, *this);
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
#if PLATFORM_ANDROID
		// The follwing binding is specific for Android, in order
		// to remap the android back button to an SDL KEY event.
		// I could have put all Android key bindings in a separate
		// else(...) clause. However, an Android user might actually
		// be using a PC keyboard (through USB or Bluetooth) and in such
		// case expect all default PC keybindings to exist as well
		bindDefault(make_shared<KeyDownEvent>(Keys::K_WORLD_92),
		            HotKeyInfo("quitmenu::quit_menu"));
#endif
	}
}

static HotKey::EventPtr createEvent(const string& str)
{
	auto event = InputEventFactory::createInputEvent(str);
	if (!dynamic_cast<const KeyEvent*>             (event.get()) &&
	    !dynamic_cast<const MouseButtonEvent*>     (event.get()) &&
	    !dynamic_cast<const MouseMotionGroupEvent*>(event.get()) &&
	    !dynamic_cast<const JoystickEvent*>        (event.get()) &&
	    !dynamic_cast<const OsdControlEvent*>      (event.get()) &&
	    !dynamic_cast<const FocusEvent*>           (event.get())) {
		throw CommandException("Unsupported event type");
	}
	return event;
}

void HotKey::loadBindings(const XMLElement& config)
{
	// restore default bindings
	unboundKeys.clear();
	boundKeys.clear();
	cmdMap = defaultMap;

	// load bindings
	auto* bindingsElement = config.findChild("bindings");
	if (!bindingsElement) return;
	auto copy = *bindingsElement; // dont iterate over changing container
	for (auto& elem : copy.getChildren()) {
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
	auto& bindingsElement = config.getCreateChild("bindings");
	bindingsElement.removeAllChildren();

	// add explicit bind's
	for (auto& k : boundKeys) {
		auto it2 = cmdMap.find(k);
		assert(it2 != end(cmdMap));
		auto& info = it2->second;
		auto& elem = bindingsElement.addChild("bind", info.command);
		elem.addAttribute("key", k->toString());
		if (info.repeat) {
			elem.addAttribute("repeat", "true");
		}
	}
	// add explicit unbind's
	for (auto& k : unboundKeys) {
		auto& elem = bindingsElement.addChild("unbind");
		elem.addAttribute("key", k->toString());
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
	if (boundKeys.find(event) == end(boundKeys)) {
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
	if ((unboundKeys.find(event) == end(unboundKeys)) &&
	    (boundKeys.find(event)   == end(boundKeys))) {
		// not explicity bound or unbound
		cmdMap[event] = info;
	}
	defaultMap[event] = info;
}

void HotKey::unbindDefault(const EventPtr& event)
{
	if ((unboundKeys.find(event) == end(unboundKeys)) &&
	    (boundKeys.find(event)   == end(boundKeys))) {
		// not explicity bound or unbound
		cmdMap.erase(event);
	}
	defaultMap.erase(event);
}

void HotKey::bindLayer(const EventPtr& event, const HotKeyInfo& info,
                       const string& layer)
{
	layerMap[layer][event] = info;
}

void HotKey::unbindLayer(const EventPtr& event, const string& layer)
{
	layerMap[layer].erase(event);
}

void HotKey::unbindFullLayer(const string& layer)
{
	layerMap.erase(layer);
}

void HotKey::activateLayer(std::string layer, bool blocking)
{
	// Insert new activattion record at the end of the list.
	// (it's not an error if the same layer was already active, in such
	// as case it will now appear twice in the list of active layer,
	// and it must also be deactivated twice).
	activeLayers.push_back({std::move(layer), blocking});
}

void HotKey::deactivateLayer(string_ref layer)
{
	// remove the first matching activation record from the end
	// (it's not an error if there is no match at all)
	auto it = std::find_if(activeLayers.rbegin(), activeLayers.rend(),
		[&](const LayerInfo& info) { return info.layer == layer; });
	if (it != activeLayers.rend()) {
		// 'reverse_iterator' -> 'iterator' conversion is a bit tricky
		activeLayers.erase((it + 1).base());
	}
}

static HotKey::BindMap::const_iterator findMatch(
	const HotKey::BindMap& map, const Event& event)
{
	return find_if(begin(map), end(map),
		[&](const HotKey::BindMap::value_type& p) {
			return p.first->matches(event); });
}

int HotKey::signalEvent(const EventPtr& event_)
{
	// Convert special 'repeat' event into the actual to-be-repeated event.
	EventPtr event = event_;
	if (event->getType() == OPENMSX_REPEAT_HOTKEY) {
		if (!lastEvent) return 0;
		event = lastEvent;
	} else if (lastEvent != event) {
		// If the newly received event is different from the repeating
		// event, we stop the repeat process.
		// Except when we're repeating a OsdControlEvent and the
		// received event was actually the 'generating' event for the
		// Osd event. E.g. a cursor-keyboard-down event will generate
		// a corresponding osd event (the osd event is send before the
		// original event). Without this hack, key-repeat will not work
		// for osd key bindings.
		if (lastEvent && lastEvent->isRepeatStopper(*event)) {
			stopRepeat();
		}
	}

	// First search in active layers (from back to front)
	bool blocking = false;
	for (auto it = activeLayers.rbegin(); it != activeLayers.rend(); ++it) {
		auto& cmap = layerMap[it->layer]; // ok, if this entry doesn't exist yet
		auto it2 = findMatch(cmap, *event);
		if (it2 != end(cmap)) {
			executeBinding(event, it2->second);
			// Deny event to MSX listeners, also don't pass event
			// to other layers (including the default layer).
			return EventDistributor::MSX;
		}
		blocking = it->blocking;
		if (blocking) break; // don't try lower layers
	}

	// If the event was not yet handled, try the default layer.
	auto it = findMatch(cmdMap, *event);
	if (it != end(cmdMap)) {
		executeBinding(event, it->second);
		return EventDistributor::MSX; // deny event to MSX listeners
	}

	// Event is not handled, only let it pass to the MSX if there was no
	// blocking layer active.
	return blocking ? EventDistributor::MSX : 0;
}

void HotKey::executeBinding(const EventPtr& event, const HotKeyInfo& info)
{
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
}

void HotKey::startRepeat(const EventPtr& event)
{
	// I initially thought about using the builtin SDL key-repeat feature,
	// but that won't work for example on joystick buttons. So we have to
	// code it ourselves.

	// On android, because of the sensitivity of the touch screen it's
	// very hard to have touches of short durations. So half a second is
	// too short for the key-repeat-delay. A full second should be fine.
	static const unsigned DELAY = PLATFORM_ANDROID ? 1000 : 500;
	// Repeat period.
	static const unsigned PERIOD = 30;

	unsigned delay = (lastEvent ? PERIOD : DELAY) * 1000;
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

string BindCmd::formatBinding(const HotKey::BindMap::value_type& p)
{
	auto& info = p.second;
	return p.first->toString() + (info.repeat ? " [repeat]" : "") +
	       ":  " + info.command + '\n';
}

static vector<TclObject> parse(bool defaultCmd, array_ref<TclObject> tokens_,
                               string& layer, bool& layers)
{
	layers = false;
	vector<TclObject> tokens(std::begin(tokens_) + 1, std::end(tokens_));
	for (size_t i = 0; i < tokens.size(); /**/) {
		if (tokens[i].getString() == "-layer") {
			if (i == (tokens.size() - 1)) {
				throw CommandException("Missing layer name");
			}
			if (defaultCmd) {
				throw CommandException(
					"Layers are not supported for default bindings");
			}
			layer = tokens[i + 1].getString().str();

			auto it = begin(tokens) + i;
			tokens.erase(it, it + 2);
		} else if (tokens[i].getString() == "-layers") {
			layers = true;
			tokens.erase(begin(tokens) + i);
		} else {
			++i;
		}
	}
	return tokens;
}

void BindCmd::execute(array_ref<TclObject> tokens_, TclObject& result)
{
	string layer;
	bool layers;
	auto tokens = parse(defaultCmd, tokens_, layer, layers);

	auto& cmdMap = defaultCmd
		? hotKey.defaultMap
		: layer.empty() ? hotKey.cmdMap
		                : hotKey.layerMap[layer];

	if (layers) {
		for (auto& p : hotKey.layerMap) {
			// An alternative for this test is to always properly
			// prune layerMap. ATM this approach seems simpler.
			if (!p.second.empty()) {
				result.addListElement(p.first);
			}
		}
		return;
	}

	switch (tokens.size()) {
	case 0: {
		// show all bounded keys (for this layer)
		string r;
		for (auto& p : cmdMap) {
			r += formatBinding(p);
		}
		result.setString(r);
		break;
	}
	case 1: {
		// show bindings for this key (in this layer)
		auto it = cmdMap.find(createEvent(tokens[0].getString().str()));
		if (it == end(cmdMap)) {
			throw CommandException("Key not bound");
		}
		result.setString(formatBinding(*it));
		break;
	}
	default: {
		// make a new binding
		string command;
		bool repeat = false;
		unsigned start = 1;
		if (tokens[1].getString() == "-repeat") {
			repeat = true;
			++start;
		}
		for (unsigned i = start; i < tokens.size(); ++i) {
			if (i != start) command += ' ';
			string_ref t = tokens[i].getString();
			command.append(t.data(), t.size());
		}
		HotKey::HotKeyInfo info(command, repeat);
		auto event = createEvent(tokens[0].getString().str());
		if (defaultCmd) {
			hotKey.bindDefault(event, info);
		} else if (layer.empty()) {
			hotKey.bind(event, info);
		} else {
			hotKey.bindLayer(event, info, layer);
		}
		break;
	}
	}
}
string BindCmd::help(const vector<string>& /*tokens*/) const
{
	string cmd = getBindCmdName(defaultCmd);
	return cmd + "                       : show all bounded keys\n" +
	       cmd + " <key>                 : show binding for this key\n" +
	       cmd + " <key> [-repeat] <cmd> : bind key to command, optionally "
	       "repeat command while key remains pressed\n"
	       "These 3 take an optional '-layer <layername>' option, "
	       "see activate_input_layer." +
	       cmd + " -layers               : show a list of layers with bound keys\n";
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

void UnbindCmd::execute(array_ref<TclObject> tokens_, TclObject& /*result*/)
{
	string layer;
	bool layers;
	auto tokens = parse(defaultCmd, tokens_, layer, layers);

	if (layers) {
		throw SyntaxError();
	}
	if ((tokens.size() > 1) || (layer.empty() && (tokens.size() != 1))) {
		throw SyntaxError();
	}

	HotKey::EventPtr event;
	if (tokens.size() == 1) {
		event = createEvent(tokens[0].getString().str());
	}

	if (defaultCmd) {
		assert(event);
		hotKey.unbindDefault(event);
	} else if (layer.empty()) {
		assert(event);
		hotKey.unbind(event);
	} else {
		if (event) {
			hotKey.unbindLayer(event, layer);
		} else {
			hotKey.unbindFullLayer(layer);
		}
	}
}
string UnbindCmd::help(const vector<string>& /*tokens*/) const
{
	string cmd = getUnbindCmdName(defaultCmd);
	return cmd + " <key>                    : unbind this key\n" +
	       cmd + " -layer <layername> <key> : unbind key in a specific layer\n" +
	       cmd + " -layer <layername>       : unbind all keys in this layer\n";
}


// class ActivateCmd

ActivateCmd::ActivateCmd(CommandController& commandController, HotKey& hotKey_)
	: Command(commandController, "activate_input_layer")
	, hotKey(hotKey_)
{
}

void ActivateCmd::execute(array_ref<TclObject> tokens, TclObject& result)
{
	string_ref layer;
	bool blocking = false;
	for (size_t i = 1; i < tokens.size(); ++i) {
		if (tokens[i].getString() == "-blocking") {
			blocking = true;
		} else {
			if (!layer.empty()) {
				throw SyntaxError();
			}
			layer = tokens[i].getString();
		}
	}

	string r;
	if (layer.empty()) {
		for (auto it = hotKey.activeLayers.rbegin();
		     it != hotKey.activeLayers.rend(); ++it) {
			r += it->layer;
			if (it->blocking) {
				r += " -blocking";
			}
			r += '\n';
		}
	} else {
		hotKey.activateLayer(layer.str(), blocking);
	}
	result.setString(r);
}

string ActivateCmd::help(const vector<string>& /*tokens*/) const
{
	return "activate_input_layer                         "
	       ": show list of active layers (most recent on top)\n"
	       "activate_input_layer [-blocking] <layername> "
	       ": activate new layer, optionally in blocking mode\n";
}


// class DeactivateCmd

DeactivateCmd::DeactivateCmd(CommandController& commandController, HotKey& hotKey_)
	: Command(commandController, "deactivate_input_layer")
	, hotKey(hotKey_)
{
}

void DeactivateCmd::execute(array_ref<TclObject> tokens, TclObject& /*result*/)
{
	if (tokens.size() != 2) {
		throw SyntaxError();
	}
	hotKey.deactivateLayer(tokens[1].getString());
}

string DeactivateCmd::help(const vector<string>& /*tokens*/) const
{
	return "deactivate_input_layer <layername> : deactive the given input layer";
}


} // namespace openmsx
