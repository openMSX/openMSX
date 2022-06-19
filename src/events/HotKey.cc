#include "HotKey.hh"
#include "InputEventFactory.hh"
#include "GlobalCommandController.hh"
#include "CommandException.hh"
#include "EventDistributor.hh"
#include "CliComm.hh"
#include "Event.hh"
#include "TclArgParser.hh"
#include "TclObject.hh"
#include "SettingsConfig.hh"
#include "one_of.hh"
#include "outer.hh"
#include "ranges.hh"
#include "view.hh"
#include "build-info.hh"
#include <cassert>
#include <memory>

using std::string;

// This file implements all Tcl key bindings. These are the 'classical' hotkeys
// (e.g. F12 to (un)mute sound) and the more recent input layers. The idea
// behind an input layer is something like an OSD widget that (temporarily)
// takes semi-exclusive access to the input. So while the widget is active
// keyboard (and joystick) input is no longer passed to the emulated MSX.
// However the classical hotkeys or the openMSX console still receive input.

namespace openmsx {

constexpr bool META_HOT_KEYS =
#ifdef __APPLE__
	true;
#else
	false;
#endif

HotKey::HotKey(RTScheduler& rtScheduler,
               GlobalCommandController& commandController_,
               EventDistributor& eventDistributor_)
	: RTSchedulable(rtScheduler)
	, bindCmd         (commandController_, *this, false)
	, bindDefaultCmd  (commandController_, *this, true)
	, unbindCmd       (commandController_, *this, false)
	, unbindDefaultCmd(commandController_, *this, true)
	, activateCmd     (commandController_)
	, deactivateCmd   (commandController_)
	, commandController(commandController_)
	, eventDistributor(eventDistributor_)
{
	initDefaultBindings();

	eventDistributor.registerEventListener(
		EventType::KEY_DOWN, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::KEY_UP, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::MOUSE_MOTION, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::MOUSE_BUTTON_DOWN, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::MOUSE_BUTTON_UP, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::MOUSE_WHEEL, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::JOY_BUTTON_DOWN, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::JOY_BUTTON_UP, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::JOY_AXIS_MOTION, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::JOY_HAT, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::FOCUS, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::FILEDROP, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::OSD_CONTROL_RELEASE, *this, EventDistributor::HOTKEY);
	eventDistributor.registerEventListener(
		EventType::OSD_CONTROL_PRESS, *this, EventDistributor::HOTKEY);
}

HotKey::~HotKey()
{
	eventDistributor.unregisterEventListener(EventType::OSD_CONTROL_PRESS, *this);
	eventDistributor.unregisterEventListener(EventType::OSD_CONTROL_RELEASE, *this);
	eventDistributor.unregisterEventListener(EventType::FILEDROP, *this);
	eventDistributor.unregisterEventListener(EventType::FOCUS, *this);
	eventDistributor.unregisterEventListener(EventType::JOY_BUTTON_UP, *this);
	eventDistributor.unregisterEventListener(EventType::JOY_BUTTON_DOWN, *this);
	eventDistributor.unregisterEventListener(EventType::JOY_AXIS_MOTION, *this);
	eventDistributor.unregisterEventListener(EventType::JOY_HAT, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_WHEEL, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_BUTTON_UP, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_BUTTON_DOWN, *this);
	eventDistributor.unregisterEventListener(EventType::MOUSE_MOTION, *this);
	eventDistributor.unregisterEventListener(EventType::KEY_UP, *this);
	eventDistributor.unregisterEventListener(EventType::KEY_DOWN, *this);
}

void HotKey::initDefaultBindings()
{
	// TODO move to Tcl script?

	if constexpr (META_HOT_KEYS) {
		// Hot key combos using Mac's Command key.
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_D, Keys::KM_META)),
		                       "screenshot -guess-name"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_P, Keys::KM_META)),
		                       "toggle pause"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_T, Keys::KM_META)),
		                       "toggle fastforward"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_L, Keys::KM_META)),
		                       "toggle console"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_U, Keys::KM_META)),
		                       "toggle mute"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_F, Keys::KM_META)),
		                       "toggle fullscreen"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_Q, Keys::KM_META)),
		                       "exit"));
	} else {
		// Hot key combos for typical PC keyboards.
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(Keys::K_PRINT),
		                       "screenshot -guess-name"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(Keys::K_PAUSE),
		                       "toggle pause"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(Keys::K_F9),
		                       "toggle fastforward"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(Keys::K_F10),
		                       "toggle console"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(Keys::K_F11),
		                       "toggle fullscreen"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(Keys::K_F12),
		                       "toggle mute"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_F4, Keys::KM_ALT)),
		                       "exit"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_PAUSE, Keys::KM_CTRL)),
		                       "exit"));
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(
		                            Keys::combine(Keys::K_RETURN, Keys::KM_ALT)),
		                       "toggle fullscreen"));
		// and for Android
		bindDefault(HotKeyInfo(Event::create<KeyDownEvent>(Keys::K_BACK),
		                       "quitmenu::quit_menu"));
	}
}

static Event createEvent(const TclObject& obj, Interpreter& interp)
{
	auto event = InputEventFactory::createInputEvent(obj, interp);
	if (getType(event) != one_of(EventType::KEY_UP, EventType::KEY_DOWN,
	                             EventType::MOUSE_BUTTON_UP, EventType::MOUSE_BUTTON_DOWN,
				     EventType::GROUP,
				     EventType::JOY_BUTTON_UP, EventType::JOY_BUTTON_DOWN,
				     EventType::JOY_AXIS_MOTION, EventType::JOY_HAT,
				     EventType::OSD_CONTROL_PRESS, EventType::OSD_CONTROL_RELEASE,
				     EventType::FOCUS)) {
		throw CommandException("Unsupported event type");
	}
	return event;
}
static Event createEvent(std::string_view str, Interpreter& interp)
{
	return createEvent(TclObject(str), interp);
}

void HotKey::loadInit()
{
	// restore default bindings
	unboundKeys.clear();
	boundKeys.clear();
	cmdMap = defaultMap;
}

void HotKey::loadBind(std::string_view key, std::string_view cmd, bool repeat, bool event)
{
	bind(HotKeyInfo(createEvent(key, commandController.getInterpreter()),
			std::string(cmd), repeat, event));
}

void HotKey::loadUnbind(std::string_view key)
{
	unbind(createEvent(key, commandController.getInterpreter()));
}

struct EqualEvent {
	EqualEvent(const Event& event_) : event(event_) {}
	bool operator()(const Event& e) const {
		return event == e;
	}
	bool operator()(const HotKey::HotKeyInfo& info) const {
		return event == info.event;
	}
	const Event& event;
};

static bool contains(auto&& range, const Event& event)
{
	return ranges::any_of(range, EqualEvent(event));
}

template<typename T>
static void erase(std::vector<T>& v, const Event& event)
{
	if (auto it = ranges::find_if(v, EqualEvent(event)); it != end(v)) {
		move_pop_back(v, it);
	}
}

static void insert(HotKey::KeySet& set, const Event& event)
{
	if (auto it = ranges::find_if(set, EqualEvent(event)); it != end(set)) {
		*it = event;
	} else {
		set.push_back(event);
	}
}

template<typename HotKeyInfo>
static void insert(HotKey::BindMap& map, HotKeyInfo&& info)
{
	if (auto it = ranges::find_if(map, EqualEvent(info.event)); it != end(map)) {
		*it = std::forward<HotKeyInfo>(info);
	} else {
		map.push_back(std::forward<HotKeyInfo>(info));
	}
}

void HotKey::bind(HotKeyInfo&& info)
{
	erase(unboundKeys, info.event);
	erase(defaultMap, info.event);
	insert(boundKeys, info.event);
	insert(cmdMap, std::move(info));
}

void HotKey::unbind(const Event& event)
{
	if (auto it1 = ranges::find_if(boundKeys, EqualEvent(event));
	    it1 == end(boundKeys)) {
		// only when not a regular bound event
		insert(unboundKeys, event);
	} else {
		//erase(boundKeys, *event);
		move_pop_back(boundKeys, it1);
	}

	erase(defaultMap, event);
	erase(cmdMap, event);
}

void HotKey::bindDefault(HotKeyInfo&& info)
{
	if (!contains(  boundKeys, info.event) &&
	    !contains(unboundKeys, info.event)) {
		// not explicitly bound or unbound
		insert(cmdMap, info);
	}
	insert(defaultMap, std::move(info));
}

void HotKey::unbindDefault(const Event& event)
{
	if (!contains(  boundKeys, event) &&
	    !contains(unboundKeys, event)) {
		// not explicitly bound or unbound
		erase(cmdMap, event);
	}
	erase(defaultMap, event);
}

void HotKey::bindLayer(HotKeyInfo&& info, const string& layer)
{
	insert(layerMap[layer], std::move(info));
}

void HotKey::unbindLayer(const Event& event, const string& layer)
{
	erase(layerMap[layer], event);
}

void HotKey::unbindFullLayer(const string& layer)
{
	layerMap.erase(layer);
}

void HotKey::activateLayer(std::string layer, bool blocking)
{
	// Insert new activation record at the end of the list.
	// (it's not an error if the same layer was already active, in such
	// as case it will now appear twice in the list of active layer,
	// and it must also be deactivated twice).
	activeLayers.push_back({std::move(layer), blocking});
}

void HotKey::deactivateLayer(std::string_view layer)
{
	// remove the first matching activation record from the end
	// (it's not an error if there is no match at all)
	if (auto it = ranges::find(view::reverse(activeLayers), layer, &LayerInfo::layer);
	    it != activeLayers.rend()) {
		// 'reverse_iterator' -> 'iterator' conversion is a bit tricky
		activeLayers.erase((it + 1).base());
	}
}

static HotKey::BindMap::const_iterator findMatch(
	const HotKey::BindMap& map, const Event& event)
{
	return ranges::find_if(map, [&](auto& p) {
		return matches(p.event, event);
	});
}

void HotKey::executeRT()
{
	if (lastEvent) executeEvent(lastEvent);
}

int HotKey::signalEvent(const Event& event) noexcept
{
	if (lastEvent.getPtr() != event.getPtr()) {
		// If the newly received event is different from the repeating
		// event, we stop the repeat process.
		// Except when we're repeating a OsdControlEvent and the
		// received event was actually the 'generating' event for the
		// Osd event. E.g. a cursor-keyboard-down event will generate
		// a corresponding osd event (the osd event is send before the
		// original event). Without this hack, key-repeat will not work
		// for osd key bindings.
		if (lastEvent && isRepeatStopper(lastEvent, event)) {
			stopRepeat();
		}
	}
	return executeEvent(event);
}

int HotKey::executeEvent(const Event& event)
{
	// First search in active layers (from back to front)
	bool blocking = false;
	for (auto& info : view::reverse(activeLayers)) {
		auto& cmap = layerMap[info.layer]; // ok, if this entry doesn't exist yet
		if (auto it = findMatch(cmap, event); it != end(cmap)) {
			executeBinding(event, *it);
			// Deny event to MSX listeners, also don't pass event
			// to other layers (including the default layer).
			return EventDistributor::MSX;
		}
		blocking = info.blocking;
		if (blocking) break; // don't try lower layers
	}

	// If the event was not yet handled, try the default layer.
	if (auto it = findMatch(cmdMap, event); it != end(cmdMap)) {
		executeBinding(event, *it);
		return EventDistributor::MSX; // deny event to MSX listeners
	}

	// Event is not handled, only let it pass to the MSX if there was no
	// blocking layer active.
	return blocking ? EventDistributor::MSX : 0;
}

void HotKey::executeBinding(const Event& event, const HotKeyInfo& info)
{
	if (info.repeat) {
		startRepeat(event);
	}
	try {
		// Make a copy of the command string because executing the
		// command could potentially execute (un)bind commands so
		// that the original string becomes invalid.
		// Valgrind complained about this in the following scenario:
		//  - open the OSD menu
		//  - activate the 'Exit openMSX' item
		// The latter is triggered from e.g. a 'OSDControl A PRESS'
		// event. The Tcl script bound to that event closes the main
		// menu and reopens a new quit_menu. This will re-bind the
		// action for the 'OSDControl A PRESS' event.
		TclObject command(info.command);
		if (info.passEvent) {
			// Add event as the last argument to the command.
			// (If needed) the current command string is first
			// converted to a list (thus split in a command name
			// and arguments).
			command.addListElement(toTclList(event));
		}

		// ignore return value
		command.executeCommand(commandController.getInterpreter());
	} catch (CommandException& e) {
		commandController.getCliComm().printWarning(
			"Error executing hot key command: ", e.getMessage());
	}
}

void HotKey::startRepeat(const Event& event)
{
	// I initially thought about using the builtin SDL key-repeat feature,
	// but that won't work for example on joystick buttons. So we have to
	// code it ourselves.

	// On android, because of the sensitivity of the touch screen it's
	// very hard to have touches of short durations. So half a second is
	// too short for the key-repeat-delay. A full second should be fine.
	static constexpr unsigned DELAY = PLATFORM_ANDROID ? 1000 : 500;
	// Repeat period.
	static constexpr unsigned PERIOD = 30;

	unsigned delay = (lastEvent ? PERIOD : DELAY) * 1000;
	lastEvent = event;
	scheduleRT(delay);
}

void HotKey::stopRepeat()
{
	lastEvent = Event{};
	cancelRT();
}


// class BindCmd

static constexpr std::string_view getBindCmdName(bool defaultCmd)
{
	return defaultCmd ? "bind_default" : "bind";
}

HotKey::BindCmd::BindCmd(CommandController& commandController_, HotKey& hotKey_,
                         bool defaultCmd_)
	: Command(commandController_, getBindCmdName(defaultCmd_))
	, hotKey(hotKey_)
	, defaultCmd(defaultCmd_)
{
}

static string formatBinding(const HotKey::HotKeyInfo& info)
{
	return strCat(toString(info.event), (info.repeat ? " [repeat]" : ""),
	              (info.passEvent ? " [event]" : ""), ":  ", info.command, '\n');
}

void HotKey::BindCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	string layer;
	bool layers = false;
	bool repeat = false;
	bool passEvent = false;
	ArgsInfo parserInfo[] = {
		valueArg("-layer", layer),
		flagArg("-layers", layers),
		flagArg("-repeat", repeat),
		flagArg("-event", passEvent),
	};
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan<1>(), parserInfo);
	if (defaultCmd && !layer.empty()) {
		throw CommandException("Layers are not supported for default bindings");
	}

	auto& cMap = defaultCmd
		? hotKey.defaultMap
		: layer.empty() ? hotKey.cmdMap
		                : hotKey.layerMap[layer];

	if (layers) {
		for (const auto& [layerName, bindings] : hotKey.layerMap) {
			// An alternative for this test is to always properly
			// prune layerMap. ATM this approach seems simpler.
			if (!bindings.empty()) {
				result.addListElement(layerName);
			}
		}
		return;
	}

	switch (arguments.size()) {
	case 0: {
		// show all bounded keys (for this layer)
		string r;
		for (auto& p : cMap) {
			r += formatBinding(p);
		}
		result = r;
		break;
	}
	case 1: {
		// show bindings for this key (in this layer)
		auto it = ranges::find_if(cMap,
		                          EqualEvent(createEvent(arguments[0], getInterpreter())));
		if (it == end(cMap)) {
			throw CommandException("Key not bound");
		}
		result = formatBinding(*it);
		break;
	}
	default: {
		// make a new binding
		string command(arguments[1].getString());
		for (const auto& arg : view::drop(arguments, 2)) {
			strAppend(command, ' ', arg.getString());
		}
		HotKey::HotKeyInfo info(
			createEvent(arguments[0], getInterpreter()),
			command, repeat, passEvent);
		if (defaultCmd) {
			hotKey.bindDefault(std::move(info));
		} else if (layer.empty()) {
			hotKey.bind(std::move(info));
		} else {
			hotKey.bindLayer(std::move(info), layer);
		}
		break;
	}
	}
}
string HotKey::BindCmd::help(std::span<const TclObject> /*tokens*/) const
{
	auto cmd = getBindCmdName(defaultCmd);
	return strCat(
		cmd, "                       : show all bounded keys\n",
		cmd, " <key>                 : show binding for this key\n",
		cmd, " <key> [-repeat] [-event] <cmd> : bind key to command, optionally "
		"repeat command while key remains pressed and also optionally "
		"give back the event as argument (a list) to <cmd>\n"
		"These 3 take an optional '-layer <layername>' option, "
		"see activate_input_layer.\n",
		cmd, " -layers               : show a list of layers with bound keys\n");
}


// class UnbindCmd

static constexpr std::string_view getUnbindCmdName(bool defaultCmd)
{
	return defaultCmd ? "unbind_default" : "unbind";
}

HotKey::UnbindCmd::UnbindCmd(CommandController& commandController_,
                             HotKey& hotKey_, bool defaultCmd_)
	: Command(commandController_, getUnbindCmdName(defaultCmd_))
	, hotKey(hotKey_)
	, defaultCmd(defaultCmd_)
{
}

void HotKey::UnbindCmd::execute(std::span<const TclObject> tokens, TclObject& /*result*/)
{
	string layer;
	ArgsInfo info[] = { valueArg("-layer", layer) };
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan<1>(), info);
	if (defaultCmd && !layer.empty()) {
		throw CommandException("Layers are not supported for default bindings");
	}

	if ((arguments.size() > 1) || (layer.empty() && (arguments.size() != 1))) {
		throw SyntaxError();
	}

	Event event;
	if (arguments.size() == 1) {
		event = createEvent(arguments[0], getInterpreter());
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
string HotKey::UnbindCmd::help(std::span<const TclObject> /*tokens*/) const
{
	auto cmd = getUnbindCmdName(defaultCmd);
	return strCat(
		cmd, " <key>                    : unbind this key\n",
		cmd, " -layer <layername> <key> : unbind key in a specific layer\n",
		cmd, " -layer <layername>       : unbind all keys in this layer\n");
}


// class ActivateCmd

HotKey::ActivateCmd::ActivateCmd(CommandController& commandController_)
	: Command(commandController_, "activate_input_layer")
{
}

void HotKey::ActivateCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	bool blocking = false;
	ArgsInfo info[] = { flagArg("-blocking", blocking) };
	auto args = parseTclArgs(getInterpreter(), tokens.subspan(1), info);

	auto& hotKey = OUTER(HotKey, activateCmd);
	switch (args.size()) {
	case 0: {
		string r;
		for (auto& layerInfo : view::reverse(hotKey.activeLayers)) {
			r += layerInfo.layer;
			if (layerInfo.blocking) {
				r += " -blocking";
			}
			r += '\n';
		}
		result = r;
		break;
	}
	case 1: {
		std::string_view layer = args[0].getString();
		hotKey.activateLayer(string(layer), blocking);
		break;
	}
	default:
		throw SyntaxError();
	}
}

string HotKey::ActivateCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "activate_input_layer                         "
	       ": show list of active layers (most recent on top)\n"
	       "activate_input_layer [-blocking] <layername> "
	       ": activate new layer, optionally in blocking mode\n";
}


// class DeactivateCmd

HotKey::DeactivateCmd::DeactivateCmd(CommandController& commandController_)
	: Command(commandController_, "deactivate_input_layer")
{
}

void HotKey::DeactivateCmd::execute(std::span<const TclObject> tokens, TclObject& /*result*/)
{
	checkNumArgs(tokens, 2, "layer");
	auto& hotKey = OUTER(HotKey, deactivateCmd);
	hotKey.deactivateLayer(tokens[1].getString());
}

string HotKey::DeactivateCmd::help(std::span<const TclObject> /*tokens*/) const
{
	return "deactivate_input_layer <layername> : deactivate the given input layer";
}


} // namespace openmsx
