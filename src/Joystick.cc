
#include "Joystick.hh"
#include <assert.h>


Joystick::Joystick(int joyNum)
{
	this->joyNum = joyNum;
	PRT_DEBUG("Creating a Joystick object for joystick "<<joyNum);
	SDL_InitSubSystem(SDL_INIT_JOYSTICK);
	SDL_JoystickEventState(SDL_ENABLE);	// joysticks generate events
	if (SDL_NumJoysticks() > joyNum) {
		PRT_DEBUG("Opening joystick "<<SDL_JoystickName(joyNum));
		joystick = SDL_JoystickOpen(joyNum);
		EventDistributor::instance()->registerListener(SDL_JOYAXISMOTION, this);
		EventDistributor::instance()->registerListener(SDL_JOYBUTTONDOWN, this);
		EventDistributor::instance()->registerListener(SDL_JOYBUTTONUP,   this);
	} else {
		PRT_DEBUG("No joystick nummer "<<joyNum);
	}
}

Joystick::~Joystick()
{
	SDL_JoystickClose(joystick);
}

//JoystickDevice
byte Joystick::read()
{
	return status;
}

void Joystick::write(byte value)
{
	//do nothing
}

//EventListener
void Joystick::signalEvent(SDL_Event &event)
{
	if (event.jaxis.which != joyNum)	// also event.jbutton.which
		return;
	switch (event.type) {
	case SDL_JOYAXISMOTION:
		switch (event.jaxis.axis) {
		case 0: // Horizontal
			if (event.jaxis.value < -TRESHOLD) {
				status &= ~JOY_LEFT;	// left pressed
				status |=  JOY_RIGHT;	// right not pressed
			} else if (event.jaxis.value > TRESHOLD) {
				status |=  JOY_LEFT;	// left not pressed
				status &= ~JOY_RIGHT;	// right pressed
			} else {
				status |=  JOY_LEFT | JOY_RIGHT;	// left nor right pressed
			}
			break;
		case 1: // Vertical
			if (event.jaxis.value < -TRESHOLD) {
				status &= ~JOY_DOWN;	// down pressed
				status |=  JOY_UP;	// up not pressed
			} else if (event.jaxis.value > TRESHOLD) {
				status |=  JOY_DOWN;	// down not pressed
				status &= ~JOY_UP;	// up pressed
			} else {
				status |=  JOY_DOWN | JOY_UP;	// down nor up pressed
			}
			break;
		default:
			// ignore other axis
			break;
		}
		break;
	case SDL_JOYBUTTONDOWN:
		switch (event.jbutton.button) {
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
	case SDL_JOYBUTTONUP:
		switch (event.jbutton.button) {
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
	default:
		assert(false);
	}
}
