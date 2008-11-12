// $Id$

#include "Joystick.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "MSXEventDistributor.hh"
#include "InputEvents.hh"
#include "checked_cast.hh"
#include "serialize.hh"
#include <cassert>

using std::string;

namespace openmsx {

static const int THRESHOLD = 32768 / 10;

void Joystick::registerAll(MSXEventDistributor& eventDistributor,
                           PluggingController& controller)
{
	if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
		SDL_JoystickEventState(SDL_ENABLE); // joysticks generate events
	}

	unsigned numJoysticks = SDL_NumJoysticks();
	for (unsigned i = 0; i < numJoysticks; i++) {
		controller.registerPluggable(new Joystick(eventDistributor, i));
	}
}

// Note: It's OK to open/close the same SDL_Joystick multiple times (we open it
// once per MSX machine). The SDL documentation doesn't state this, but I
// checked the implementation and a SDL_Joystick uses a 'reference count' on
// the open/close calls.
Joystick::Joystick(MSXEventDistributor& eventDistributor_, unsigned joyNum_)
	: eventDistributor(eventDistributor_)
	, name(string("joystick") + char('1' + joyNum_))
	, desc(string(SDL_JoystickName(joyNum_)))
	, joystick(SDL_JoystickOpen(joyNum_))
	, joyNum(joyNum_)
{
}

Joystick::~Joystick()
{
	if (getConnector()) {
		// still plugged in
		eventDistributor.unregisterEventListener(*this);
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

void Joystick::plugHelper(Connector& /*connector*/, const EmuTime& /*time*/)
{
	if (!joystick) {
		throw PlugException("Failed to open joystick device");
	}

	eventDistributor.registerEventListener(*this);

	calcInitialState();
}

void Joystick::unplugHelper(const EmuTime& /*time*/)
{
	eventDistributor.unregisterEventListener(*this);
}


// JoystickDevice
byte Joystick::read(const EmuTime& /*time*/)
{
	return status;
}

void Joystick::write(byte /*value*/, const EmuTime& /*time*/)
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

// EventListener
void Joystick::signalEvent(shared_ptr<const Event> event, const EmuTime& /*time*/)
{
	const JoystickEvent* joyEvent =
		dynamic_cast<const JoystickEvent*>(event.get());
	if (!joyEvent) {
		return;
	}

	// TODO: It would be more efficient to make a dispatcher instead of
	//       sending the event to all joysticks.
	if (joyEvent->getJoystick() != joyNum) {
		return;
	}

	switch (event->getType()) {
	case OPENMSX_JOY_AXIS_MOTION_EVENT: {
		const JoystickAxisMotionEvent& motionEvent =
			checked_cast<const JoystickAxisMotionEvent&>(*event);
		short value = motionEvent.getValue();
		switch (motionEvent.getAxis()) {
		case JoystickAxisMotionEvent::X_AXIS: // Horizontal
			if (value < -THRESHOLD) {
				status &= ~JOY_LEFT;	// left      pressed
				status |=  JOY_RIGHT;	// right not pressed
			} else if (value > THRESHOLD) {
				status |=  JOY_LEFT;	// left  not pressed
				status &= ~JOY_RIGHT;	// right     pressed
			} else {
				status |=  JOY_LEFT;;	// left  not pressed
				status |=  JOY_RIGHT;	// right not pressed
			}
			break;
		case JoystickAxisMotionEvent::Y_AXIS: // Vertical
			if (value < -THRESHOLD) {
				status |=  JOY_DOWN;	// down not pressed
				status &= ~JOY_UP;	// up       pressed
			} else if (value > THRESHOLD) {
				status &= ~JOY_DOWN;	// down     pressed
				status |=  JOY_UP;	// up   not pressed
			} else {
				status |=  JOY_DOWN;	// down not pressed
				status |=  JOY_UP;	// up   not pressed
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
			status &= ~JOY_BUTTONB;
		} else {
			status &= ~JOY_BUTTONA;
		}
		break;
	}
	case OPENMSX_JOY_BUTTON_UP_EVENT: {
		const JoystickButtonEvent& buttonEvent =
			checked_cast<const JoystickButtonEvent&>(*event);
		if (buttonEvent.getButton() & 1) {
			status |= JOY_BUTTONB;
		} else {
			status |= JOY_BUTTONA;
		}
		break;
	}
	default:
		assert(false);
	}
}

template<typename Archive>
void Joystick::serialize(Archive& ar, unsigned /*version*/)
{
	// don't serialize 'status':
	// it should be based on the 'current' joystick state
	if (ar.isLoader()) {
		Connector* conn = getConnector();
		if (joystick && conn) {
			EmuTime* dummy = 0;
			plugHelper(*conn, *dummy);
		}
	}
}
INSTANTIATE_SERIALIZE_METHODS(Joystick);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, Joystick, "Joystick");

} // namespace openmsx
