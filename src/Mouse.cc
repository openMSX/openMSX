// $Id$

#include "Mouse.hh"
#include "PluggingController.hh"
#include "EventDistributor.hh"


//TODO implement timeout



Mouse::Mouse()
{
	status = JOY_BUTTONA | JOY_BUTTONB;
	faze = FAZE_YLOW;
	xrel = yrel = curxrel = curyrel = 0;
	
	EventDistributor::instance()->registerEventListener(SDL_MOUSEMOTION,     this);
	EventDistributor::instance()->registerEventListener(SDL_MOUSEBUTTONDOWN, this);
	EventDistributor::instance()->registerEventListener(SDL_MOUSEBUTTONUP,   this);

	PluggingController::instance()->registerPluggable(this);
}

Mouse::~Mouse()
{
	PluggingController::instance()->unregisterPluggable(this);
	
	EventDistributor::instance()->unregisterEventListener(SDL_MOUSEMOTION,     this);
	EventDistributor::instance()->unregisterEventListener(SDL_MOUSEBUTTONDOWN, this);
	EventDistributor::instance()->unregisterEventListener(SDL_MOUSEBUTTONUP,   this);
}


//Pluggable
const std::string &Mouse::getName()
{
	static const std::string name("mouse");
	return name;
}


//JoystickDevice
byte Mouse::read(const EmuTime &time)
{
	switch (faze) {
	case FAZE_XHIGH:
		return (((xrel/SCALE)>>4)&0x0f)|status;
	case FAZE_XLOW:
		return  ((xrel/SCALE)    &0x0f)|status;
	case FAZE_YHIGH:
		return (((yrel/SCALE)>>4)&0x0f)|status;
	case FAZE_YLOW:
		return  ((yrel/SCALE)    &0x0f)|status;
	default:
		assert(false);
		return status;	// avoid warning
	}
}

void Mouse::write(byte value, const EmuTime &time)
{
	switch (faze) {
	case FAZE_XHIGH:
		if ((value&STROBE)==0) faze=FAZE_XLOW;
		break;
	case FAZE_XLOW:
		if ((value&STROBE)!=0) faze=FAZE_YHIGH;
		break;
	case FAZE_YHIGH:
		if ((value&STROBE)==0) faze=FAZE_YLOW;
		break;
	case FAZE_YLOW:
		if ((value&STROBE)!=0) {
			faze=FAZE_XHIGH;
			xrel = curxrel; yrel = curyrel;
			curxrel = 0; curyrel = 0;
		}
		break;
	}
}


//EventListener
bool Mouse::signalEvent(SDL_Event &event)
{
	switch (event.type) {
	case SDL_MOUSEMOTION:
		curxrel -= event.motion.xrel;
		curyrel -= event.motion.yrel;
		if (curxrel> 127*SCALE) curxrel= 127*SCALE;
		if (curxrel<-128*SCALE) curxrel=-128*SCALE;
		if (curyrel> 127*SCALE) curyrel= 127*SCALE;
		if (curyrel<-128*SCALE) curyrel=-128*SCALE;
		break;
	case SDL_MOUSEBUTTONDOWN:
		switch (event.button.button) {
		case SDL_BUTTON_LEFT:
			status &= ~JOY_BUTTONA;
			break;
		case SDL_BUTTON_RIGHT:
			status &= ~JOY_BUTTONB;
			break;
		default:
			// ignore other buttons
			break;
		}
		break;
	case SDL_MOUSEBUTTONUP:
		switch (event.button.button) {
		case SDL_BUTTON_LEFT:
			status |= JOY_BUTTONA;
			break;
		case SDL_BUTTON_RIGHT:
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
	return true;
}
