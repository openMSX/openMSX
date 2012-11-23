// $Id$

#ifndef EVENT_HH
#define EVENT_HH

#include "noncopyable.hh"
#include <string>

namespace openmsx {

class TclObject;

enum EventType
{
	OPENMSX_KEY_UP_EVENT,
	OPENMSX_KEY_DOWN_EVENT,
	OPENMSX_MOUSE_MOTION_EVENT,
	OPENMSX_MOUSE_BUTTON_UP_EVENT,
	OPENMSX_MOUSE_BUTTON_DOWN_EVENT,
	OPENMSX_JOY_AXIS_MOTION_EVENT,
	OPENMSX_JOY_BUTTON_UP_EVENT,
	OPENMSX_JOY_BUTTON_DOWN_EVENT,
	OPENMSX_FOCUS_EVENT,
	OPENMSX_RESIZE_EVENT,
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

	/** Delayed repaint */
	OPENMSX_DELAYED_REPAINT_EVENT,

	/** Throttle LED events */
	OPENMSX_THROTTLE_LED_EVENT,

	/** Send when main-thread should save SRAM contents */
	OPENMSX_SAVE_SRAM,

	/** Send when hotkey command should be repeated */
	OPENMSX_REPEAT_HOTKEY,

	/** Used to schedule 'taking reverse snapshots' between Z80 instructions. */
	OPENMSX_TAKE_REVERSE_SNAPSHOT,

	/** Command received on CliComm connection */
	OPENMSX_CLICOMMAND_EVENT,

	/** This event is periodically send (50 times per second atm).
	  * Used to implement polling (e.g SDL input events). */
	OPENMSX_POLL_EVENT,

	/** Send when a (new) machine configuration is loaded */
	OPENMSX_MACHINE_LOADED_EVENT,

	/** Send when a machine is (de)activated.
	  * This events is specific per machine. */
	OPENMSX_MACHINE_ACTIVATED,
	OPENMSX_MACHINE_DEACTIVATED,

	/** Send when (part of) the openMSX window gets exposed, and thus
	  * should be repainted. */
	OPENMSX_EXPOSE_EVENT,

	/** Delete old MSXMotherboards */
	OPENMSX_DELETE_BOARDS,

	OPENMSX_MIDI_IN_READER_EVENT,
	OPENMSX_MIDI_IN_WINDOWS_EVENT,
	OPENMSX_RS232_TESTER_EVENT,
	OPENMSX_AFTER_REALTIME_EVENT,
	OPENMSX_POINTER_TIMER_EVENT,
};

class Event : private noncopyable
{
public:
	virtual ~Event();

	EventType getType() const;
	std::string toString() const;
	bool operator< (const Event& other) const;
	bool operator==(const Event& other) const;
	bool operator!=(const Event& other) const;

protected:
	explicit Event(EventType type);

private:
	virtual void toStringImpl(TclObject& result) const;
	virtual bool lessImpl(const Event& other) const;

	const EventType type;
};

// implementation for events that don't need additional data
class SimpleEvent : public Event
{
public:
	SimpleEvent(EventType type) : Event(type) {}
};

} // namespace openmsx

#endif
