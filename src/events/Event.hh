#ifndef EVENT_HH
#define EVENT_HH

#include <string>

namespace openmsx {

class TclObject;

enum EventType
{
	OPENMSX_KEY_UP_EVENT,
	OPENMSX_KEY_DOWN_EVENT,
	OPENMSX_KEY_GROUP_EVENT,
	OPENMSX_MOUSE_MOTION_EVENT,
	OPENMSX_MOUSE_MOTION_GROUP_EVENT,
	OPENMSX_MOUSE_BUTTON_UP_EVENT,
	OPENMSX_MOUSE_BUTTON_DOWN_EVENT,
	OPENMSX_MOUSE_BUTTON_GROUP_EVENT,
	OPENMSX_MOUSE_WHEEL_EVENT,
	OPENMSX_MOUSE_WHEEL_GROUP_EVENT,
	OPENMSX_JOY_AXIS_MOTION_EVENT,
	OPENMSX_JOY_AXIS_MOTION_GROUP_EVENT,
	OPENMSX_JOY_HAT_EVENT,
	OPENMSX_JOY_HAT_GROUP_EVENT,
	OPENMSX_JOY_BUTTON_UP_EVENT,
	OPENMSX_JOY_BUTTON_DOWN_EVENT,
	OPENMSX_JOY_BUTTON_GROUP_EVENT,
	OPENMSX_FOCUS_EVENT,
	OPENMSX_RESIZE_EVENT,
	OPENMSX_FILEDROP_EVENT,
	OPENMSX_FILEDROP_GROUP_EVENT,
	OPENMSX_QUIT_EVENT,
	OPENMSX_OSD_CONTROL_RELEASE_EVENT,
	OPENMSX_OSD_CONTROL_PRESS_EVENT,
	OPENMSX_BOOT_EVENT, // sent when the MSX resets or power ups

	/** Sent when VDP (V99x8 or V9990) reaches the end of a frame */
	OPENMSX_FINISH_FRAME_EVENT,

	/** Sent when a OPENMSX_FINISH_FRAME_EVENT caused a redraw of the screen.
	  * So in other words send when a OPENMSX_FINISH_FRAME_EVENT event was send
	  * and the frame was not skipped and the event came from the active video
	  * source. */
	OPENMSX_FRAME_DRAWN_EVENT,

	OPENMSX_BREAK_EVENT,
	OPENMSX_SWITCH_RENDERER_EVENT,

	/** Used to schedule 'taking reverse snapshots' between Z80 instructions. */
	OPENMSX_TAKE_REVERSE_SNAPSHOT,

	/** Command received on CliComm connection */
	OPENMSX_CLICOMMAND_EVENT,

	/** Send when an after-emutime command should be executed. */
	OPENMSX_AFTER_TIMED_EVENT,

	/** Send when a (new) machine configuration is loaded */
	OPENMSX_MACHINE_LOADED_EVENT,

	/** Send when a machine is (de)activated.
	  * This events is specific per machine. */
	OPENMSX_MACHINE_ACTIVATED,
	OPENMSX_MACHINE_DEACTIVATED,

	/** Send when (part of) the openMSX window gets exposed, and thus
	  * should be repainted. */
	OPENMSX_EXPOSE_EVENT,

	OPENMSX_MIDI_IN_READER_EVENT,
	OPENMSX_MIDI_IN_WINDOWS_EVENT,
	OPENMSX_MIDI_IN_COREMIDI_EVENT,
	OPENMSX_MIDI_IN_COREMIDI_VIRTUAL_EVENT,
	OPENMSX_RS232_TESTER_EVENT,

	NUM_EVENT_TYPES // must be last
};

class Event
{
public:
	Event(const Event&) = delete;
	Event& operator=(const Event&) = delete;

	[[nodiscard]] EventType getType() const { return type; }

	/** Get a string representation of this event. */
	[[nodiscard]] std::string toString() const;

	/** Similar to toString(), but retains the structure of the event. */
	[[nodiscard]] virtual TclObject toTclList() const = 0;

	[[nodiscard]] bool operator< (const Event& other) const;
	[[nodiscard]] bool operator> (const Event& other) const;
	[[nodiscard]] bool operator<=(const Event& other) const;
	[[nodiscard]] bool operator>=(const Event& other) const;
	[[nodiscard]] bool operator==(const Event& other) const;
	[[nodiscard]] bool operator!=(const Event& other) const;

	/** Should 'bind -repeat' be stopped by 'other' event.
	 * Normally all events should stop auto-repeat of the previous
	 * event. But see OsdControlEvent for some exceptions. */
	[[nodiscard]] virtual bool isRepeatStopper(const Event& /*other*/) const {
		return true;
	}

	/** Does this event 'match' the given event. Normally an event
	 * only matches itself (as defined by operator==). But e.g.
	 * MouseMotionGroupEvent matches any MouseMotionEvent. */
	[[nodiscard]] virtual bool matches(const Event& other) const {
		return *this == other;
	}

protected:
	explicit Event(EventType type_) : type(type_) {}
	~Event() = default;

private:
	[[nodiscard]] virtual bool lessImpl(const Event& other) const = 0;

	const EventType type;
};

// implementation for events that don't need additional data
class SimpleEvent final : public Event
{
public:
	explicit SimpleEvent(EventType type_) : Event(type_) {}
	[[nodiscard]] TclObject toTclList() const override;
	[[nodiscard]] bool lessImpl(const Event& other) const override;
};

} // namespace openmsx

#endif
