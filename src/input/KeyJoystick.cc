// $Id$

#include <cassert>
#include "KeyJoystick.hh"
#include "PluggingController.hh"
#include "EventDistributor.hh"
#include "Keys.hh"


KeyJoystick::KeyJoystick()
{
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this, 1);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP  , this, 1);

	PluggingController::instance()->registerPluggable(this);

	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

KeyJoystick::~KeyJoystick()
{
	PluggingController::instance()->unregisterPluggable(this);

	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this, 1);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP  , this, 1);
}

// Pluggable
const std::string &KeyJoystick::getName() const
{
	static std::string name("keyjoystick");
	return name;
}

void KeyJoystick::plug(Connector* connector, const EmuTime& time)
{
}

void KeyJoystick::unplug(const EmuTime& time)
{
}


// KeyJoystickDevice
byte KeyJoystick::read(const EmuTime &time)
{
	return status;
}

void KeyJoystick::write(byte value, const EmuTime &time)
{
	//do nothing
}


// EventListener
bool KeyJoystick::signalEvent(SDL_Event &event)
{
	switch (event.type) {
	case SDL_KEYDOWN:
		switch (event.key.keysym.sym) {
		case Keys::K_UP:
			status &= ~JOY_UP;
			break;
		case Keys::K_DOWN:
			status &= ~JOY_DOWN;
			break;
		case Keys::K_LEFT:
			status &= ~JOY_LEFT;
			break;
		case Keys::K_RIGHT:
			status &= ~JOY_RIGHT;
			break;
		case Keys::K_SPACE:
			status &= ~JOY_BUTTONA;
			break;
		case Keys::K_LALT:
			status &= ~JOY_BUTTONB;
			break;
		default:
			// nothing
			break;
		}
		break;
	case SDL_KEYUP:
		switch (event.key.keysym.sym) {
		case Keys::K_UP: 
			status |= JOY_UP;
			break;
		case Keys::K_DOWN:
			status |= JOY_DOWN;
			break;
		case Keys::K_LEFT:
			status |= JOY_LEFT;
			break;
		case Keys::K_RIGHT:
			status |= JOY_RIGHT;
			break;
		case Keys::K_SPACE:
			status |= JOY_BUTTONA;
			break;
		case Keys::K_LALT:
			status |= JOY_BUTTONB;
			break;
		default:
			// nothing
			break;
		}
		break;
	default:
		assert(false);
	}
	return true;
}
