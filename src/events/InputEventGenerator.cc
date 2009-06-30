// $Id$

#include "Command.hh"
#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"
#include "BooleanSetting.hh"
#include "Keys.hh" // GP2X
#include "openmsx.hh"
#include "checked_cast.hh"
#include <cassert>

using std::string;
using std::vector;

namespace openmsx {

class EscapeGrabCmd : public SimpleCommand
{
public:
	EscapeGrabCmd(CommandController& commandController,
		      InputEventGenerator& inputEventGenerator);
	virtual string execute(const vector<string>& tokens);
	virtual string help(const vector<string>& tokens) const;
private:
	InputEventGenerator& inputEventGenerator;
};


InputEventGenerator::InputEventGenerator(CommandController& commandController,
                                         EventDistributor& eventDistributor_)
	: eventDistributor(eventDistributor_)
	, grabInput(new BooleanSetting(commandController, "grabinput",
		"This setting controls if openMSX takes over mouse and keyboard input",
		false, Setting::DONT_SAVE))
	, escapeGrabCmd(new EscapeGrabCmd(commandController, *this))
	, escapeGrabState(ESCAPE_GRAB_WAIT_CMD)
#if PLATFORM_GP2X
	, stat8(0)
#endif
	, keyRepeat(false)
{
	setGrabInput(grabInput->getValue());
	grabInput->attach(*this);
	eventDistributor.registerEventListener(OPENMSX_FOCUS_EVENT, *this);
	eventDistributor.registerEventListener(OPENMSX_POLL_EVENT,  *this);

	reinit();
}

InputEventGenerator::~InputEventGenerator()
{
	eventDistributor.unregisterEventListener(OPENMSX_POLL_EVENT,  *this);
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	grabInput->detach(*this);
}

void InputEventGenerator::reinit()
{
	SDL_EnableUNICODE(1);
	setKeyRepeat(keyRepeat);
}

void InputEventGenerator::wait()
{
	// SDL bug workaround
	if (!SDL_WasInit(SDL_INIT_VIDEO)) {
		SDL_Delay(100);
	}

	if (SDL_WaitEvent(NULL)) {
		poll();
	}
}

void InputEventGenerator::poll()
{
	SDL_Event event;
	while (SDL_PollEvent(&event) == 1) {
		PRT_DEBUG("SDL event received");
		handle(event);
	}
}

void InputEventGenerator::setKeyRepeat(bool enable)
{
	keyRepeat = enable;
	if (keyRepeat) {
		SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
		                    SDL_DEFAULT_REPEAT_INTERVAL);
	} else {
		SDL_EnableKeyRepeat(0, 0);
	}
}

#if PLATFORM_GP2X
static const int GP2X_UP        = 0;
static const int GP2X_UPLEFT    = 1;
static const int GP2X_LEFT      = 2;
static const int GP2X_DOWNLEFT  = 3;
static const int GP2X_DOWN      = 4;
static const int GP2X_DOWNRIGHT = 5;
static const int GP2X_RIGHT     = 6;
static const int GP2X_UPRIGHT   = 7;
static const int GP2X_DIRECTION = 8; // directions are smaller than this

static const int GP2X_START     = 8;
static const int GP2X_SELECT    = 9;
static const int GP2X_L         = 10;
static const int GP2X_R         = 11;
static const int GP2X_A         = 12;
static const int GP2X_B         = 13;
static const int GP2X_X         = 14;
static const int GP2X_Y         = 15;
static const int GP2X_VOLUP     = 16;
static const int GP2X_VOLDOWN   = 17;
static const int GP2X_FIRE      = 18;

static Event* createKeyEvent(Keys::KeyCode key, bool up)
{
	if (up) {
		return new KeyUpEvent  (Keys::combine(key, Keys::KD_RELEASE), 0);
	} else {
		return new KeyDownEvent(Keys::combine(key, Keys::KD_PRESS),   0);
	}
}

static Event* translateGP2Xbutton(int button, bool up)
{
	// TODO mapping is hardcoded ATM, should be configurable at run-time
	Keys::KeyCode key;
	switch (button) {
		case GP2X_A:         key = Keys::K_LSHIFT; break;
		case GP2X_B:         key = Keys::K_SPACE;  break;
		case GP2X_X:         key = Keys::K_M;      break;
		case GP2X_Y:         key = Keys::K_ESCAPE; break;
		case GP2X_L:         key = Keys::K_L;      break;
		case GP2X_R:         key = Keys::K_R;      break;
		case GP2X_FIRE:      key = Keys::K_F;      break;
		case GP2X_START:     key = Keys::K_S;      break;
		case GP2X_SELECT:    key = Keys::K_E;      break;
		case GP2X_VOLUP:     key = Keys::K_U;      break;
		case GP2X_VOLDOWN:   key = Keys::K_D;      break;
		default:             key = Keys::K_NONE;   break;
	}
	return createKeyEvent(key, up);
}

static int calcStat4(int stat8)
{
	// This function converts the 8-input GP2X joystick to 4-input MSX
	// joystick. See the following page for an explanation on the
	// different configurations.
	//   http://wiki.gp2x.org/wiki/Suggested_Joystick_Configurations
	static const int UP4    = 1 << 0;
	static const int LEFT4  = 1 << 1;
	static const int DOWN4  = 1 << 2;
	static const int RIGHT4 = 1 << 3;
	static const int UP8        = 1 << GP2X_UP;
	static const int UPLEFT8    = 1 << GP2X_UPLEFT;
	static const int LEFT8      = 1 << GP2X_LEFT;
	static const int DOWNLEFT8  = 1 << GP2X_DOWNLEFT;
	static const int DOWN8      = 1 << GP2X_DOWN;
	static const int DOWNRIGHT8 = 1 << GP2X_DOWNRIGHT;
	static const int RIGHT8     = 1 << GP2X_RIGHT;
	static const int UPRIGHT8   = 1 << GP2X_UPRIGHT;

	/*// Configuration '2' (hor/vert bias)
	if (stat8 & UP8       ) return UP4;
	if (stat8 & LEFT8     ) return LEFT4;
	if (stat8 & DOWN8     ) return DOWN4;
	if (stat8 & RIGHT8    ) return RIGHT4;
	if (stat8 & UPLEFT8   ) return UP4   | LEFT4;
	if (stat8 & DOWNLEFT8 ) return DOWN4 | LEFT4;
	if (stat8 & DOWNRIGHT8) return DOWN4 | RIGHT4;
	if (stat8 & UPRIGHT8  ) return UP4   | RIGHT4;
	return 0;*/

	/*// Configuration '3' (diagonal bias)
	int result = 0;
	if (stat8 & (UPRIGHT8   | UP8    | UPLEFT8   )) result |= UP4;
	if (stat8 & (UPLEFT8    | LEFT8  | DOWNLEFT8 )) result |= LEFT4;
	if (stat8 & (DOWNLEFT8  | DOWN8  | DOWNRIGHT8)) result |= DOWN4;
	if (stat8 & (DOWNRIGHT8 | RIGHT8 | UPRIGHT8  )) result |= RIGHT4;
	return result;*/

	// Configuration 'not-listed' (no bias but slightly rotated clock-wise)
	if ((stat8 & (UP8    | UPLEFT8   )) == UP8       ) return UP4;
	if ((stat8 & (LEFT8  | UPLEFT8   )) == UPLEFT8   ) return UP4   | LEFT4;
	if ((stat8 & (LEFT8  | DOWNLEFT8 )) == LEFT8     ) return         LEFT4;
	if ((stat8 & (DOWN8  | DOWNLEFT8 )) == DOWNLEFT8 ) return DOWN4 | LEFT4;
	if ((stat8 & (DOWN8  | DOWNRIGHT8)) == DOWN8     ) return DOWN4;
	if ((stat8 & (RIGHT8 | DOWNRIGHT8)) == DOWNRIGHT8) return DOWN4 | RIGHT4;
	if ((stat8 & (RIGHT8 | UPRIGHT8  )) == RIGHT8    ) return         RIGHT4;
	if ((stat8 & (UP8    | UPRIGHT8  )) == UPRIGHT8  ) return UP4   | RIGHT4;
	return 0;
}
#endif

void InputEventGenerator::handle(const SDL_Event& evt)
{
	Event* event;
	switch (evt.type) {
	case SDL_KEYUP:
		event = new KeyUpEvent(
		        Keys::getCode(evt.key.keysym.sym,
		                      evt.key.keysym.mod,
				      evt.key.keysym.scancode,
		                      true),
		        evt.key.keysym.unicode);
		break;
	case SDL_KEYDOWN:
		event = new KeyDownEvent(
		        Keys::getCode(evt.key.keysym.sym,
		                      evt.key.keysym.mod,
				      evt.key.keysym.scancode,
		                      false),
		        evt.key.keysym.unicode);
		break;

	case SDL_MOUSEBUTTONUP:
		event = new MouseButtonUpEvent(evt.button.button);
		break;
	case SDL_MOUSEBUTTONDOWN:
		event = new MouseButtonDownEvent(evt.button.button);
		break;
	case SDL_MOUSEMOTION:
		event = new MouseMotionEvent(evt.motion.xrel, evt.motion.yrel);
		break;

#if PLATFORM_GP2X
	// SDL sees GP2X keys/joystick as joystick events, for openMSX it is
	// easier to handle these as keyboard events (regular keys + cursors).
	// Code below remaps the events. This will probably have to be rewritten
	// to allow more dynamic mappings.
	case SDL_JOYBUTTONUP:
	case SDL_JOYBUTTONDOWN: {
		int button = evt.jbutton.button;
		bool up = evt.type == SDL_JOYBUTTONUP;
		if (button < GP2X_DIRECTION) {
			static const Keys::KeyCode dirKeys[4] = {
				Keys::K_UP, Keys::K_LEFT, Keys::K_DOWN, Keys::K_RIGHT
			};
			int o4 = calcStat4(stat8);
			if (up) {
				stat8 &= ~(1 << button);
			} else {
				stat8 |=  (1 << button);
			}
			int n4 = calcStat4(stat8);
			for (int i = 0; i < 4; ++i) {
				if ((o4 ^ n4) & (1 << i)) {
					event = createKeyEvent(dirKeys[i], o4 & (1 << i));
					eventDistributor.distributeEvent(event);
				}
			}
			event = 0;
		} else {
			event = translateGP2Xbutton(button, up);
		}
		break;
	}
#else
	case SDL_JOYBUTTONUP:
		event = new JoystickButtonUpEvent(evt.jbutton.which,
		                                  evt.jbutton.button);
		break;
	case SDL_JOYBUTTONDOWN:
		event = new JoystickButtonDownEvent(evt.jbutton.which,
		                                    evt.jbutton.button);
		break;
#endif
	case SDL_JOYAXISMOTION:
		event = new JoystickAxisMotionEvent(evt.jaxis.which,
		                                    evt.jaxis.axis,
		                                    evt.jaxis.value);
		break;

	case SDL_ACTIVEEVENT:
		event = new FocusEvent(evt.active.gain != 0);
		break;

	case SDL_VIDEORESIZE:
		event = new ResizeEvent(evt.resize.w, evt.resize.h);
		break;

	case SDL_VIDEOEXPOSE:
		event = new SimpleEvent(OPENMSX_EXPOSE_EVENT);
		break;

	case SDL_QUIT:
		event = new QuitEvent();
		break;

	default:
		event = 0;
		break;
	}

	if (event) {
		eventDistributor.distributeEvent(event);
	}
}


void InputEventGenerator::update(const Setting& setting)
{
	(void)setting;
	assert(&setting == grabInput.get());
	escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
	setGrabInput(grabInput->getValue());
}

bool InputEventGenerator::signalEvent(shared_ptr<const Event> event)
{
	if (event->getType() == OPENMSX_FOCUS_EVENT) {
		const FocusEvent& focusEvent = checked_cast<const FocusEvent&>(*event);
		switch (escapeGrabState) {
			case ESCAPE_GRAB_WAIT_CMD:
				// nothing
				break;
			case ESCAPE_GRAB_WAIT_LOST:
				if (focusEvent.getGain() == false) {
					escapeGrabState = ESCAPE_GRAB_WAIT_GAIN;
				}
				break;
			case ESCAPE_GRAB_WAIT_GAIN:
				if (focusEvent.getGain() == true) {
					escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
				}
				setGrabInput(true);
				break;
			default:
				assert(false);
		}
	} else if (event->getType() == OPENMSX_POLL_EVENT) {
		poll();
	}
	return true;
}

void InputEventGenerator::setGrabInput(bool grab)
{
	SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
}


// class EscapeGrabCmd
EscapeGrabCmd::EscapeGrabCmd(CommandController& commandController,
                             InputEventGenerator& inputEventGenerator_)
	: SimpleCommand(commandController, "escape_grab")
	, inputEventGenerator(inputEventGenerator_)
{
}

string EscapeGrabCmd::execute(const vector<string>& /*tokens*/)
{
	if (inputEventGenerator.grabInput->getValue()) {
		inputEventGenerator.escapeGrabState =
			InputEventGenerator::ESCAPE_GRAB_WAIT_LOST;
		inputEventGenerator.setGrabInput(false);
	}
	return "";
}

string EscapeGrabCmd::help(const vector<string>& /*tokens*/) const
{
	return "Temporarily release input grab.";
}

} // namespace openmsx
