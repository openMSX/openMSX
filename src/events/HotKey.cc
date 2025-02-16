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

#include "build-info.hh"

#include <algorithm>
#include <array>
#include <cassert>
#include <ranges>

using std::string;

// This file implements all Tcl key bindings. These are the 'classical' hotkeys
// (e.g. F12 to (un)mute sound) and the more recent input layers. The idea
// behind an input layer is something like an OSD widget that (temporarily)
// takes semi-exclusive access to the input. So while the widget is active
// keyboard (and joystick) input is no longer passed to the emulated MSX.
// However the classical hotkeys or the openMSX console still receive input.

namespace openmsx {

static constexpr bool META_HOT_KEYS =
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
	, listenerHigh(*this, EventDistributor::Priority::HOTKEY_HIGH)
	, listenerLow (*this, EventDistributor::Priority::HOTKEY_LOW)
{
	initDefaultBindings();
}

HotKey::Listener::Listener(HotKey& hotKey_, EventDistributor::Priority priority_)
	: hotKey(hotKey_), priority(priority_)
{
	auto& distributor = hotKey.eventDistributor;
	using enum EventType;
	for (auto type : {KEY_DOWN, KEY_UP,
	                  MOUSE_MOTION, MOUSE_BUTTON_DOWN, MOUSE_BUTTON_UP, MOUSE_WHEEL,
	                  JOY_BUTTON_DOWN, JOY_BUTTON_UP, JOY_AXIS_MOTION, JOY_HAT,
	                  WINDOW, FILE_DROP, OSD_CONTROL_RELEASE, OSD_CONTROL_PRESS}) {
		distributor.registerEventListener(type, *this, priority);
	}
}

HotKey::Listener::~Listener()
{
	auto& distributor = hotKey.eventDistributor;
	using enum EventType;
	for (auto type : {OSD_CONTROL_PRESS, OSD_CONTROL_RELEASE, FILE_DROP, WINDOW,
	                  JOY_BUTTON_UP, JOY_BUTTON_DOWN, JOY_AXIS_MOTION, JOY_HAT,
	                  MOUSE_WHEEL, MOUSE_BUTTON_UP, MOUSE_BUTTON_DOWN, MOUSE_MOTION,
	                  KEY_UP, KEY_DOWN}) {
		distributor.unregisterEventListener(type, *this);
	}
}

void HotKey::initDefaultBindings()
{
	// TODO move to Tcl script?

	if constexpr (META_HOT_KEYS) {
		// Hot key combos using Mac's Command key.
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_d, KMOD_GUI),
		                       "screenshot -guess-name"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_p, KMOD_GUI),
		                       "toggle pause"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_t, KMOD_GUI),
		                       "toggle fastforward"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_l, KMOD_GUI),
		                       "toggle console"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_u, KMOD_GUI),
		                       "toggle mute"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_f, KMOD_GUI),
		                       "toggle fullscreen"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_q, KMOD_GUI),
		                       "exit"));
	} else {
		// Hot key combos for typical PC keyboards.
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_PRINTSCREEN),
		                       "screenshot -guess-name"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_PAUSE),
		                       "toggle pause"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_F9),
		                       "toggle fastforward"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_F10),
		                       "toggle console"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_F11),
		                       "toggle fullscreen"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_F12),
		                       "toggle mute"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_F4, KMOD_ALT),
		                       "exit"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_PAUSE, KMOD_CTRL),
		                       "exit"));
		bindDefault(HotKeyInfo(KeyDownEvent::create(SDLK_RETURN, KMOD_ALT),
		                       "toggle fullscreen"));
	}
}

static Event createEvent(const TclObject& obj, Interpreter& interp)
{
	auto event = InputEventFactory::createInputEvent(obj, interp);
	using enum EventType;
	if (getType(event) != one_of(KEY_UP, KEY_DOWN,
	                             MOUSE_BUTTON_UP, MOUSE_BUTTON_DOWN, GROUP,
	                             JOY_BUTTON_UP, JOY_BUTTON_DOWN, JOY_AXIS_MOTION, JOY_HAT,
	                             OSD_CONTROL_PRESS, OSD_CONTROL_RELEASE, WINDOW)) {
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

void HotKey::loadBind(const Data& data)
{
	bind(HotKeyInfo(createEvent(data.key, commandController.getInterpreter()),
			std::string(data.cmd), data.repeat, data.event, data.msx));
}

void HotKey::loadUnbind(std::string_view key)
{
	unbind(createEvent(key, commandController.getInterpreter()));
}

struct EqualEvent {
	explicit EqualEvent(const Event& event_) : event(event_) {}
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
	return std::ranges::any_of(range, EqualEvent(event));
}

template<typename T>
static void erase(std::vector<T>& v, const Event& event)
{
	if (auto it = std::ranges::find_if(v, EqualEvent(event)); it != end(v)) {
		move_pop_back(v, it);
	}
}

static void insert(HotKey::KeySet& set, const Event& event)
{
	if (auto it = std::ranges::find_if(set, EqualEvent(event)); it != end(set)) {
		*it = event;
	} else {
		set.push_back(event);
	}
}

template<typename HotKeyInfo>
static void insert(HotKey::BindMap& map, HotKeyInfo&& info)
{
	if (auto it = std::ranges::find_if(map, EqualEvent(info.event)); it != end(map)) {
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
	if (auto it1 = std::ranges::find_if(boundKeys, EqualEvent(event));
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
	activeLayers.emplace_back(std::move(layer), blocking);
}

void HotKey::deactivateLayer(std::string_view layer)
{
	// remove the first matching activation record from the end
	// (it's not an error if there is no match at all)
	if (auto it = std::ranges::find(std::views::reverse(activeLayers), layer, &LayerInfo::layer);
	    it != activeLayers.rend()) {
		// 'reverse_iterator' -> 'iterator' conversion is a bit tricky
		activeLayers.erase((it + 1).base());
	}
}

static HotKey::BindMap::const_iterator findMatch(
	const HotKey::BindMap& map, const Event& event, bool msx)
{
	return std::ranges::find_if(map, [&](auto& p) {
		return (p.msx == msx) && matches(p.event, event);
	});
}

void HotKey::executeRT()
{
	if (lastEvent) {
		executeEvent(*lastEvent, EventDistributor::Priority::HOTKEY_HIGH);
		executeEvent(*lastEvent, EventDistributor::Priority::HOTKEY_LOW);
	}
}

bool HotKey::Listener::signalEvent(const Event& event)
{
	return hotKey.signalEvent(event, priority);
}

bool HotKey::signalEvent(const Event& event, EventDistributor::Priority priority)
{
	if (lastEvent && *lastEvent != event) {
		// If the newly received event is different from the repeating
		// event, we stop the repeat process.
		// Except when we're repeating a OsdControlEvent and the
		// received event was actually the 'generating' event for the
		// Osd event. E.g. a cursor-keyboard-down event will generate
		// a corresponding osd event (the osd event is send before the
		// original event). Without this hack, key-repeat will not work
		// for osd key bindings.
		stopRepeat();
	}
	return executeEvent(event, priority);
}

bool HotKey::executeEvent(const Event& event, EventDistributor::Priority priority)
{
	bool msx = priority == EventDistributor::Priority::HOTKEY_LOW;

	// First search in active layers (from back to front)
	bool blocking = false;
	for (const auto& info : std::views::reverse(activeLayers)) {
		auto& cmap = layerMap[info.layer]; // ok, if this entry doesn't exist yet
		if (auto it = findMatch(cmap, event, msx); it != end(cmap)) {
			executeBinding(event, *it);
			// Deny event to lower priority listeners, also don't pass event
			// to other layers (including the default layer).
			return true;
		}
		blocking = info.blocking;
		if (blocking) break; // don't try lower layers
	}

	// If the event was not yet handled, try the default layer.
	if (auto it = findMatch(cmdMap, event, msx); it != end(cmdMap)) {
		executeBinding(event, *it);
		return true; // deny event to lower priority listeners
	}

	// Event is not handled, only let it pass to the MSX if there was no
	// blocking layer active.
	return blocking;
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
	lastEvent.reset();
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
	return strCat(toString(info.event),
	              (info.msx ? " [msx]" : ""),
	              (info.repeat ? " [repeat]" : ""),
	              (info.passEvent ? " [event]" : ""),
	              ":  ", info.command, '\n');
}

void HotKey::BindCmd::execute(std::span<const TclObject> tokens, TclObject& result)
{
	string layer;
	bool layers = false;
	bool repeat = false;
	bool passEvent = false;
	bool msx = false;
	std::array parserInfo = {
		valueArg("-layer", layer),
		flagArg("-layers", layers),
		flagArg("-repeat", repeat),
		flagArg("-event", passEvent),
		flagArg("-msx", msx),
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
		for (const auto& p : cMap) {
			r += formatBinding(p);
		}
		result = r;
		break;
	}
	case 1: {
		// show bindings for this key (in this layer)
		auto it = std::ranges::find_if(cMap,
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
		for (const auto& arg : std::views::drop(arguments, 2)) {
			strAppend(command, ' ', arg.getString());
		}
		HotKey::HotKeyInfo info(
			createEvent(arguments[0], getInterpreter()),
			command, repeat, passEvent, msx);
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
		cmd, " <key> [-msx] [-repeat] [-event] <cmd> : bind key to command, optionally "
		"repeat command while key remains pressed and also optionally "
		"give back the event as argument (a list) to <cmd>.\n"
		"When the '-msx' flag is given the binding only has effect "
		"when the msx window has focus (not when the GUI has focus).\n"
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
	std::array info = {valueArg("-layer", layer)};
	auto arguments = parseTclArgs(getInterpreter(), tokens.subspan<1>(), info);
	if (defaultCmd && !layer.empty()) {
		throw CommandException("Layers are not supported for default bindings");
	}

	if ((arguments.size() > 1) || (layer.empty() && (arguments.size() != 1))) {
		throw SyntaxError();
	}

	std::optional<Event> event;
	if (arguments.size() == 1) {
		event = createEvent(arguments[0], getInterpreter());
	}

	if (defaultCmd) {
		assert(event);
		hotKey.unbindDefault(*event);
	} else if (layer.empty()) {
		assert(event);
		hotKey.unbind(*event);
	} else {
		if (event) {
			hotKey.unbindLayer(*event, layer);
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
	std::array info = {flagArg("-blocking", blocking)};
	auto args = parseTclArgs(getInterpreter(), tokens.subspan(1), info);

	auto& hotKey = OUTER(HotKey, activateCmd);
	switch (args.size()) {
	case 0: {
		string r;
		for (const auto& layerInfo : std::views::reverse(hotKey.activeLayers)) {
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
