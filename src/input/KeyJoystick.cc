// $Id$

#include "KeyJoystick.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "KeyCodeSetting.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include <cassert>

using std::string;

namespace openmsx {

class KeyJoyState : public StateChange
{
public:
	KeyJoyState() {} // for serialize
	KeyJoyState(EmuTime::param time, const string& name_,
	            byte press_, byte release_)
		: StateChange(time)
		, name(name_), press(press_), release(release_) {}
	const string& getName() const { return name; }
	byte getPress()   const { return press; }
	byte getRelease() const { return release; }
	template<typename Archive> void serialize(Archive& ar, unsigned /*version*/)
	{
		ar.template serializeBase<StateChange>(*this);
		ar.serialize("name", name);
		ar.serialize("press", press);
		ar.serialize("release", release);
	}

private:
	string name;
	byte press, release;
};
REGISTER_POLYMORPHIC_CLASS(StateChange, KeyJoyState, "KeyJoyState");

KeyJoystick::KeyJoystick(CommandController& commandController,
                         MSXEventDistributor& eventDistributor_,
                         StateChangeDistributor& stateChangeDistributor_,
                         const string& name_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
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
	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

KeyJoystick::~KeyJoystick()
{
	if (isPluggedIn()) {
		KeyJoystick::unplugHelper(EmuTime::dummy());
	}
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

void KeyJoystick::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void KeyJoystick::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
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


// MSXEventListener
void KeyJoystick::signalEvent(shared_ptr<const Event> event,
                              EmuTime::param time)
{
	byte press = 0;
	byte release = 0;
	switch (event->getType()) {
	case OPENMSX_KEY_DOWN_EVENT:
	case OPENMSX_KEY_UP_EVENT: {
		const KeyEvent& keyEvent = checked_cast<const KeyEvent&>(*event);
		Keys::KeyCode key = static_cast<Keys::KeyCode>(
			int(keyEvent.getKeyCode()) & int(Keys::K_MASK));
		if (event->getType() == OPENMSX_KEY_DOWN_EVENT) {
			if      (key == up->getValue())    press   = JOY_UP;
			else if (key == down->getValue())  press   = JOY_DOWN;
			else if (key == left->getValue())  press   = JOY_LEFT;
			else if (key == right->getValue()) press   = JOY_RIGHT;
			else if (key == trigA->getValue()) press   = JOY_BUTTONA;
			else if (key == trigB->getValue()) press   = JOY_BUTTONB;
		} else {
			if      (key == up->getValue())    release = JOY_UP;
			else if (key == down->getValue())  release = JOY_DOWN;
			else if (key == left->getValue())  release = JOY_LEFT;
			else if (key == right->getValue()) release = JOY_RIGHT;
			else if (key == trigA->getValue()) release = JOY_BUTTONA;
			else if (key == trigB->getValue()) release = JOY_BUTTONB;
		}
		break;
	}
	default:
		// ignore
		break;
	}

	if (((status & ~press) | release) != status) {
		stateChangeDistributor.distributeNew(shared_ptr<StateChange>(
			new KeyJoyState(time, name, press, release)));
	}
}

// StateChangeListener
void KeyJoystick::signalStateChange(shared_ptr<StateChange> event)
{
	const KeyJoyState* kjs = dynamic_cast<const KeyJoyState*>(event.get());
	if (!kjs) return;
	if (kjs->getName() != name) return;

	status = (status & ~kjs->getPress()) | kjs->getRelease();
}

void KeyJoystick::stopReplay(EmuTime::param time)
{
	// TODO read actual host key state
	byte newStatus = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	                 JOY_BUTTONA | JOY_BUTTONB;
	if (newStatus != status) {
		byte release = newStatus & ~status;
		stateChangeDistributor.distributeNew(shared_ptr<StateChange>(
			new KeyJoyState(time, name, 0, release)));
	}
}


// version 1: Initial version, the variable status was not serialized.
// version 2: Also serialize the above variable, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
template<typename Archive>
void KeyJoystick::serialize(Archive& ar, unsigned version)
{
	if (version >= 2) {
		ar.serialize("status", status);
	}
	if (ar.isLoader() && isPluggedIn()) {
		plugHelper(*getConnector(), EmuTime::dummy());
	}
}
INSTANTIATE_SERIALIZE_METHODS(KeyJoystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, KeyJoystick, "KeyJoystick");

} // namespace openmsx
