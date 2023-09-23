#ifndef EVENT_HH
#define EVENT_HH

#include "ObjectPool.hh"
#include "SDLKey.hh"
#include "static_vector.hh"
#include "StringStorage.hh"
#include "stl.hh"
#include "TclObject.hh"
#include "Thread.hh"
#include <cassert>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <variant>

#include <SDL.h>

namespace openmsx {

class CliConnection;
enum class EventType : uint8_t;

// --- The actual event classes (the Event class refers to one of these) ---

class EventBase {};

class SdlEvent : public EventBase
{
public:
	SdlEvent(const SDL_Event& e) : evt(e) {}
	[[nodiscard]] const SDL_Event& getSdlEvent() const { return evt; }
	[[nodiscard]] const SDL_CommonEvent& getCommonSdlEvent() const { return evt.common; }

protected:
	SDL_Event evt;
};


class KeyEvent : public SdlEvent
{
public:
	[[nodiscard]] bool         getRepeat()    const { return evt.key.repeat; }
	[[nodiscard]] SDL_Keycode  getKeyCode()   const { return evt.key.keysym.sym; }
	[[nodiscard]] SDL_Scancode getScanCode()  const { return evt.key.keysym.scancode; }
	[[nodiscard]] uint16_t     getModifiers() const { return evt.key.keysym.mod; }
	[[nodiscard]] uint32_t     getUnicode()   const {
		// HACK: repurposed 'unused' field as 'unicode' field
		return evt.key.keysym.unused;
	}
	void clearUnicode() { evt.key.keysym.unused = 0; }
	[[nodiscard]] SDLKey getKey() const { return SDLKey{evt.key.keysym, evt.type == SDL_KEYDOWN}; }

protected:
	KeyEvent(const SDL_Event& e) : SdlEvent(e) {}
};

class KeyUpEvent final : public KeyEvent
{
public:
	explicit KeyUpEvent(const SDL_Event& e) : KeyEvent(e) {}

	[[nodiscard]] static KeyUpEvent create(SDL_Keycode code, SDL_Keymod mod = KMOD_NONE) {
		SDL_Event evt = {};
		SDL_KeyboardEvent& e = evt.key;
		e.type = SDL_KEYUP;
		e.timestamp = SDL_GetTicks();
		e.state = SDL_RELEASED;
		e.keysym.sym = code;
		e.keysym.mod = mod;
		return KeyUpEvent(evt);
	}
};

class KeyDownEvent final : public KeyEvent
{
public:
	explicit KeyDownEvent(const SDL_Event& e) : KeyEvent(e) {}

	[[nodiscard]] static KeyDownEvent create(SDL_Keycode code, SDL_Keymod mod = KMOD_NONE) {
		SDL_Event evt = {};
		SDL_KeyboardEvent& e = evt.key;
		e.type = SDL_KEYDOWN;
		e.timestamp = SDL_GetTicks();
		e.state = SDL_PRESSED;
		e.keysym.sym = code;
		e.keysym.mod = mod;
		return KeyDownEvent(evt);
	}
	[[nodiscard]] static KeyDownEvent create(uint32_t timestamp, unsigned unicode) {
		SDL_Event evt = {};
		SDL_KeyboardEvent& e = evt.key;
		e.type = SDL_KEYDOWN;
		e.timestamp = timestamp;
		e.state = SDL_PRESSED;
		e.keysym.sym = SDLK_UNKNOWN;
		e.keysym.mod = KMOD_NONE;
		e.keysym.unused = unicode;
		return KeyDownEvent(evt);
	}
};


class MouseButtonEvent : public SdlEvent
{
public:
	[[nodiscard]] auto getButton() const { return evt.button.button; }

protected:
	explicit MouseButtonEvent(const SDL_Event& e)
		: SdlEvent(e) {}
};

class MouseButtonUpEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(const SDL_Event& e)
		: MouseButtonEvent(e) {}
};

class MouseButtonDownEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(const SDL_Event& e)
		: MouseButtonEvent(e) {}
};

class MouseWheelEvent final : public SdlEvent
{
public:
	MouseWheelEvent(const SDL_Event& e)
		: SdlEvent(e) {}

	[[nodiscard]] int getX() const { return normalize(evt.wheel.x); }
	[[nodiscard]] int getY() const { return normalize(evt.wheel.y); }

private:
	[[nodiscard]] int normalize(int x) const {
		return evt.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -x : x;
	}
};

class MouseMotionEvent final : public SdlEvent
{
public:
	MouseMotionEvent(const SDL_Event& e)
		: SdlEvent(e) {}

	[[nodiscard]] int getX() const    { return evt.motion.xrel; }
	[[nodiscard]] int getY() const    { return evt.motion.yrel; }
	[[nodiscard]] int getAbsX() const { return evt.motion.x; }
	[[nodiscard]] int getAbsY() const { return evt.motion.y; }
};


class JoystickEvent : public SdlEvent
{
public:
	[[nodiscard]] int getJoystick() const { return evt.jbutton.which; }

protected:
	explicit JoystickEvent(const SDL_Event& e)
		: SdlEvent(e) {}
};

class JoystickButtonEvent : public JoystickEvent
{
public:
	[[nodiscard]] unsigned getButton() const { return evt.jbutton.button; }

protected:
	JoystickButtonEvent(const SDL_Event& e)
		: JoystickEvent(e) {}
};

class JoystickButtonUpEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(const SDL_Event& e)
		: JoystickButtonEvent(e) {}
};

class JoystickButtonDownEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(const SDL_Event& e)
		: JoystickButtonEvent(e) {}
};

class JoystickAxisMotionEvent final : public JoystickEvent
{
public:
	static constexpr uint8_t X_AXIS = 0;
	static constexpr uint8_t Y_AXIS = 1;

	JoystickAxisMotionEvent(const SDL_Event& e)
		: JoystickEvent(e) {}

	[[nodiscard]] uint8_t getAxis() const { return evt.jaxis.axis; }
	[[nodiscard]] int16_t getValue() const { return evt.jaxis.value; }
};

class JoystickHatEvent final : public JoystickEvent
{
public:
	JoystickHatEvent(const SDL_Event& e)
		: JoystickEvent(e) {}

	[[nodiscard]] uint8_t getHat()   const { return evt.jhat.hat; }
	[[nodiscard]] uint8_t getValue() const { return evt.jhat.value; }
};


class WindowEvent : public SdlEvent
{
public:
	explicit WindowEvent(const SDL_Event& e)
		: SdlEvent(e) {}
	[[nodiscard]] const SDL_WindowEvent& getSdlWindowEvent() const { return evt.window; }
	[[nodiscard]] bool isMainWindow() const { return isMainWindow(evt.window.windowID); }

public:
	static void setMainWindowId(uint32_t id) { mainWindowId = id; }
	static uint32_t getMainWindowId() { return mainWindowId; }
	[[nodiscard]] static bool isMainWindow(unsigned id) { return id == mainWindowId; }
private:
	static inline uint32_t mainWindowId = unsigned(-1);
};


class TextEvent : public SdlEvent
{
public:
	explicit TextEvent(const SDL_Event& e)
		: SdlEvent(e) {}
};


class FileDropEvent final : public EventBase
{
public:
	explicit FileDropEvent(std::string_view fileName_)
		: fileName(allocate_c_string(fileName_)) {}
	FileDropEvent(const FileDropEvent&) { assert(false); }
	FileDropEvent& operator=(const FileDropEvent&) { assert(false); return *this; }
	FileDropEvent(FileDropEvent&&) = default;
	FileDropEvent& operator=(FileDropEvent&&) = default;

	[[nodiscard]] zstring_view getFileName() const { return fileName.get(); }

private:
	StringStorage fileName;
};


class QuitEvent final : public EventBase {};


/** OSD events are triggered by other events. They aggregate keyboard and
 * joystick events into one set of events that can be used to e.g. control
 * OSD elements.
 */
class OsdControlEvent : public EventBase
{
public:
	enum { LEFT_BUTTON, RIGHT_BUTTON, UP_BUTTON, DOWN_BUTTON,
		A_BUTTON, B_BUTTON };

	[[nodiscard]] unsigned getButton() const { return button; }

protected:
	OsdControlEvent(unsigned button_)
		: button(button_) {}

private:
	unsigned button;
};

class OsdControlReleaseEvent final : public OsdControlEvent
{
public:
	explicit OsdControlReleaseEvent(unsigned button_)
		: OsdControlEvent(button_) {}
};

class OsdControlPressEvent final : public OsdControlEvent
{
public:
	explicit OsdControlPressEvent(unsigned button_)
		: OsdControlEvent(button_) {}
};


class GroupEvent final : public EventBase
{
public:
	GroupEvent(std::initializer_list<EventType> typesToMatch_, TclObject tclListComponents_)
		: typesToMatch(typesToMatch_)
		, tclListComponents(std::move(tclListComponents_)) {}

	[[nodiscard]] const auto& getTypesToMatch() const { return typesToMatch; }
	[[nodiscard]] const auto& getTclListComponents() const { return tclListComponents; }

private:
	static_vector<EventType, 3> typesToMatch;
	TclObject tclListComponents;
};

/**
 * This event is send when a device (v99x8, v9990, video9000, laserdisc)
 * reaches the end of a frame. This event has info on:
 *  - which device generated the event
 *  - which video layer was active
 *  - was the frame actually rendered or not (frameskip)
 * Note that even if a frame was rendered (not skipped) it may not (need to) be
 * displayed because the corresponding video layer is not active. Or also even
 * if the corresponding video layer for a device is not active, the rendered
 * frame may still be displayed as part of a superimposed video layer.
 */
class FinishFrameEvent final : public EventBase
{
public:
	FinishFrameEvent(int thisSource_, int selectedSource_,
	                 bool skipped_)
		: thisSource(thisSource_), selectedSource(selectedSource_)
		, skipped(skipped_)
	{
	}

	[[nodiscard]] int getSource()         const { return thisSource; }
	[[nodiscard]] int getSelectedSource() const { return selectedSource; }
	[[nodiscard]] bool isSkipped() const { return skipped; }
	[[nodiscard]] bool needRender() const { return !skipped && (thisSource == selectedSource); }

private:
	int thisSource;
	int selectedSource;
	bool skipped;
};

/** Command received on CliComm connection. */
class CliCommandEvent final : public EventBase
{
public:
	CliCommandEvent(std::string_view command_, const CliConnection* id_)
		: command(allocate_c_string(command_)), id(id_) {}
	CliCommandEvent(const CliCommandEvent&) { assert(false); }
	CliCommandEvent& operator=(const CliCommandEvent&) { assert(false); return *this; }
	CliCommandEvent(CliCommandEvent&&) = default;
	CliCommandEvent& operator=(CliCommandEvent&&) = default;

	[[nodiscard]] zstring_view getCommand() const { return command.get(); }
	[[nodiscard]] const CliConnection* getId() const { return id; }

private:
	StringStorage command;
	const CliConnection* id;
};


// Events that don't need additional data
class SimpleEvent : public EventBase {};

/** Sent when the MSX resets or powers up. */
class BootEvent                  final : public SimpleEvent {};

/** Sent when a FINISH_FRAME caused a redraw of the screen.
  * So in other words send when a FINISH_FRAME event was send
  * and the frame was not skipped and the event came from the active video
  * source. */
class FrameDrawnEvent            final : public SimpleEvent {};

class BreakEvent                 final : public SimpleEvent {};
class SwitchRendererEvent        final : public SimpleEvent {};

/** Used to schedule 'taking reverse snapshots' between Z80 instructions. */
class TakeReverseSnapshotEvent   final : public SimpleEvent {};

/** Send when an after-EmuTime command should be executed. */
class AfterTimedEvent            final : public SimpleEvent {};

/** Send when a (new) machine configuration is loaded. */
class MachineLoadedEvent         final : public SimpleEvent {};

/** Send when a machine is (de)activated.
  * This events is specific per machine. */
class MachineActivatedEvent      final : public SimpleEvent {};
class MachineDeactivatedEvent    final : public SimpleEvent {};

class MidiInReaderEvent          final : public SimpleEvent {};
class MidiInWindowsEvent         final : public SimpleEvent {};
class MidiInCoreMidiEvent        final : public SimpleEvent {};
class MidiInCoreMidiVirtualEvent final : public SimpleEvent {};
class MidiInALSAEvent            final : public SimpleEvent {};
class Rs232TesterEvent           final : public SimpleEvent {};
class ImGuiDelayedActionEvent    final : public SimpleEvent {};


// --- Put all (non-abstract) Event classes into a std::variant ---

using Event = std::variant<
	KeyUpEvent,
	KeyDownEvent,
	MouseMotionEvent,
	MouseButtonUpEvent,
	MouseButtonDownEvent,
	MouseWheelEvent,
	JoystickAxisMotionEvent,
	JoystickHatEvent,
	JoystickButtonUpEvent,
	JoystickButtonDownEvent,
	OsdControlReleaseEvent,
	OsdControlPressEvent,
	WindowEvent,
	TextEvent,
	FileDropEvent,
	QuitEvent,
	FinishFrameEvent,
	CliCommandEvent,
	GroupEvent,
	BootEvent,
	FrameDrawnEvent,
	BreakEvent,
	SwitchRendererEvent,
	TakeReverseSnapshotEvent,
	AfterTimedEvent,
	MachineLoadedEvent,
	MachineActivatedEvent,
	MachineDeactivatedEvent,
	MidiInReaderEvent,
	MidiInWindowsEvent,
	MidiInCoreMidiEvent,
	MidiInCoreMidiVirtualEvent,
	MidiInALSAEvent,
	Rs232TesterEvent,
	ImGuiDelayedActionEvent
>;

template<typename T>
inline constexpr uint8_t event_index = get_index<T, Event>::value;


// --- Define an enum for all concrete event types. ---
// Use the numeric value from the corresponding index in the Event.
enum class EventType : uint8_t
{
	KEY_UP                   = event_index<KeyUpEvent>,
	KEY_DOWN                 = event_index<KeyDownEvent>,
	MOUSE_MOTION             = event_index<MouseMotionEvent>,
	MOUSE_BUTTON_UP          = event_index<MouseButtonUpEvent>,
	MOUSE_BUTTON_DOWN        = event_index<MouseButtonDownEvent>,
	MOUSE_WHEEL              = event_index<MouseWheelEvent>,
	JOY_AXIS_MOTION          = event_index<JoystickAxisMotionEvent>,
	JOY_HAT                  = event_index<JoystickHatEvent>,
	JOY_BUTTON_UP            = event_index<JoystickButtonUpEvent>,
	JOY_BUTTON_DOWN          = event_index<JoystickButtonDownEvent>,
	OSD_CONTROL_RELEASE      = event_index<OsdControlReleaseEvent>,
	OSD_CONTROL_PRESS        = event_index<OsdControlPressEvent>,
	WINDOW                   = event_index<WindowEvent>,
	TEXT                     = event_index<TextEvent>,
	FILE_DROP                = event_index<FileDropEvent>,
	QUIT                     = event_index<QuitEvent>,
	GROUP                    = event_index<GroupEvent>,
	BOOT                     = event_index<BootEvent>,
	FINISH_FRAME             = event_index<FinishFrameEvent>,
	FRAME_DRAWN              = event_index<FrameDrawnEvent>,
	BREAK                    = event_index<BreakEvent>,
	SWITCH_RENDERER          = event_index<SwitchRendererEvent>,
	TAKE_REVERSE_SNAPSHOT    = event_index<TakeReverseSnapshotEvent>,
	CLICOMMAND               = event_index<CliCommandEvent>,
	AFTER_TIMED              = event_index<AfterTimedEvent>,
	MACHINE_LOADED           = event_index<MachineLoadedEvent>,
	MACHINE_ACTIVATED        = event_index<MachineActivatedEvent>,
	MACHINE_DEACTIVATED      = event_index<MachineDeactivatedEvent>,
	MIDI_IN_READER           = event_index<MidiInReaderEvent>,
	MIDI_IN_WINDOWS          = event_index<MidiInWindowsEvent>,
	MIDI_IN_COREMIDI         = event_index<MidiInCoreMidiEvent>,
	MIDI_IN_COREMIDI_VIRTUAL = event_index<MidiInCoreMidiVirtualEvent>,
	MIDI_IN_ALSA             = event_index<MidiInALSAEvent>,
	RS232_TESTER             = event_index<Rs232TesterEvent>,
	IMGUI_DELAYED_ACTION     = event_index<ImGuiDelayedActionEvent>,

	NUM_EVENT_TYPES // must be last
};


// --- Event class, free functions ---

[[nodiscard]] EventType getType(const Event& event);

/** Get a string representation of this event. */
[[nodiscard]] std::string toString(const Event& event);

/** Similar to toString(), but retains the structure of the event. */
[[nodiscard]] TclObject toTclList(const Event& event);

[[nodiscard]] bool operator==(const Event& x, const Event& y);

/** Does this event 'match' the given event. Normally an event
  * only matches itself (as defined by operator==). But e.g.
  * MouseMotionGroupEvent matches any MouseMotionEvent. */
[[nodiscard]] bool matches(const Event& self, const Event& other);


// --- Event class implementation, free functions ---

inline EventType getType(const Event& event)
{
	return EventType(event.index());
}

// Similar to std::get() and std::get_if()
template<typename T>
struct GetIfEventHelper { // standard std::get_if() behavior
	const T* operator()(const Event& event) {
		return std::get_if<T>(&event);
	}
};
template<>
struct GetIfEventHelper<SdlEvent> { // extension for base-classes
	const SdlEvent* operator()(const Event& event) {
		const auto& var = event;
		switch (EventType(var.index())) {
		case EventType::KEY_UP:              return &std::get<KeyUpEvent>(var);
		case EventType::KEY_DOWN:            return &std::get<KeyDownEvent>(var);
		case EventType::MOUSE_BUTTON_UP:     return &std::get<MouseButtonUpEvent>(var);
		case EventType::MOUSE_BUTTON_DOWN:   return &std::get<MouseButtonDownEvent>(var);
		case EventType::MOUSE_WHEEL:         return &std::get<MouseWheelEvent>(var);
		case EventType::MOUSE_MOTION:        return &std::get<MouseMotionEvent>(var);
		case EventType::JOY_BUTTON_UP:       return &std::get<JoystickButtonUpEvent>(var);
		case EventType::JOY_BUTTON_DOWN:     return &std::get<JoystickButtonDownEvent>(var);
		case EventType::JOY_AXIS_MOTION:     return &std::get<JoystickAxisMotionEvent>(var);
		case EventType::JOY_HAT:             return &std::get<JoystickHatEvent>(var);
		case EventType::WINDOW:              return &std::get<WindowEvent>(var);
		case EventType::TEXT:                return &std::get<TextEvent>(var);
		default: return nullptr;
		}
	}
};
template<>
struct GetIfEventHelper<KeyEvent> {
	const KeyEvent* operator()(const Event& event) {
		const auto& var = event;
		switch (EventType(var.index())) {
		case EventType::KEY_UP:   return &std::get<KeyUpEvent>(var);
		case EventType::KEY_DOWN: return &std::get<KeyDownEvent>(var);
		default: return nullptr;
		}
	}
};
template<>
struct GetIfEventHelper<JoystickEvent> {
	const JoystickEvent* operator()(const Event& event) {
		const auto& var = event;
		switch (EventType(var.index())) {
		case EventType::JOY_BUTTON_UP:   return &std::get<JoystickButtonUpEvent>(var);
		case EventType::JOY_BUTTON_DOWN: return &std::get<JoystickButtonDownEvent>(var);
		case EventType::JOY_AXIS_MOTION: return &std::get<JoystickAxisMotionEvent>(var);
		case EventType::JOY_HAT:         return &std::get<JoystickHatEvent>(var);
		default: return nullptr;
		}
	}
};
template<typename T>
const T* get_event_if(const Event& event)
{
	GetIfEventHelper<T> helper;
	return helper(event);
}
template<typename T>
T* get_event_if(Event& event)
{
	GetIfEventHelper<T> helper;
	return const_cast<T*>(helper(event));
}
template<typename T>
const T& get_event(const Event& event)
{
	const T* t = get_event_if<T>(event);
	assert(t);
	return *t;
}

} // namespace openmsx

#endif
