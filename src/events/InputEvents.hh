#ifndef INPUTEVENTS_HH
#define INPUTEVENTS_HH

#include "Event.hh"
#include "Keys.hh"
#include "TclObject.hh"
#include <cstdint>
#include <memory>
#include <vector>

namespace openmsx {

class TimedEvent : public Event
{
public:
	/** Query creation time. */
	[[nodiscard]] uint64_t getRealTime() const { return realtime; }
protected:
	explicit TimedEvent(EventType type);
	~TimedEvent() = default;
private:
	const uint64_t realtime;
};


class KeyEvent : public TimedEvent
{
public:
	[[nodiscard]] Keys::KeyCode getKeyCode() const { return keyCode; }
	[[nodiscard]] Keys::KeyCode getScanCode() const { return scanCode; }
	[[nodiscard]] uint32_t getUnicode() const { return unicode; }

protected:
	KeyEvent(EventType type, Keys::KeyCode keyCode, Keys::KeyCode scanCode, uint32_t unicode);
	~KeyEvent() = default;

private:
	[[nodiscard]] TclObject toTclList() const override;
	[[nodiscard]] bool equalImpl(const Event& other) const override;
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
		: KeyEvent(OPENMSX_KEY_UP_EVENT, keyCode_, scanCode_, 0) {}
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
		: KeyEvent(OPENMSX_KEY_DOWN_EVENT, keyCode_, scanCode_, unicode_) {}
};


class MouseButtonEvent : public TimedEvent
{
public:
	static constexpr unsigned LEFT      = 1;
	static constexpr unsigned MIDDLE    = 2;
	static constexpr unsigned RIGHT     = 3;

	[[nodiscard]] unsigned getButton() const { return button; }

protected:
	MouseButtonEvent(EventType type, unsigned button_);
	~MouseButtonEvent() = default;
	[[nodiscard]] TclObject toTclHelper(std::string_view direction) const;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const unsigned button;
};

class MouseButtonUpEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(unsigned button);
	[[nodiscard]] TclObject toTclList() const override;
};

class MouseButtonDownEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(unsigned button);
	[[nodiscard]] TclObject toTclList() const override;
};

class MouseWheelEvent final : public TimedEvent
{
public:
	MouseWheelEvent(int x, int y);
	[[nodiscard]] int getX() const  { return x; }
	[[nodiscard]] int getY() const  { return y; }
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const int x;
	const int y;
};

class MouseMotionEvent final : public TimedEvent
{
public:
	MouseMotionEvent(int xrel, int yrel, int xabs, int yabs);
	[[nodiscard]] int getX() const    { return xrel; }
	[[nodiscard]] int getY() const    { return yrel; }
	[[nodiscard]] int getAbsX() const { return xabs; }
	[[nodiscard]] int getAbsY() const { return yabs; }
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const int xrel;
	const int yrel;
	const int xabs;
	const int yabs;
};

class JoystickEvent : public TimedEvent
{
public:
	[[nodiscard]] unsigned getJoystick() const { return joystick; }

protected:
	JoystickEvent(EventType type, unsigned joystick);
	~JoystickEvent() = default;
	[[nodiscard]] TclObject toTclHelper() const;

private:
	const unsigned joystick;
};

class JoystickButtonEvent : public JoystickEvent
{
public:
	[[nodiscard]] unsigned getButton() const { return button; }

protected:
	JoystickButtonEvent(EventType type, unsigned joystick, unsigned button);
	~JoystickButtonEvent() = default;
	[[nodiscard]] TclObject toTclHelper(std::string_view direction) const;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const unsigned button;
};

class JoystickButtonUpEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(unsigned joystick, unsigned button);
	[[nodiscard]] TclObject toTclList() const override;
};

class JoystickButtonDownEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(unsigned joystick, unsigned button);
	[[nodiscard]] TclObject toTclList() const override;
};

class JoystickAxisMotionEvent final : public JoystickEvent
{
public:
	static constexpr unsigned X_AXIS = 0;
	static constexpr unsigned Y_AXIS = 1;

	JoystickAxisMotionEvent(unsigned joystick, unsigned axis, int value);
	[[nodiscard]] unsigned getAxis() const { return axis; }
	[[nodiscard]] int getValue() const { return value; }
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const unsigned axis;
	const int value;
};

class JoystickHatEvent final : public JoystickEvent
{
public:
	JoystickHatEvent(unsigned joystick, unsigned hat, unsigned value);
	[[nodiscard]] unsigned getHat()   const { return hat; }
	[[nodiscard]] unsigned getValue() const { return value; }
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const unsigned hat;
	const unsigned value;
};


class FocusEvent final : public Event
{
public:
	explicit FocusEvent(bool gain);
	[[nodiscard]] bool getGain() const { return gain; }
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const bool gain;
};


class ResizeEvent final : public Event
{
public:
	ResizeEvent(unsigned x, unsigned y);
	[[nodiscard]] unsigned getX() const { return x; }
	[[nodiscard]] unsigned getY() const { return y; }
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const unsigned x;
	const unsigned y;
};


class FileDropEvent final : public Event
{
public:
	explicit FileDropEvent(std::string fileName);
	[[nodiscard]] const std::string& getFileName() const { return fileName; }
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	const std::string fileName;
};


class QuitEvent final : public Event
{
public:
	QuitEvent();
	[[nodiscard]] TclObject toTclList() const override;
private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
};

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
	/** Normally all events should stop the repeat process in 'bind -repeat',
	 * but in case of OsdControlEvent there are two exceptions:
	 *  - we should not stop because of the original host event that
	 *    actually generated this 'artificial' OsdControlEvent.
	 *  - if the original host event is a joystick motion event, we
	 *    should not stop repeat for 'small' relative new joystick events.
	 */
	[[nodiscard]] bool isRepeatStopper(const Event& other) const final;

protected:
	OsdControlEvent(EventType type, unsigned button_,
	                std::shared_ptr<const Event> origEvent);
	~OsdControlEvent() = default;
	[[nodiscard]] TclObject toTclHelper(std::string_view direction) const;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const final;
	const std::shared_ptr<const Event> origEvent;
	const unsigned button;
};

class OsdControlReleaseEvent final : public OsdControlEvent
{
public:
	OsdControlReleaseEvent(unsigned button,
	                       const std::shared_ptr<const Event>& origEvent);
	[[nodiscard]] TclObject toTclList() const override;
};

class OsdControlPressEvent final : public OsdControlEvent
{
public:
	OsdControlPressEvent(unsigned button,
	                     const std::shared_ptr<const Event>& origEvent);
	[[nodiscard]] TclObject toTclList() const override;
};


class GroupEvent final : public Event
{
public:
	GroupEvent(EventType type, std::vector<EventType> typesToMatch, TclObject tclListComponents);
	[[nodiscard]] TclObject toTclList() const override;

private:
	[[nodiscard]] bool equalImpl(const Event& other) const override;
	[[nodiscard]] bool matches(const Event& other) const override;
	const std::vector<EventType> typesToMatch;
	const TclObject tclListComponents;
};


} // namespace openmsx

#endif
