#ifndef EVENT_HH
#define EVENT_HH

#include "ObjectPool.hh"
#include "Keys.hh"
#include "static_vector.hh"
#include "StringStorage.hh"
#include "stl.hh"
#include "TclObject.hh"
#include "Thread.hh"
#include "Timer.hh"
#include <cassert>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <utility>
#include <variant>

namespace openmsx {

class CliConnection;
enum class EventType : uint8_t;
struct RcEvent;

// --- The Event class, this acts as a handle for the concrete event-type classes ---

class Event {
public:
	template<typename T, typename ...Args>
	[[nodiscard]] static Event create(Args&& ...args);

	Event() = default;
	Event(const Event& other) : ptr(other.ptr) { incr(); }
	Event(Event&& other) noexcept : ptr(other.ptr) { other.ptr = nullptr; }
	Event& operator=(const Event& other);
	Event& operator=(Event&& other) noexcept;
	~Event() { decr(); }

	[[nodiscard]] explicit operator bool() const { return ptr; }
	[[nodiscard]] const RcEvent* getPtr() const { return ptr; }

private:
	explicit Event(RcEvent* ptr);
	void incr();
	void decr();

private:
	RcEvent* ptr = nullptr;
};


// --- Event class, free functions ---

[[nodiscard]] EventType getType(const Event& event);

/** Get a string representation of this event. */
[[nodiscard]] std::string toString(const Event& event);

/** Similar to toString(), but retains the structure of the event. */
[[nodiscard]] TclObject toTclList(const Event& event);

[[nodiscard]] bool operator==(const Event& x, const Event& y);

/** Should 'bind -repeat' be stopped by 'other' event.
	* Normally all events should stop auto-repeat of the previous
	* event. But see OsdControlEvent for some exceptions. */
[[nodiscard]] bool isRepeatStopper(const Event& self, const Event& other);

/** Does this event 'match' the given event. Normally an event
  * only matches itself (as defined by operator==). But e.g.
  * MouseMotionGroupEvent matches any MouseMotionEvent. */
[[nodiscard]] bool matches(const Event& self, const Event& other);


// --- The actual event classes (the Event class refers to one of these) ---

class EventBase {};

class TimedEvent : public EventBase
{
public:
	[[nodiscard]] uint64_t getRealTime() const { return realtime; }

private:
	const uint64_t realtime = Timer::getTime(); // TODO use SDL2 event timestamp
};


class KeyEvent : public TimedEvent
{
public:
	[[nodiscard]] Keys::KeyCode getKeyCode() const { return keyCode; }
	[[nodiscard]] Keys::KeyCode getScanCode() const { return scanCode; }
	[[nodiscard]] uint32_t getUnicode() const { return unicode; }

protected:
	KeyEvent(Keys::KeyCode keyCode_, Keys::KeyCode scanCode_, uint32_t unicode_)
		: keyCode(keyCode_), scanCode(scanCode_), unicode(unicode_) {}

private:
	const Keys::KeyCode keyCode;
	const Keys::KeyCode scanCode; // 2nd class support, see comments in toTclList()
	const uint32_t unicode;
};

class KeyUpEvent final : public KeyEvent
{
public:
	explicit KeyUpEvent(Keys::KeyCode keyCode_)
		: KeyUpEvent(keyCode_, keyCode_) {}

	explicit KeyUpEvent(Keys::KeyCode keyCode_, Keys::KeyCode scanCode_)
		: KeyEvent(keyCode_, scanCode_, 0) {}
};

class KeyDownEvent final : public KeyEvent
{
public:
	explicit KeyDownEvent(Keys::KeyCode keyCode_)
		: KeyDownEvent(keyCode_, keyCode_, 0) {}

	explicit KeyDownEvent(Keys::KeyCode keyCode_, Keys::KeyCode scanCode_)
		: KeyDownEvent(keyCode_, scanCode_, 0) {}

	explicit KeyDownEvent(Keys::KeyCode keyCode_, uint32_t unicode_)
		: KeyDownEvent(keyCode_, keyCode_, unicode_) {}

	explicit KeyDownEvent(Keys::KeyCode keyCode_, Keys::KeyCode scanCode_, uint32_t unicode_)
		: KeyEvent(keyCode_, scanCode_, unicode_) {}
};


class MouseButtonEvent : public TimedEvent
{
public:
	static constexpr unsigned LEFT      = 1;
	static constexpr unsigned MIDDLE    = 2;
	static constexpr unsigned RIGHT     = 3;

	[[nodiscard]] unsigned getButton() const { return button; }

protected:
	explicit MouseButtonEvent(unsigned button_)
		: button(button_) {}

private:
	const unsigned button;
};

class MouseButtonUpEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(unsigned button_)
		: MouseButtonEvent(button_) {}
};

class MouseButtonDownEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(unsigned button_)
		: MouseButtonEvent(button_) {}
};

class MouseWheelEvent final : public TimedEvent
{
public:
	MouseWheelEvent(int x_, int y_)
		: x(x_), y(y_) {}

	[[nodiscard]] int getX() const  { return x; }
	[[nodiscard]] int getY() const  { return y; }

private:
	const int x;
	const int y;
};

class MouseMotionEvent final : public TimedEvent
{
public:
	MouseMotionEvent(int xrel_, int yrel_, int xabs_, int yabs_)
		: xrel(xrel_), yrel(yrel_)
		, xabs(xabs_), yabs(yabs_) {}

	[[nodiscard]] int getX() const    { return xrel; }
	[[nodiscard]] int getY() const    { return yrel; }
	[[nodiscard]] int getAbsX() const { return xabs; }
	[[nodiscard]] int getAbsY() const { return yabs; }

private:
	const int xrel;
	const int yrel;
	const int xabs;
	const int yabs;
};


class JoystickEvent : public TimedEvent
{
public:
	[[nodiscard]] int getJoystick() const { return joystick; }

protected:
	explicit JoystickEvent(int joystick_)
		: joystick(joystick_) {}

private:
	const int joystick;
};

class JoystickButtonEvent : public JoystickEvent
{
public:
	[[nodiscard]] unsigned getButton() const { return button; }

protected:
	JoystickButtonEvent(int joystick_, unsigned button_)
		: JoystickEvent(joystick_), button(button_) {}

private:
	const unsigned button;
};

class JoystickButtonUpEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(int joystick_, unsigned button_)
		: JoystickButtonEvent(joystick_, button_) {}
};

class JoystickButtonDownEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(int joystick_, unsigned button_)
		: JoystickButtonEvent(joystick_, button_) {}
};

class JoystickAxisMotionEvent final : public JoystickEvent
{
public:
	static constexpr unsigned X_AXIS = 0;
	static constexpr unsigned Y_AXIS = 1;

	JoystickAxisMotionEvent(int joystick_, unsigned axis_, int value_)
		: JoystickEvent(joystick_), axis(axis_), value(value_) {}

	[[nodiscard]] unsigned getAxis() const { return axis; }
	[[nodiscard]] int getValue() const { return value; }

private:
	const unsigned axis;
	const int value;
};

class JoystickHatEvent final : public JoystickEvent
{
public:
	JoystickHatEvent(int joystick_, unsigned hat_, unsigned value_)
		: JoystickEvent(joystick_), hat(hat_), value(value_) {}

	[[nodiscard]] unsigned getHat()   const { return hat; }
	[[nodiscard]] unsigned getValue() const { return value; }

private:
	const unsigned hat;
	const unsigned value;
};


class FocusEvent final : public EventBase
{
public:
	explicit FocusEvent(bool gain_)
		: gain(gain_) {}

	[[nodiscard]] bool getGain() const { return gain; }

private:
	const bool gain;
};


class ResizeEvent final : public EventBase
{
public:
	ResizeEvent(unsigned x_, unsigned y_)
		: x(x_), y(y_) {}

	[[nodiscard]] unsigned getX() const { return x; }
	[[nodiscard]] unsigned getY() const { return y; }

private:
	const unsigned x;
	const unsigned y;
};


class FileDropEvent final : public EventBase
{
public:
	explicit FileDropEvent(std::string_view fileName_)
		: fileName(allocate_c_string(fileName_)) {}

	[[nodiscard]] zstring_view getFileName() const { return fileName.get(); }

private:
	const StringStorage fileName;
};


class QuitEvent final : public EventBase {};


/** OSD events are triggered by other events. They aggregate keyboard and
 * joystick events into one set of events that can be used to e.g. control
 * OSD elements.
 */
class OsdControlEvent : public TimedEvent
{
public:
	enum { LEFT_BUTTON, RIGHT_BUTTON, UP_BUTTON, DOWN_BUTTON,
		A_BUTTON, B_BUTTON };

	[[nodiscard]] unsigned getButton() const { return button; }

	/** Get the event that actually triggered the creation of this event.
	 * Typically this will be a keyboard or joystick event. This could
	 * also return nullptr (after a toString/fromString conversion).
	 * For the current use (key-repeat) this is ok. */
	[[nodiscard]] const Event& getOrigEvent() const { return origEvent; }

protected:
	OsdControlEvent(unsigned button_, Event origEvent_)
		: origEvent(std::move(origEvent_)), button(button_) {}

private:
	const Event origEvent;
	const unsigned button;
};

class OsdControlReleaseEvent final : public OsdControlEvent
{
public:
	OsdControlReleaseEvent(unsigned button_, Event origEvent_)
		: OsdControlEvent(button_, std::move(origEvent_)) {}
};

class OsdControlPressEvent final : public OsdControlEvent
{
public:
	OsdControlPressEvent(unsigned button_, Event origEvent_)
		: OsdControlEvent(button_, std::move(origEvent_)) {}
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
	const static_vector<EventType, 3> typesToMatch;
	const TclObject tclListComponents;
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
	const int thisSource;
	const int selectedSource;
	const bool skipped;
};

/** Command received on CliComm connection. */
class CliCommandEvent final : public EventBase
{
public:
	CliCommandEvent(std::string_view command_, const CliConnection* id_)
		: command(allocate_c_string(command_)), id(id_) {}

	[[nodiscard]] zstring_view getCommand() const { return command.get(); }
	[[nodiscard]] const CliConnection* getId() const { return id; }

private:
	const StringStorage command;
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

/** Send when an after-emutime command should be executed. */
class AfterTimedEvent            final : public SimpleEvent {};

/** Send when a (new) machine configuration is loaded. */
class MachineLoadedEvent         final : public SimpleEvent {};

/** Send when a machine is (de)activated.
  * This events is specific per machine. */
class MachineActivatedEvent      final : public SimpleEvent {};
class MachineDeactivatedEvent    final : public SimpleEvent {};

/** Send when (part of) the openMSX window gets exposed, and thus
  * should be repainted. */
class ExposeEvent                final : public SimpleEvent {};

class MidiInReaderEvent          final : public SimpleEvent {};
class MidiInWindowsEvent         final : public SimpleEvent {};
class MidiInCoreMidiEvent        final : public SimpleEvent {};
class MidiInCoreMidiVirtualEvent final : public SimpleEvent {};
class Rs232TesterEvent           final : public SimpleEvent {};


// --- Put all (non-abstract) Event classes into a std::variant ---

using EventVariant = std::variant<
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
	FocusEvent,
	ResizeEvent,
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
	ExposeEvent,
	MidiInReaderEvent,
	MidiInWindowsEvent,
	MidiInCoreMidiEvent,
	MidiInCoreMidiVirtualEvent,
	Rs232TesterEvent
>;

template<typename T>
inline constexpr uint8_t event_index = get_index<T, EventVariant>::value;


// --- Define an enum for all concrete event types. ---
// Use the numeric value from the corresponding index in the EventVariant.
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
	FOCUS                    = event_index<FocusEvent>,
	RESIZE                   = event_index<ResizeEvent>,
	FILEDROP                 = event_index<FileDropEvent>,
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
	EXPOSE                   = event_index<ExposeEvent>,
	MIDI_IN_READER           = event_index<MidiInReaderEvent>,
	MIDI_IN_WINDOWS          = event_index<MidiInWindowsEvent>,
	MIDI_IN_COREMIDI         = event_index<MidiInCoreMidiEvent>,
	MIDI_IN_COREMIDI_VIRTUAL = event_index<MidiInCoreMidiVirtualEvent>,
	RS232_TESTER             = event_index<Rs232TesterEvent>,

	NUM_EVENT_TYPES // must be last
};


// --- Add a reference-count ---
struct RcEvent : EventVariant {
	template<typename ...Args>
	RcEvent(Args&& ...args) : EventVariant(std::forward<Args>(args)...) {}

	uint8_t refCount = 1;
};

// --- Store event objects into a pool ---
inline ObjectPool<RcEvent> eventPool;
// A note on threading:
// * All threads are allowed to create and destroy Event objects.
// * Copying Event objects within one thread is allowed. Copying across threads
//   is not.
// * Moving an object from one thread to another is allowed, but only when that
//   object was not copied yet (when the internal reference count is still 1).
// The technical reason for this is that we protect the 'eventPool' with a
// mutex, but the reference count on each Event object is not protected. This is
// sufficient for the following scenario:
// * Most event handling in openMSX is solely done on the main thread. Here
//   events can be freely copied. And shared Event objects (objects with
//   reference count > 1) only exist on the main thread.
// * In some cases a helper thread want to signal something to the main thread.
//   It can then construct an Event object (construction is allowed on non-main
//   thread), and _move_ this freshly created (not yet copied) object to the
//   EventDistributor. The EventDistribtor may or may not move this object into
//   some queue. When it was moved then later the main-thread will pick it up
//   (and processes and eventually destroy it). When it was not moved the (same)
//   non-main thread will destroy the object.
// So creation and destruction can be done in any thread and must be protected
// with a mutex. Copying should only be done in the main-thread and thus it's
// not required to protect the reference count.
inline std::recursive_mutex eventPoolMutex;


// --- Event class implementation, member functions ---

template<typename T, typename ...Args>
Event Event::create(Args&& ...args)
{
	std::scoped_lock lock(eventPoolMutex);
	return Event(eventPool.emplace(std::in_place_type_t<T>{}, std::forward<Args>(args)...).ptr);
}

inline Event::Event(RcEvent* ptr_)
	: ptr(ptr_)
{
	assert(ptr->refCount == 1);
}

inline Event& Event::operator=(const Event& other)
{
	if (this != &other) {
		decr();
		ptr = other.ptr;
		incr();
	}
	return *this;
}

inline Event& Event::operator=(Event&& other) noexcept
{
	if (this != &other) {
		decr();
		ptr = other.ptr;
		other.ptr = nullptr;
	}
	return *this;
}

inline void Event::incr()
{
	if (!ptr) return;
	assert(Thread::isMainThread());
	assert(ptr->refCount < std::numeric_limits<decltype(ptr->refCount)>::max());
	++ptr->refCount;
}

inline void Event::decr()
{
	if (!ptr) return;
	assert(Thread::isMainThread());
	--ptr->refCount;
	if (ptr->refCount == 0) {
		std::scoped_lock lock(eventPoolMutex);
		eventPool.remove(ptr);
		ptr = nullptr;
	}
}


// --- Event class implementation, free functions ---

inline const EventVariant& getVariant(const Event& event)
{
	return *event.getPtr();
}

inline EventType getType(const Event& event)
{
	assert(event);
	return EventType(getVariant(event).index());
}

// Similar to std::visit()
template<typename Visitor>
auto visit(Visitor&& visitor, const Event& event)
{
	assert(event);
	return std::visit(std::forward<Visitor>(visitor), getVariant(event));
}
template<typename Visitor>
auto visit(Visitor&& visitor, const Event& event1, const Event& event2)
{
	assert(event1 && event2);
	return std::visit(std::forward<Visitor>(visitor), getVariant(event1), getVariant(event2));
}

// Similar to std::get() and std::get_if()
template<typename T>
struct GetIfEventHelper { // standard std::get_if() behavior
	const T* operator()(const Event& event) {
		return std::get_if<T>(&getVariant(event));
	}
};
template<>
struct GetIfEventHelper<TimedEvent> { // extension for base-classes
	const TimedEvent* operator()(const Event& event) {
		const auto& var = getVariant(event);
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
		case EventType::OSD_CONTROL_RELEASE: return &std::get<OsdControlReleaseEvent>(var);
		case EventType::OSD_CONTROL_PRESS:   return &std::get<OsdControlPressEvent>(var);
		default: return nullptr;
		}
	}
};
template<>
struct GetIfEventHelper<KeyEvent> {
	const KeyEvent* operator()(const Event& event) {
		const auto& var = getVariant(event);
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
		const auto& var = getVariant(event);
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
const T* get_if(const Event& event)
{
	assert(event);
	GetIfEventHelper<T> helper;
	return helper(event);
}
template<typename T>
const T& get(const Event& event)
{
	assert(event);
	const T* t = get_if<T>(event);
	assert(t);
	return *t;
}

} // namespace openmsx

#endif
