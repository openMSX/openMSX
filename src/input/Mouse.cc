// $Id$

#include "Mouse.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"

using std::string;

namespace openmsx {

const int TRESHOLD = 2;
const int SCALE = 2;
const int FAZE_XHIGH = 0;
const int FAZE_XLOW  = 1;
const int FAZE_YHIGH = 2;
const int FAZE_YLOW  = 3;
const int STROBE = 0x04;


Mouse::Mouse()
{
	status = JOY_BUTTONA | JOY_BUTTONB;
	faze = FAZE_YLOW;
	xrel = yrel = curxrel = curyrel = 0;
	mouseMode = true;

	EventDistributor::instance().registerEventListener(MOUSE_MOTION_EVENT,      *this);
	EventDistributor::instance().registerEventListener(MOUSE_BUTTON_DOWN_EVENT, *this);
	EventDistributor::instance().registerEventListener(MOUSE_BUTTON_UP_EVENT,   *this);
}

Mouse::~Mouse()
{
	EventDistributor::instance().unregisterEventListener(MOUSE_MOTION_EVENT,      *this);
	EventDistributor::instance().unregisterEventListener(MOUSE_BUTTON_DOWN_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(MOUSE_BUTTON_UP_EVENT,   *this);
}


//Pluggable
const string& Mouse::getName() const
{
	static const string name("mouse");
	return name;
}

const string& Mouse::getDescription() const
{
	static const string desc("MSX mouse.");
	return desc;
}

void Mouse::plugHelper(Connector* /*connector*/, const EmuTime& time)
{
	if (status & JOY_BUTTONA) {
		// not pressed, mouse mode
		mouseMode = true;
		lastTime.advance(time);
	} else {
		// left mouse button pressed, joystick emulation mode
		mouseMode = false;
	}
}

void Mouse::unplugHelper(const EmuTime& /*time*/)
{
}


//JoystickDevice
byte Mouse::read(const EmuTime& /*time*/)
{
	if (mouseMode) {
		switch (faze) {
			case FAZE_XHIGH:
				return (((xrel / SCALE) >> 4) & 0x0F) | status;
			case FAZE_XLOW:
				return  ((xrel / SCALE)       & 0x0F) | status;
			case FAZE_YHIGH:
				return (((yrel / SCALE) >> 4) & 0x0F) | status;
			case FAZE_YLOW:
				return  ((yrel / SCALE)       & 0x0F) | status;
			default:
				assert(false);
				return status;	// avoid warning
		}
	} else {
		emulateJoystick();
		return status;
	}
}

void Mouse::emulateJoystick()
{
	status &= ~(JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT);

	int deltax = curxrel; curxrel = 0;
	int deltay = curyrel; curyrel = 0;
	int absx = (deltax > 0) ? deltax : -deltax;
	int absy = (deltay > 0) ? deltay : -deltay;

	if ((absx < TRESHOLD) && (absy < TRESHOLD)) {
		return;
	}
	
	// tan(pi/8) ~= 5/12
	if (deltax > 0) {
		if (deltay > 0) {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_RIGHT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_DOWN;
			}
		} else {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_RIGHT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_UP;
			}
		}
	} else {
		if (deltay > 0) {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_LEFT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_DOWN;
			}
		} else {
			if ((12 * absx) > (5 * absy)) {
				status |= JOY_LEFT;
			}
			if ((12 * absy) > (5 * absx)) {
				status |= JOY_UP;
			}
		}
	}
}

void Mouse::write(byte value, const EmuTime& time)
{
	if (mouseMode) {
		// TODO figure out the timeout mechanism
		//      does it exist at all?
		
		const int TIMEOUT = 1000;	// TODO find a good value
		int delta = lastTime.getTicksTill(time);
		lastTime.advance(time);
		if (delta >= TIMEOUT) {
			faze = FAZE_YLOW;
		}

		switch (faze) {
			case FAZE_XHIGH:
				if ((value & STROBE) == 0) faze = FAZE_XLOW;
				break;
			case FAZE_XLOW:
				if ((value & STROBE) != 0) faze = FAZE_YHIGH;
				break;
			case FAZE_YHIGH:
				if ((value & STROBE) == 0) faze = FAZE_YLOW;
				break;
			case FAZE_YLOW:
				if ((value & STROBE) != 0) {
					faze = FAZE_XHIGH;
					xrel = curxrel; yrel = curyrel;
					curxrel = 0; curyrel = 0;
				}
				break;
		}
	} else {
		// ignore
	}
}


//EventListener
bool Mouse::signalEvent(const Event& event)
{
	switch (event.getType()) {
	case MOUSE_MOTION_EVENT: {
		const MouseMotionEvent* motionEvent =
			dynamic_cast<const MouseMotionEvent*>(&event);
		assert(motionEvent);
		curxrel -= motionEvent->getX();
		curyrel -= motionEvent->getY();
		if (curxrel >  127 * SCALE) curxrel =  127 * SCALE;
		if (curxrel < -128 * SCALE) curxrel = -128 * SCALE;
		if (curyrel >  127 * SCALE) curyrel =  127 * SCALE;
		if (curyrel < -128 * SCALE) curyrel = -128 * SCALE;
		break;
	}
	case MOUSE_BUTTON_DOWN_EVENT: {
		const MouseButtonEvent* buttonEvent =
			dynamic_cast<const MouseButtonEvent*>(&event);
		assert(buttonEvent);
		switch (buttonEvent->getButton()) {
			case MouseButtonEvent::LEFT:
				status &= ~JOY_BUTTONA;
				break;
			case MouseButtonEvent::RIGHT:
				status &= ~JOY_BUTTONB;
				break;
			default:
				// ignore other buttons
				break;
		}
		break;
	}
	case MOUSE_BUTTON_UP_EVENT: {
		const MouseButtonEvent* buttonEvent =
			dynamic_cast<const MouseButtonEvent*>(&event);
		assert(buttonEvent);
		switch (buttonEvent->getButton()) {
			case MouseButtonEvent::LEFT:
				status |= JOY_BUTTONA;
				break;
			case MouseButtonEvent::RIGHT:
				status |= JOY_BUTTONB;
				break;
			default:
				// ignore other buttons
				break;
		}
		break;
	}
	default:
		assert(false);
	}
	return true;
}

} // namespace openmsx
