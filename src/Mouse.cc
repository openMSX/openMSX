
#include "Mouse.hh"


Mouse::Mouse()
{
	status = JOY_BUTTONA | JOY_BUTTONB;
	faze = FAZE_YLOW;
	xrel = yrel = curxrel = curyrel = 0;
	mut = SDL_CreateMutex();
	EventDistributor::instance()->registerListener(SDL_MOUSEMOTION,     this);
	EventDistributor::instance()->registerListener(SDL_MOUSEBUTTONDOWN, this);
	EventDistributor::instance()->registerListener(SDL_MOUSEBUTTONUP,   this);
}

Mouse::~Mouse()
{
	SDL_DestroyMutex(mut);
}

//JoystickDevice
byte Mouse::read()
{
	switch (faze) {
	case FAZE_XHIGH:
		return ((xrel>>4)&0x0f)|status;
	case FAZE_XLOW:
		return  (xrel    &0x0f)|status;
	case FAZE_YHIGH:
		return ((yrel>>4)&0x0f)|status;
	case FAZE_YLOW:
		return  (yrel    &0x0f)|status;
	default:
		assert(false);
	}
}

void Mouse::write(byte value)
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
			SDL_mutexP(mut);
			xrel = curxrel; yrel = curyrel;
			curxrel = 0; curyrel = 0;
			SDL_mutexV(mut);
		}
		break;
	}
}

//EventListener
// note: this method runs in a different thread!!
//  variable curxrel and curyrel should be locked
void Mouse::signalEvent(SDL_Event &event)
{
	switch (event.type) {
	case SDL_MOUSEMOTION:
		SDL_mutexP(mut);
		curxrel += event.motion.xrel;
		curyrel += event.motion.yrel;
		if (curxrel> 127) curxrel= 127;
		if (curxrel<-128) curxrel=-128;
		if (curyrel> 127) curyrel= 127;
		if (curyrel<-128) curyrel=-128;
		SDL_mutexV(mut);
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
}
