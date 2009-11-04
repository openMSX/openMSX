// $Id$

#include "Joystick.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "MSXEventDistributor.hh"
#include "StateChangeDistributor.hh"
#include "InputEvents.hh"
#include "StateChange.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include "serialize_meta.hh"
#include "unreachable.hh"

using std::string;

namespace openmsx {

static const int THRESHOLD = 32768 / 10;

void Joystick::registerAll(MSXEventDistributor& eventDistributor,
                           StateChangeDistributor& stateChangeDistributor,
                           PluggingController& controller)
{
#ifdef SDL_JOYSTICK_DISABLED
	(void)eventDistributor;
	(void)stateChangeDistributor;
	(void)controller;
#else
	if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
		SDL_JoystickEventState(SDL_ENABLE); // joysticks generate events
	}

	unsigned numJoysticks = SDL_NumJoysticks();
	for (unsigned i = 0; i < numJoysticks; i++) {
		controller.registerPluggable(new Joystick(
			eventDistributor, stateChangeDistributor, i));
	}
#endif
}


class JoyState : public StateChange
{
public:
	JoyState(EmuTime::param time, unsigned joyNum_, byte press_, byte release_)
		: StateChange(time)
		, joyNum(joyNum_), press(press_), release(release_)
	{
		assert((press != 0) || (release != 0));
		assert((press & release) == 0);
	}
	unsigned getJoystick() const { return joyNum; }
	byte     getPress()    const { return press; }
	byte     getRelease()  const { return release; }
private:
	const unsigned joyNum;
	const byte press, release;
};


#ifndef SDL_JOYSTICK_DISABLED
// Note: It's OK to open/close the same SDL_Joystick multiple times (we open it
// once per MSX machine). The SDL documentation doesn't state this, but I
// checked the implementation and a SDL_Joystick uses a 'reference count' on
// the open/close calls.
Joystick::Joystick(MSXEventDistributor& eventDistributor_,
                   StateChangeDistributor& stateChangeDistributor_,
                   unsigned joyNum_)
	: eventDistributor(eventDistributor_)
	, stateChangeDistributor(stateChangeDistributor_)
	, name(string("joystick") + char('1' + joyNum_))
	, desc(string(SDL_JoystickName(joyNum_)))
	, joystick(SDL_JoystickOpen(joyNum_))
	, joyNum(joyNum_)
{
}

Joystick::~Joystick()
{
	if (isPluggedIn()) {
		Joystick::unplugHelper(EmuTime::dummy());
	}
	if (joystick) {
		SDL_JoystickClose(joystick);
	}
}

// Pluggable
const string& Joystick::getName() const
{
	return name;
}

const string& Joystick::getDescription() const
{
	return desc;
}

void Joystick::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	if (!joystick) {
		throw PlugException("Failed to open joystick device");
	}
	plugHelper2();
	calcInitialState();
}

void Joystick::plugHelper2()
{
	eventDistributor.registerEventListener(*this);
	stateChangeDistributor.registerListener(*this);
}

void Joystick::unplugHelper(EmuTime::param /*time*/)
{
	stateChangeDistributor.unregisterListener(*this);
	eventDistributor.unregisterEventListener(*this);
}


// JoystickDevice
byte Joystick::read(EmuTime::param /*time*/)
{
	return status;
}

void Joystick::write(byte /*value*/, EmuTime::param /*time*/)
{
	// nothing
}

void Joystick::calcInitialState()
{
	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;

	if (joystick) {
		int xAxis = SDL_JoystickGetAxis(joystick, 0);
		if (xAxis < -THRESHOLD) {
			status &= ~JOY_LEFT;
		} else if (xAxis > THRESHOLD) {
			status &= ~JOY_RIGHT;
		}

		int yAxis = SDL_JoystickGetAxis(joystick, 1);
		if (yAxis < -THRESHOLD) {
			status &= ~JOY_UP;
		} else if (yAxis > THRESHOLD) {
			status &= ~JOY_DOWN;
		}

		int numButtons = SDL_JoystickNumButtons(joystick);
		for (int button = 0; button < numButtons; ++button) {
			if (SDL_JoystickGetButton(joystick, button)) {
				if (button & 1) {
					status &= ~JOY_BUTTONB;
				} else {
					status &= ~JOY_BUTTONA;
				}
			}
		}
	}
}

// MSXEventListener
void Joystick::signalEvent(shared_ptr<const Event> event, EmuTime::param time)
{
	const JoystickEvent* joyEvent =
		dynamic_cast<const JoystickEvent*>(event.get());
	if (!joyEvent) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	if (joyEvent->getJoystick() != joyNum) return;

	switch (event->getType()) {
	case OPENMSX_JOY_AXIS_MOTION_EVENT: {
		const JoystickAxisMotionEvent& motionEvent =
			checked_cast<const JoystickAxisMotionEvent&>(*event);
		short value = motionEvent.getValue();
		switch (motionEvent.getAxis()) {
		case JoystickAxisMotionEvent::X_AXIS: // Horizontal
			if (value < -THRESHOLD) {
				// left, not right
				createEvent(time, joyNum, JOY_LEFT, JOY_RIGHT);
			} else if (value > THRESHOLD) {
				// not left, right
				createEvent(time, joyNum, JOY_RIGHT, JOY_LEFT);
			} else {
				// not left, not right
				createEvent(time, joyNum, 0, JOY_LEFT | JOY_RIGHT);
			}
			break;
		case JoystickAxisMotionEvent::Y_AXIS: // Vertical
			if (value < -THRESHOLD) {
				// up, not down
				createEvent(time, joyNum, JOY_UP, JOY_DOWN);
			} else if (value > THRESHOLD) {
				// not up, down
				createEvent(time, joyNum, JOY_DOWN, JOY_UP);
			} else {
				// not up, not down
				createEvent(time, joyNum, 0, JOY_UP | JOY_DOWN);
			}
			break;
		default:
			// ignore other axis
			break;
		}
		break;
	}
	case OPENMSX_JOY_BUTTON_DOWN_EVENT: {
		const JoystickButtonEvent& buttonEvent =
			checked_cast<const JoystickButtonEvent&>(*event);
		if (buttonEvent.getButton() & 1) {
			createEvent(time, joyNum, JOY_BUTTONB, 0);
		} else {
			createEvent(time, joyNum, JOY_BUTTONA, 0);
		}
		break;
	}
	case OPENMSX_JOY_BUTTON_UP_EVENT: {
		const JoystickButtonEvent& buttonEvent =
			checked_cast<const JoystickButtonEvent&>(*event);
		if (buttonEvent.getButton() & 1) {
			createEvent(time, joyNum, 0, JOY_BUTTONB);
		} else {
			createEvent(time, joyNum, 0, JOY_BUTTONA);
		}
		break;
	}
	default:
		UNREACHABLE;
	}
}

void Joystick::createEvent(EmuTime::param time, int joyNum, byte press, byte release)
{
	byte newStatus = (status & ~press) | release;
	byte diff = status ^ newStatus;
	if (!diff) {
		// event won't actually change the status, so ignore it
		return;
	}
	// make sure we create an event with minimal changes
	press   =    status & diff;
	release = newStatus & diff;
	stateChangeDistributor.distribute(shared_ptr<const StateChange>(
		new JoyState(time, joyNum, press, release)));
}

// StateChangeListener
void Joystick::signalStateChange(shared_ptr<const StateChange> event)
{
	const JoyState* js = dynamic_cast<const JoyState*>(event.get());
	if (!js) return;

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	// TODO an alternative is to log events based on the connector instead
	//      of the joystick. That would make it possible to replay on a
	//      different host without an actual SDL joystick connected.
	if (js->getJoystick() != joyNum) return;

	status &= js->getPress();
	status |= js->getRelease();
}

// version 1: Initial version, the variable status was not serialized.
// version 2: Also serialize the above variable, this is required for
//            record/replay, see comment in Keyboard.cc for more details.
template<typename Archive>
void Joystick::serialize(Archive& ar, unsigned version)
{
	if (version >= 2) {
		ar.serialize("status", status);
	}
	if (ar.isLoader()) {
		if (joystick && isPluggedIn()) {
			plugHelper2();
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Joystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Joystick, "Joystick");

#endif // SDL_JOYSTICK_DISABLED

} // namespace openmsx
