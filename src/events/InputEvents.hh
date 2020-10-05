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
	uint64_t getRealTime() const { return realtime; }
protected:
	explicit TimedEvent(EventType type);
	~TimedEvent() = default;
private:
	const uint64_t realtime;
};


class KeyEvent : public TimedEvent
{
public:
	Keys::KeyCode getKeyCode() const { return keyCode; }
	Keys::KeyCode getScanCode() const { return scanCode; }
	uint32_t getUnicode() const { return unicode; }

protected:
	KeyEvent(EventType type, Keys::KeyCode keyCode, Keys::KeyCode scanCode, uint32_t unicode);
	~KeyEvent() = default;

private:
	TclObject toTclList() const override;
	bool lessImpl(const Event& other) const override;
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

	unsigned getButton() const { return button; }

protected:
	MouseButtonEvent(EventType type, unsigned button_);
	~MouseButtonEvent() = default;
	TclObject toTclHelper(std::string_view direction) const;

private:
	bool lessImpl(const Event& other) const override;
	const unsigned button;
};

class MouseButtonUpEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(unsigned button);
	TclObject toTclList() const override;
};

class MouseButtonDownEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(unsigned button);
	TclObject toTclList() const override;
};

class MouseWheelEvent final : public TimedEvent
{
public:
	MouseWheelEvent(int x, int y);
	int getX() const  { return x; }
	int getY() const  { return y; }
	TclObject toTclList() const override;

private:
	bool lessImpl(const Event& other) const override;
	const int x;
	const int y;
};

class MouseMotionEvent final : public TimedEvent
{
public:
	MouseMotionEvent(int xrel, int yrel, int xabs, int yabs);
	int getX() const    { return xrel; }
	int getY() const    { return yrel; }
	int getAbsX() const { return xabs; }
	int getAbsY() const { return yabs; }
	TclObject toTclList() const override;

private:
	bool lessImpl(const Event& other) const override;
	const int xrel;
	const int yrel;
	const int xabs;
	const int yabs;
};

class JoystickEvent : public TimedEvent
{
public:
	unsigned getJoystick() const { return joystick; }

protected:
	JoystickEvent(EventType type, unsigned joystick);
	~JoystickEvent() = default;
	TclObject toTclHelper() const;

private:
	bool lessImpl(const Event& other) const override;
	virtual bool lessImpl(const JoystickEvent& other) const = 0;
	const unsigned joystick;
};

class JoystickButtonEvent : public JoystickEvent
{
public:
	unsigned getButton() const { return button; }

protected:
	JoystickButtonEvent(EventType type, unsigned joystick, unsigned button);
	~JoystickButtonEvent() = default;
	TclObject toTclHelper(std::string_view direction) const;

private:
	bool lessImpl(const JoystickEvent& other) const override;
	const unsigned button;
};

class JoystickButtonUpEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(unsigned joystick, unsigned button);
	TclObject toTclList() const override;
};

class JoystickButtonDownEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(unsigned joystick, unsigned button);
	TclObject toTclList() const override;
};

class JoystickAxisMotionEvent final : public JoystickEvent
{
public:
	static constexpr unsigned X_AXIS = 0;
	static constexpr unsigned Y_AXIS = 1;

	JoystickAxisMotionEvent(unsigned joystick, unsigned axis, int value);
	unsigned getAxis() const { return axis; }
	int getValue() const { return value; }
	TclObject toTclList() const override;

private:
	bool lessImpl(const JoystickEvent& other) const override;
	const unsigned axis;
	const int value;
};

class JoystickHatEvent final : public JoystickEvent
{
public:
	JoystickHatEvent(unsigned joystick, unsigned hat, unsigned value);
	unsigned getHat()   const { return hat; }
	unsigned getValue() const { return value; }
	TclObject toTclList() const override;

private:
	bool lessImpl(const JoystickEvent& other) const override;
	const unsigned hat;
	const unsigned value;
};


class FocusEvent final : public Event
{
public:
	explicit FocusEvent(bool gain);
	bool getGain() const { return gain; }
	TclObject toTclList() const override;

private:
	bool lessImpl(const Event& other) const override;
	const bool gain;
};


class ResizeEvent final : public Event
{
public:
	ResizeEvent(unsigned x, unsigned y);
	unsigned getX() const { return x; }
	unsigned getY() const { return y; }
	TclObject toTclList() const override;

private:
	bool lessImpl(const Event& other) const override;
	const unsigned x;
	const unsigned y;
};


class FileDropEvent final : public Event
{
public:
	explicit FileDropEvent(std::string fileName);
	const std::string& getFileName() const { return fileName; }
	TclObject toTclList() const override;

private:
	bool lessImpl(const Event& other) const override;
	const std::string fileName;
};


class QuitEvent final : public Event
{
public:
	QuitEvent();
	TclObject toTclList() const override;
private:
	bool lessImpl(const Event& other) const override;
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

	unsigned getButton() const { return button; }

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
	bool isRepeatStopper(const Event& other) const final override;

protected:
	OsdControlEvent(EventType type, unsigned button_,
	                std::shared_ptr<const Event> origEvent);
	~OsdControlEvent() = default;
	TclObject toTclHelper(std::string_view direction) const;

private:
	bool lessImpl(const Event& other) const final override;
	const std::shared_ptr<const Event> origEvent;
	const unsigned button;
};

class OsdControlReleaseEvent final : public OsdControlEvent
{
public:
	OsdControlReleaseEvent(unsigned button,
	                       const std::shared_ptr<const Event>& origEvent);
	TclObject toTclList() const override;
};

class OsdControlPressEvent final : public OsdControlEvent
{
public:
	OsdControlPressEvent(unsigned button,
	                     const std::shared_ptr<const Event>& origEvent);
	TclObject toTclList() const override;
};


class GroupEvent final : public Event
{
public:
	GroupEvent(EventType type, std::vector<EventType> typesToMatch, const TclObject& tclListComponents);
	TclObject toTclList() const override;

private:
	bool lessImpl(const Event& other) const override;
	bool matches(const Event& other) const override;
	const std::vector<EventType> typesToMatch;
	const TclObject tclListComponents;
};


} // namespace openmsx

#endif
