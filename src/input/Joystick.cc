// $Id$

#include <cassert>
#include "Joystick.hh"
#include "PluggingController.hh"
#include "EventDistributor.hh"


Joystick::Joystick(int joyNum_)
	: joyNum(joyNum_)
{
	PRT_DEBUG("Creating a Joystick object for joystick " << joyNum);

	if (!SDL_WasInit(SDL_INIT_JOYSTICK)) {
		SDL_InitSubSystem(SDL_INIT_JOYSTICK);
		SDL_JoystickEventState(SDL_ENABLE);	// joysticks generate events
	}

	if (SDL_NumJoysticks() <= joyNum)
		throw JoystickException("No such joystick number");

	name = std::string("joystick")+(char)('1'+joyNum);

	PRT_DEBUG("Opening joystick " << SDL_JoystickName(joyNum));
	joystick = SDL_JoystickOpen(joyNum);
	EventDistributor::instance()->registerEventListener(SDL_JOYAXISMOTION, this);
	EventDistributor::instance()->registerEventListener(SDL_JOYBUTTONDOWN, this);
	EventDistributor::instance()->registerEventListener(SDL_JOYBUTTONUP,   this);

	PluggingController::instance()->registerPluggable(this);
	
	status = JOY_UP | JOY_DOWN | JOY_LEFT | JOY_RIGHT |
	         JOY_BUTTONA | JOY_BUTTONB;
}

Joystick::~Joystick()
{
	PluggingController::instance()->unregisterPluggable(this);
	EventDistributor::instance()->unregisterEventListener(SDL_JOYAXISMOTION, this);
	EventDistributor::instance()->unregisterEventListener(SDL_JOYBUTTONDOWN, this);
	EventDistributor::instance()->unregisterEventListener(SDL_JOYBUTTONUP,   this);
	SDL_JoystickClose(joystick);
}

//Pluggable
const std::string &Joystick::getName() const
{
	return name;
}


//JoystickDevice
byte Joystick::read(const EmuTime &time)
{
	return status;
}

void Joystick::write(byte value, const EmuTime &time)
{
	//do nothing
}

//EventListener
bool Joystick::signalEvent(SDL_Event &event)
{
	if (event.jaxis.which == joyNum) {	// also event.jbutton.which
		switch (event.type) {
		case SDL_JOYAXISMOTION:
			switch (event.jaxis.axis) {
			case 0: // Horizontal
				if (event.jaxis.value < -THRESHOLD) {
					status &= ~JOY_LEFT;	// left      pressed
					status |=  JOY_RIGHT;	// right not pressed
				} else if (event.jaxis.value > THRESHOLD) {
					status |=  JOY_LEFT;	// left  not pressed
					status &= ~JOY_RIGHT;	// right     pressed
				} else {
					status |=  JOY_LEFT;;	// left  not pressed
					status |=  JOY_RIGHT;	// right not pressed
				}
				break;
			case 1: // Vertical
				if (event.jaxis.value < -THRESHOLD) {
					status |=  JOY_DOWN;	// down not pressed
					status &= ~JOY_UP;	// up       pressed
				} else if (event.jaxis.value > THRESHOLD) {
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
	return true;
}
