#ifndef INPUTEVENTS_HH
#define INPUTEVENTS_HH

#include "Event.hh"
#include "Keys.hh"
#include <cstdint>
#include <memory>

namespace openmsx {

class TimedEvent : public Event
{
public:
	/** Query creation time. */
	uint64_t getRealTime() const { return realtime; }
protected:
	explicit TimedEvent(EventType type);
private:
	const uint64_t realtime;
};


class KeyEvent : public TimedEvent
{
public:
	Keys::KeyCode getKeyCode() const { return keyCode; }
	uint16_t getUnicode() const { return unicode; }

protected:
	KeyEvent(EventType type, Keys::KeyCode keyCode, uint16_t unicode);

private:
	void toStringImpl(TclObject& result) const override;
	bool lessImpl(const Event& other) const override;
	const Keys::KeyCode keyCode;
	const uint16_t unicode;
};

class KeyUpEvent final : public KeyEvent
{
public:
	explicit KeyUpEvent(Keys::KeyCode keyCode);
	KeyUpEvent(Keys::KeyCode keyCode, uint16_t unicode);
};

class KeyDownEvent final : public KeyEvent
{
public:
	explicit KeyDownEvent(Keys::KeyCode keyCode);
	KeyDownEvent(Keys::KeyCode keyCode, uint16_t unicode);
};


class MouseButtonEvent : public TimedEvent
{
public:
	static const unsigned LEFT      = 1;
	static const unsigned MIDDLE    = 2;
	static const unsigned RIGHT     = 3;
	static const unsigned WHEELUP   = 4;
	static const unsigned WHEELDOWN = 5;

	unsigned getButton() const { return button; }

protected:
	MouseButtonEvent(EventType type, unsigned button_);
	void toStringHelper(TclObject& result) const;

private:
	bool lessImpl(const Event& other) const override;
	const unsigned button;
};

class MouseButtonUpEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonUpEvent(unsigned button);
private:
	void toStringImpl(TclObject& result) const override;
};

class MouseButtonDownEvent final : public MouseButtonEvent
{
public:
	explicit MouseButtonDownEvent(unsigned button);
private:
	void toStringImpl(TclObject& result) const override;
};

class MouseMotionEvent final : public TimedEvent
{
public:
	MouseMotionEvent(int xrel, int yrel, int xabs, int yabs);
	int getX() const    { return xrel; }
	int getY() const    { return yrel; }
	int getAbsX() const { return xabs; }
	int getAbsY() const { return yabs; }
private:
	void toStringImpl(TclObject& result) const override;
	bool lessImpl(const Event& other) const override;
	const int xrel;
	const int yrel;
	const int xabs;
	const int yabs;
};

class MouseMotionGroupEvent final : public Event
{
public:
	MouseMotionGroupEvent();

private:
	void toStringImpl(TclObject& result) const override;
	bool lessImpl(const Event& other) const override;
	bool matches(const Event& other) const override;
};


class JoystickEvent : public TimedEvent
{
public:
	unsigned getJoystick() const { return joystick; }

protected:
	JoystickEvent(EventType type, unsigned joystick);
	void toStringHelper(TclObject& result) const;

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
	void toStringHelper(TclObject& result) const;

private:
	bool lessImpl(const JoystickEvent& other) const override;
	const unsigned button;
};

class JoystickButtonUpEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonUpEvent(unsigned joystick, unsigned button);
private:
	void toStringImpl(TclObject& result) const override;
};

class JoystickButtonDownEvent final : public JoystickButtonEvent
{
public:
	JoystickButtonDownEvent(unsigned joystick, unsigned button);
private:
	void toStringImpl(TclObject& result) const override;
};

class JoystickAxisMotionEvent final : public JoystickEvent
{
public:
	static const unsigned X_AXIS = 0;
	static const unsigned Y_AXIS = 1;

	JoystickAxisMotionEvent(unsigned joystick, unsigned axis, int value);
	unsigned getAxis() const { return axis; }
	int getValue() const { return value; }

private:
	void toStringImpl(TclObject& result) const override;
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

private:
	void toStringImpl(TclObject& result) const override;
	bool lessImpl(const JoystickEvent& other) const override;
	const unsigned hat;
	const unsigned value;
};


class FocusEvent final : public Event
{
public:
	explicit FocusEvent(bool gain);

	bool getGain() const { return gain; }

private:
	void toStringImpl(TclObject& result) const override;
	bool lessImpl(const Event& other) const override;
	const bool gain;
};


class ResizeEvent final : public Event
{
public:
	ResizeEvent(unsigned x, unsigned y);

	unsigned getX() const { return x; }
	unsigned getY() const { return y; }

private:
	void toStringImpl(TclObject& result) const override;
	bool lessImpl(const Event& other) const override;
	const unsigned x;
	const unsigned y;
};


class QuitEvent final : public Event
{
public:
	QuitEvent();
private:
	void toStringImpl(TclObject& result) const override;
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
	void toStringHelper(TclObject& result) const;

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
private:
	void toStringImpl(TclObject& result) const override;
};

class OsdControlPressEvent final : public OsdControlEvent
{
public:
	OsdControlPressEvent(unsigned button,
	                     const std::shared_ptr<const Event>& origEvent);
private:
	void toStringImpl(TclObject& result) const override;
};

} // namespace openmsx

#endif
