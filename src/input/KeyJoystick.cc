// $Id$

#include <cassert>
#include <iostream>
#include "KeyJoystick.hh"
#include "EventDistributor.hh"
#include "Keys.hh"
#include "SettingsConfig.hh"
#include "CliCommOutput.hh"
#include "InputEvents.hh"

namespace openmsx {

static Keys::KeyCode getConfigKeyCode(const string& keyname,
                                      const XMLElement& config)
{
	Keys::KeyCode testKey = Keys::K_NONE;
	const XMLElement* keyElem = config.getChild(keyname);
	if (keyElem) {
		string key = keyElem->getData();
		testKey = Keys::getCode(key);
		if (testKey == Keys::K_NONE) {
			CliCommOutput::instance().printWarning(
				"unknown keycode \"" + key + "\" for key \"" +
				keyname + "\" in KeyJoystick configuration");
		}
	}
	return testKey;
}

KeyJoystick::KeyJoystick()
{
	EventDistributor::instance().registerEventListener(KEY_DOWN_EVENT, *this);
	EventDistributor::instance().registerEventListener(KEY_UP_EVENT  , *this);

	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;

	// built in defaults: no working key joystick
	upKey      = Keys::K_NONE;
	rightKey   = Keys::K_NONE;
	downKey    = Keys::K_NONE;
	leftKey    = Keys::K_NONE;
	buttonAKey = Keys::K_NONE;
	buttonBKey = Keys::K_NONE;

	const XMLElement* config = SettingsConfig::instance().findConfigById("KeyJoystick");
	if (config) {
		upKey      = getConfigKeyCode("upkey",      *config);
		rightKey   = getConfigKeyCode("rightkey",   *config);
		downKey    = getConfigKeyCode("downkey",    *config);
		leftKey    = getConfigKeyCode("leftkey",    *config);
		buttonAKey = getConfigKeyCode("buttonakey", *config);
		buttonBKey = getConfigKeyCode("buttonbkey", *config);
	} else {
		CliCommOutput::instance().printWarning(
			"KeyJoystick not configured, so it won't be usable...");
	}
}

KeyJoystick::~KeyJoystick()
{
	EventDistributor::instance().unregisterEventListener(KEY_DOWN_EVENT, *this);
	EventDistributor::instance().unregisterEventListener(KEY_UP_EVENT  , *this);
}

// Pluggable
const string& KeyJoystick::getName() const
{
	static const string name("keyjoystick");
	return name;
}

const string& KeyJoystick::getDescription() const
{
	static const string desc(
		"Key-Joystick, use your keyboard to emulate an MSX joystick. "
		"See manual for information on how to configure this.");
	return desc;
}

void KeyJoystick::plugHelper(Connector* connector, const EmuTime& time)
{
}

void KeyJoystick::unplugHelper(const EmuTime& time)
{
}


// KeyJoystickDevice
byte KeyJoystick::read(const EmuTime& time)
{
	return status;
}

void KeyJoystick::write(byte value, const EmuTime& time)
{
	//do nothing
}


// EventListener
bool KeyJoystick::signalEvent(const Event& event)
{
	assert(dynamic_cast<const KeyEvent*>(&event));
	Keys::KeyCode key = (Keys::KeyCode)((int)((KeyEvent&)event).getKeyCode() &
		                            (int)Keys::K_MASK);
	switch (event.getType()) {
	case KEY_DOWN_EVENT:
		if      (key == upKey)      status &= ~JOY_UP;
		else if (key == downKey)    status &= ~JOY_DOWN;
		else if (key == leftKey)    status &= ~JOY_LEFT;
		else if (key == rightKey)   status &= ~JOY_RIGHT;
		else if (key == buttonAKey) status &= ~JOY_BUTTONA;
		else if (key == buttonBKey) status &= ~JOY_BUTTONB;
		break;
	case KEY_UP_EVENT:
		if      (key == upKey)      status |= JOY_UP;
		else if (key == downKey)    status |= JOY_DOWN;
		else if (key == leftKey)    status |= JOY_LEFT;
		else if (key == rightKey)   status |= JOY_RIGHT;
		else if (key == buttonAKey) status |= JOY_BUTTONA;
		else if (key == buttonBKey) status |= JOY_BUTTONB;
		break;
	default:
		assert(false);
	}
	return true;
}

} // namespace openmsx
