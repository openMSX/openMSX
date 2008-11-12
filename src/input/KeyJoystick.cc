// $Id$

#include "KeyJoystick.hh"
#include "MSXEventDistributor.hh"
#include "KeyCodeSetting.hh"
#include "InputEvents.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <cassert>

using std::string;

namespace openmsx {

KeyJoystick::KeyJoystick(CommandController& commandController,
                         MSXEventDistributor& eventDistributor_,
                         const string& name_)
	: eventDistributor(eventDistributor_)
	, name(name_)
	, up   (new KeyCodeSetting(commandController, name + ".up",
		"key for direction up",    Keys::K_UP))
	, down (new KeyCodeSetting(commandController, name + ".down",
		"key for direction down",  Keys::K_DOWN))
	, left (new KeyCodeSetting(commandController, name + ".left",
		"key for direction left",  Keys::K_LEFT))
	, right(new KeyCodeSetting(commandController, name + ".right",
		"key for direction right", Keys::K_RIGHT))
	, trigA(new KeyCodeSetting(commandController, name + ".triga",
		"key for trigger A",       Keys::K_SPACE))
	, trigB(new KeyCodeSetting(commandController, name + ".trigb",
		"key for trigger B",       Keys::K_M))
{
	eventDistributor.registerEventListener(*this);

	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

KeyJoystick::~KeyJoystick()
{
	eventDistributor.unregisterEventListener(*this);
}


// Pluggable
const string& KeyJoystick::getName() const
{
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

void KeyJoystick::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
}

void KeyJoystick::unplugHelper(EmuTime::param /*time*/)
{
}


// KeyJoystickDevice
byte KeyJoystick::read(EmuTime::param /*time*/)
{
	return status;
}

void KeyJoystick::write(byte /*value*/, EmuTime::param /*time*/)
{
	// nothing
}


// EventListener
void KeyJoystick::signalEvent(shared_ptr<const Event> event,
                              EmuTime::param /*time*/)
{
	switch (event->getType()) {
	case OPENMSX_CONSOLE_ON_EVENT:
		allUp();
		break;
	case OPENMSX_KEY_DOWN_EVENT:
	case OPENMSX_KEY_UP_EVENT: {
		const KeyEvent& keyEvent = checked_cast<const KeyEvent&>(*event);
		Keys::KeyCode key = static_cast<Keys::KeyCode>(
			int(keyEvent.getKeyCode()) & int(Keys::K_MASK));
		if (event->getType() == OPENMSX_KEY_DOWN_EVENT) {
			if      (key == up->getValue())    status &= ~JOY_UP;
			else if (key == down->getValue())  status &= ~JOY_DOWN;
			else if (key == left->getValue())  status &= ~JOY_LEFT;
			else if (key == right->getValue()) status &= ~JOY_RIGHT;
			else if (key == trigA->getValue()) status &= ~JOY_BUTTONA;
			else if (key == trigB->getValue()) status &= ~JOY_BUTTONB;
		} else {
			if      (key == up->getValue())    status |= JOY_UP;
			else if (key == down->getValue())  status |= JOY_DOWN;
			else if (key == left->getValue())  status |= JOY_LEFT;
			else if (key == right->getValue()) status |= JOY_RIGHT;
			else if (key == trigA->getValue()) status |= JOY_BUTTONA;
			else if (key == trigB->getValue()) status |= JOY_BUTTONB;
		}
		break;
	}
	default:
		// ignore
		break;
	}
}

template<typename Archive>
void KeyJoystick::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't serialize 'status', is controlled by key events
}
INSTANTIATE_SERIALIZE_METHODS(KeyJoystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, KeyJoystick, "KeyJoystick");

} // namespace openmsx
