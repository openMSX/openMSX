// $Id$

#include <cassert>
#include "KeyJoystick.hh"
#include "PluggingController.hh"
#include "EventDistributor.hh"
#include "Keys.hh"
#include "MSXConfig.hh"

namespace openmsx {

KeyJoystick::KeyJoystick()
{
	EventDistributor::instance()->registerEventListener(SDL_KEYDOWN, this, 1);
	EventDistributor::instance()->registerEventListener(SDL_KEYUP  , this, 1);

	PluggingController::instance()->registerPluggable(this);

	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;

	try {
		Config *config = MSXConfig::instance()->getConfigById("KeyJoystick");
		upKey=Keys::getCode(config->getParameter("upkey"));
		rightKey=Keys::getCode(config->getParameter("rightkey"));
		downKey=Keys::getCode(config->getParameter("downkey"));
		leftKey=Keys::getCode(config->getParameter("leftkey"));
		buttonAKey=Keys::getCode(config->getParameter("buttonakey"));
		buttonBKey=Keys::getCode(config->getParameter("buttonbkey"));
	} catch (ConfigException &e) {
		PRT_INFO("KeyJoystick not (completely) configured, using default settings...");
		upKey=Keys::K_UP;
		rightKey=Keys::K_RIGHT;
		downKey=Keys::K_DOWN;
		leftKey=Keys::K_LEFT;
		buttonAKey=Keys::K_SPACE;
		buttonBKey=Keys::K_LALT;
	}
}

KeyJoystick::~KeyJoystick()
{
	PluggingController::instance()->unregisterPluggable(this);

	EventDistributor::instance()->unregisterEventListener(SDL_KEYDOWN, this, 1);
	EventDistributor::instance()->unregisterEventListener(SDL_KEYUP  , this, 1);
}

// Pluggable
const string &KeyJoystick::getName() const
{
	static string name("keyjoystick");
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
	Keys::KeyCode theKey = Keys::getCode(event.key.keysym.sym);
	switch (event.type) {
	case SDL_KEYDOWN:
		if (theKey==upKey) status &= ~JOY_UP;
		else if (theKey==downKey) status &= ~JOY_DOWN;
		else if (theKey==leftKey) status &= ~JOY_LEFT;
		else if (theKey==rightKey) status &= ~JOY_RIGHT;
		else if (theKey==buttonAKey) status &= ~JOY_BUTTONA;
		else if (theKey==buttonBKey) status &= ~JOY_BUTTONB;
		break;
	case SDL_KEYUP:
		if (theKey==upKey) status |= JOY_UP;
		else if (theKey==downKey) status |= JOY_DOWN;
		else if (theKey==leftKey) status |= JOY_LEFT;
		else if (theKey==rightKey) status |= JOY_RIGHT;
		else if (theKey==buttonAKey) status |= JOY_BUTTONA;
		else if (theKey==buttonBKey) status |= JOY_BUTTONB;
		break;
	default:
		assert(false);
	}
	return true;
}

} // namespace openmsx
