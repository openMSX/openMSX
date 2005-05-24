// $Id$

#include <cassert>
#include <iostream>
#include "KeyJoystick.hh"
#include "EventDistributor.hh"
#include "Keys.hh"
#include "SettingsConfig.hh"
#include "CliComm.hh"
#include "InputEvents.hh"

using std::string;

namespace openmsx {

static Keys::KeyCode getConfigKeyCode(const string& keyname,
                                      const string& defaultValue,
                                      XMLElement& config)
{
	XMLElement& keyElem = config.getCreateChild(keyname, defaultValue);
	string key = keyElem.getData();
	Keys::KeyCode testKey = Keys::getCode(key);
	if (testKey == Keys::K_NONE) {
		CliComm::instance().printWarning(
			"unknown keycode \"" + key + "\" for key \"" +
			keyname + "\" in KeyJoystick configuration");
	}
	return testKey;
}

KeyJoystick::KeyJoystick()
	: keyJoyConfig(SettingsConfig::instance().getCreateChild("KeyJoystick"))
{
	EventDistributor & distributor(EventDistributor::instance());
	distributor.registerEventListener(OPENMSX_KEY_DOWN_EVENT,   *this);
	distributor.registerEventListener(OPENMSX_KEY_UP_EVENT,     *this);
	distributor.registerEventListener(OPENMSX_CONSOLE_ON_EVENT, *this);
	// We do not listen for CONSOLE_OFF_EVENTS because rescanning the

	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;

	readKeys();
	keyJoyConfig.addListener(*this);
}

KeyJoystick::~KeyJoystick()
{
	keyJoyConfig.removeListener(*this);

	EventDistributor & distributor(EventDistributor::instance());
	distributor.unregisterEventListener(OPENMSX_KEY_UP_EVENT,   *this);
	distributor.unregisterEventListener(OPENMSX_KEY_DOWN_EVENT, *this);
	distributor.unregisterEventListener(OPENMSX_CONSOLE_ON_EVENT, *this);
}

void KeyJoystick::readKeys()
{
	upKey      = getConfigKeyCode("upkey",      "UP",    keyJoyConfig);
	rightKey   = getConfigKeyCode("rightkey",   "RIGHT", keyJoyConfig);
	downKey    = getConfigKeyCode("downkey",    "DOWN",  keyJoyConfig);
	leftKey    = getConfigKeyCode("leftkey",    "LEFT",  keyJoyConfig);
	buttonAKey = getConfigKeyCode("buttonakey", "SPACE", keyJoyConfig);
	buttonBKey = getConfigKeyCode("buttonbkey", "M",     keyJoyConfig);
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

void KeyJoystick::allUp()
{
	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

void KeyJoystick::plugHelper(Connector* /*connector*/, const EmuTime& /*time*/)
{
}

void KeyJoystick::unplugHelper(const EmuTime& /*time*/)
{
}


// KeyJoystickDevice
byte KeyJoystick::read(const EmuTime& /*time*/)
{
	return status;
}

void KeyJoystick::write(byte /*value*/, const EmuTime& /*time*/)
{
	//do nothing
}


// EventListener
bool KeyJoystick::signalEvent(const Event& event)
{
	switch (event.getType()) {
	case OPENMSX_CONSOLE_ON_EVENT: 
		allUp();
		break;
	default: // must be keyEvent 
		assert(dynamic_cast<const KeyEvent*>(&event));
		Keys::KeyCode key = (Keys::KeyCode)((int)((KeyEvent&)event).getKeyCode() &
		                                    (int)Keys::K_MASK);
		switch (event.getType()) {
		case OPENMSX_KEY_DOWN_EVENT:
			if      (key == upKey)      status &= ~JOY_UP;
			else if (key == downKey)    status &= ~JOY_DOWN;
			else if (key == leftKey)    status &= ~JOY_LEFT;
			else if (key == rightKey)   status &= ~JOY_RIGHT;
			else if (key == buttonAKey) status &= ~JOY_BUTTONA;
			else if (key == buttonBKey) status &= ~JOY_BUTTONB;
			break;
		case OPENMSX_KEY_UP_EVENT:
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
	}
	return true;
}

// XMLElementListener
void KeyJoystick::updateData(const XMLElement& /*element*/)
{
	readKeys();
}

void KeyJoystick::childAdded(const XMLElement& /*parent*/,
                             const XMLElement& /*child*/)
{
	readKeys();
}

} // namespace openmsx
