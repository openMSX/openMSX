// $Id$

#include "Command.hh"
#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"
#include "BooleanSetting.hh"
#include "Keys.hh" // GP2X and OSDcontrol
#include "openmsx.hh"
#include "checked_cast.hh"
#include "memory.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <cassert>

using std::string;
using std::vector;
using std::make_shared;

namespace openmsx {

class EscapeGrabCmd : public Command
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
	, grabInput(make_unique<BooleanSetting>(
		commandController, "grabinput",
		"This setting controls if openMSX takes over mouse and keyboard input",
		false, Setting::DONT_SAVE))
	, escapeGrabCmd(make_unique<EscapeGrabCmd>(commandController, *this))
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

	osdControlButtonsState = ~0; // 0 is pressed, 1 is released
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

	if (SDL_WaitEvent(nullptr)) {
		poll();
	}
}

void InputEventGenerator::poll()
{
	SDL_Event event;
	while (SDL_PollEvent(&event) == 1) {
#ifdef DEBUG
		string evtType;
		switch (event.type) {
			case SDL_ACTIVEEVENT: evtType = "SDL_ACTIVEEVENT"; break;
			case SDL_KEYDOWN: evtType = "SDL_KEYDOWN"; break;
			case SDL_KEYUP: evtType = "SDL_KEYUP"; break;
			case SDL_MOUSEMOTION: evtType = "SDL_MOUSEMOTION"; break;
			case SDL_MOUSEBUTTONDOWN: evtType = "SDL_MOUSEBUTTONDOWN"; break;
			case SDL_MOUSEBUTTONUP: evtType = "SDL_MOUSEBUTTONUP"; break;
			case SDL_JOYAXISMOTION: evtType = "SDL_JOYAXISMOTION"; break;
			case SDL_JOYBALLMOTION: evtType = "SDL_JOYBALLMOTION"; break;
			case SDL_JOYHATMOTION: evtType = "SDL_JOYHATMOTION"; break;
			case SDL_JOYBUTTONDOWN: evtType = "SDL_JOYBUTTONDOWN"; break;
			case SDL_JOYBUTTONUP: evtType = "SDL_JOYBUTTONUP"; break;
			case SDL_QUIT: evtType = "SDL_QUIT"; break;
			case SDL_SYSWMEVENT: evtType = "SDL_SYSWMEVENT"; break;
			case SDL_VIDEORESIZE: evtType = "SDL_VIDEORESIZE"; break;
			case SDL_VIDEOEXPOSE: evtType = "SDL_VIDEOEXPOSE"; break;
			case SDL_USEREVENT: evtType = "SDL_USEREVENT"; break;
			default: evtType = "UNKNOWN"; break;
		}
		PRT_DEBUG("SDL event received, type: " + evtType);
#endif
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

static EventDistributor::EventPtr createKeyEvent(Keys::KeyCode key, bool up)
{
	if (up) {
		return make_shared<KeyUpEvent  >(Keys::combine(key, Keys::KD_RELEASE), 0);
	} else {
		return make_shared<KeyDownEvent<(Keys::combine(key, Keys::KD_PRESS),   0);
	}
}

static EventDistributor::EventPtr translateGP2Xbutton(int button, bool up)
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


#if PLATFORM_ANDROID
//TODO: make JOYVALUE_THRESHOLD dynamic, depending on virtual key size
static const int JOYVALUE_THRESHOLD = 4 * 32768 / 10;
#else
static const int JOYVALUE_THRESHOLD = 32768 / 10;
#endif

void InputEventGenerator::setNewOsdControlButtonState(
		unsigned newState)
{
	EventDistributor::EventPtr event;
	unsigned deltaState = osdControlButtonsState ^ newState;
	for (unsigned i = OsdControlEvent::LEFT_BUTTON;
			i <= OsdControlEvent::B_BUTTON; ++i) {
		if (deltaState & (1 << i)) {
			if (newState & (1 << i)) {
				eventDistributor.distributeEvent(make_shared<OsdControlReleaseEvent>(i));
			} else {
				eventDistributor.distributeEvent(make_shared<OsdControlPressEvent>(i));
			}
		}
	}
	osdControlButtonsState = newState;
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickAxisMotion(
		unsigned axis,
		short value)
{
	short normalized_value = (value < -JOYVALUE_THRESHOLD ? -1 : (value > JOYVALUE_THRESHOLD ? 1 : 0));
	unsigned neg_button, pos_button;

	switch (axis) {
	case 0:
		neg_button = 1<<OsdControlEvent::LEFT_BUTTON;
		pos_button = 1<<OsdControlEvent::RIGHT_BUTTON;
		break; // axis 0
	case 1:
		neg_button = 1<<OsdControlEvent::UP_BUTTON;
		pos_button = 1<<OsdControlEvent::DOWN_BUTTON;
		break;
	default:
		// Ignore all other axis (3D joysticks and flight joysticks may have more then 2 axis)
		return;
	}
	switch(normalized_value) {
	case 0:
		// release both buttons
		setNewOsdControlButtonState(
				osdControlButtonsState | neg_button | pos_button);
		break;
	case 1:
		// release negative button, press positive button
		setNewOsdControlButtonState(
				(osdControlButtonsState | neg_button) & ~pos_button);
		break;
	case -1:
		// press negative button, release positive button
		setNewOsdControlButtonState(
				(osdControlButtonsState | pos_button) & ~neg_button);
		break;
	}
}	

void InputEventGenerator::osdControlChangeButton(
		bool up, unsigned changedButtonMask) 
{
	auto newButtonState = up
	   ? osdControlButtonsState | changedButtonMask
	   : osdControlButtonsState & ~changedButtonMask;
	setNewOsdControlButtonState(newButtonState); 
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickButtonEvent(
		unsigned button, bool up)
{
	osdControlChangeButton(up, 
		(button & 1) ? 1<<OsdControlEvent::B_BUTTON : 1<<OsdControlEvent::A_BUTTON);
}

void InputEventGenerator::triggerOsdControlEventsFromKeyEvent(
		Keys::KeyCode keyCode,
		bool up)
{
	keyCode = static_cast<Keys::KeyCode>(keyCode & Keys::K_MASK);
	if (keyCode == Keys::K_LEFT) {
		osdControlChangeButton(up, 1<<OsdControlEvent::LEFT_BUTTON);
	} else if (keyCode == Keys::K_RIGHT) {
		osdControlChangeButton(up, 1<<OsdControlEvent::RIGHT_BUTTON);
	} else if (keyCode == Keys::K_UP) {
		osdControlChangeButton(up, 1<<OsdControlEvent::UP_BUTTON);
	} else if (keyCode == Keys::K_DOWN) {
		osdControlChangeButton(up, 1<<OsdControlEvent::DOWN_BUTTON);
	} else if (keyCode == Keys::K_SPACE || keyCode == Keys::K_RETURN) {
		osdControlChangeButton(up, 1<<OsdControlEvent::A_BUTTON);
	} else if (keyCode == Keys::K_ESCAPE) {
		osdControlChangeButton(up, 1<<OsdControlEvent::B_BUTTON);
	}
}

void InputEventGenerator::handle(const SDL_Event& evt)
{
	EventDistributor::EventPtr event;
	Keys::KeyCode keyCode;
	switch (evt.type) {
	case SDL_KEYUP:
		// Virtual joystick of SDL Android port does not have joystick buttons.
		// It has however up to 6 virtual buttons that can be mapped to SDL keyboard
		// events. Two of these virtual buttons will be mapped to keys SDLK_WORLD_93 and 94
		// and are interpeted here as joystick buttons (respectively button 0 and 1).
		if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_93) {
			event = make_shared<JoystickButtonUpEvent>(0, 0);
			triggerOsdControlEventsFromJoystickButtonEvent(0, true);
		} else if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_94) {
			event = make_shared<JoystickButtonUpEvent>(0, 1);
			triggerOsdControlEventsFromJoystickButtonEvent(1, true);
		} else {
			keyCode = Keys::getCode(
					evt.key.keysym.sym,
					evt.key.keysym.mod,
					evt.key.keysym.scancode,
					true);
			event = make_shared<KeyUpEvent>(keyCode, evt.key.keysym.unicode);
			triggerOsdControlEventsFromKeyEvent(keyCode, true);
		}
		break;
	case SDL_KEYDOWN:
		if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_93) {
			event = make_shared<JoystickButtonDownEvent>(0, 0);
			triggerOsdControlEventsFromJoystickButtonEvent(0, false);
		} else if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_94) {
			event = make_shared<JoystickButtonDownEvent>(0, 1);
			triggerOsdControlEventsFromJoystickButtonEvent(1, false);
		} else {
			keyCode = Keys::getCode(
					evt.key.keysym.sym,
					evt.key.keysym.mod,
					evt.key.keysym.scancode,
					false);
			event = make_shared<KeyDownEvent>(keyCode, evt.key.keysym.unicode);
			triggerOsdControlEventsFromKeyEvent(keyCode, false);
		}
		break;

	case SDL_MOUSEBUTTONUP:
		event = make_shared<MouseButtonUpEvent>(evt.button.button);
		break;
	case SDL_MOUSEBUTTONDOWN:
		event = make_shared<MouseButtonDownEvent>(evt.button.button);
		break;
	case SDL_MOUSEMOTION:
		event = make_shared<MouseMotionEvent>(evt.motion.xrel, evt.motion.yrel);
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
			event = nullptr;
		} else {
			event = translateGP2Xbutton(button, up);
		}
		break;
	}
#else
	case SDL_JOYBUTTONUP:
		triggerOsdControlEventsFromJoystickButtonEvent(
				evt.jbutton.button, true);
		event = make_shared<JoystickButtonUpEvent>(
			evt.jbutton.which, evt.jbutton.button);
		break;
	case SDL_JOYBUTTONDOWN:
		triggerOsdControlEventsFromJoystickButtonEvent(
				evt.jbutton.button, false);
		event = make_shared<JoystickButtonDownEvent>(
			evt.jbutton.which, evt.jbutton.button);
		break;
#endif
	case SDL_JOYAXISMOTION:
		event = make_shared<JoystickAxisMotionEvent>(
			evt.jaxis.which, evt.jaxis.axis, evt.jaxis.value);
		triggerOsdControlEventsFromJoystickAxisMotion(
				evt.jaxis.axis, evt.jaxis.value);
		break;

	case SDL_ACTIVEEVENT:
		event = make_shared<FocusEvent>(evt.active.gain != 0);
		break;

	case SDL_VIDEORESIZE:
		event = make_shared<ResizeEvent>(evt.resize.w, evt.resize.h);
		break;

	case SDL_VIDEOEXPOSE:
		event = make_shared<SimpleEvent>(OPENMSX_EXPOSE_EVENT);
		break;

	case SDL_QUIT:
		event = make_shared<QuitEvent>();
		break;

	default:
		break;
	}

#ifdef DEBUG
	if (!event.get()) {
		PRT_DEBUG("SDL event was of unknown type, not converted to an openMSX event");
	} else {
		PRT_DEBUG("SDL event converted to: " + event.get()->toString());
	}

#endif
	if (event.get()) {
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

int InputEventGenerator::signalEvent(const std::shared_ptr<const Event>& event)
{
	if (event->getType() == OPENMSX_FOCUS_EVENT) {
		auto& focusEvent = checked_cast<const FocusEvent&>(*event);
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
				UNREACHABLE;
		}
	} else if (event->getType() == OPENMSX_POLL_EVENT) {
		poll();
	}
	return 0;
}

void InputEventGenerator::setGrabInput(bool grab)
{
	SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
}


// class EscapeGrabCmd
EscapeGrabCmd::EscapeGrabCmd(CommandController& commandController,
                             InputEventGenerator& inputEventGenerator_)
	: Command(commandController, "escape_grab")
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
