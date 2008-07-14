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

//Pluggable
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

	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

void Joystick::unplugHelper(const EmuTime& /*time*/)
{
	eventDistributor.unregisterEventListener(*this);
}


//JoystickDevice
byte Joystick::read(const EmuTime& /*time*/)
{
	return status;
}

void Joystick::write(byte /*value*/, const EmuTime& /*time*/)
{
	//do nothing
}

//EventListener
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
		switch (buttonEvent.getButton() % 2) {
		case 0:
			status &= ~JOY_BUTTONA;
			break;
		case 1:
			status &= ~JOY_BUTTONB;
			break;
		default:
			// ignore other buttons
			break;
		}
		break;
	}
	case OPENMSX_JOY_BUTTON_UP_EVENT: {
		const JoystickButtonEvent& buttonEvent =
			checked_cast<const JoystickButtonEvent&>(*event);
		switch (buttonEvent.getButton() % 2) {
		case 0:
			status |= JOY_BUTTONA;
			break;
		case 1:
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
}

template<typename Archive>
void Joystick::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't serialize 'status', is controlled by joystick ibutton/motion events
}
INSTANTIATE_SERIALIZE_METHODS(Joystick);

} // namespace openmsx
