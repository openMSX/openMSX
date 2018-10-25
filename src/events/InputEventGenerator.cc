#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"
#include "IntegerSetting.hh"
#include "GlobalSettings.hh"
#include "Keys.hh"
#include "checked_cast.hh"
#include "outer.hh"
#include "unreachable.hh"
#include "build-info.hh"
#include <cassert>
#include <iostream>
#include <memory>

using std::string;
using std::vector;
using std::make_shared;

namespace openmsx {

bool InputEventGenerator::androidButtonA = false;
bool InputEventGenerator::androidButtonB = false;

InputEventGenerator::InputEventGenerator(CommandController& commandController,
                                         EventDistributor& eventDistributor_,
                                         GlobalSettings& globalSettings_)
	: eventDistributor(eventDistributor_)
	, globalSettings(globalSettings_)
	, grabInput(
		commandController, "grabinput",
		"This setting controls if openMSX takes over mouse and keyboard input",
		false, Setting::DONT_SAVE)
	, escapeGrabCmd(commandController)
	, escapeGrabState(ESCAPE_GRAB_WAIT_CMD)
	, keyRepeat(false)
{
	setGrabInput(grabInput.getBoolean());
	grabInput.attach(*this);
	eventDistributor.registerEventListener(OPENMSX_FOCUS_EVENT, *this);

	reinit();

	osdControlButtonsState = unsigned(~0); // 0 is pressed, 1 is released

#ifndef SDL_JOYSTICK_DISABLED
	SDL_JoystickEventState(SDL_ENABLE); // joysticks generate events
#endif
}

InputEventGenerator::~InputEventGenerator()
{
	eventDistributor.unregisterEventListener(OPENMSX_FOCUS_EVENT, *this);
	grabInput.detach(*this);
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
#if 0
		string t;
		switch (event.type) {
			case SDL_ACTIVEEVENT:     t = "SDL_ACTIVEEVENT";     break;
			case SDL_KEYDOWN:         t = "SDL_KEYDOWN";         break;
			case SDL_KEYUP:           t = "SDL_KEYUP";           break;
			case SDL_MOUSEMOTION:     t = "SDL_MOUSEMOTION";     break;
			case SDL_MOUSEBUTTONDOWN: t = "SDL_MOUSEBUTTONDOWN"; break;
			case SDL_MOUSEBUTTONUP:   t = "SDL_MOUSEBUTTONUP";   break;
			case SDL_JOYAXISMOTION:   t = "SDL_JOYAXISMOTION";   break;
			case SDL_JOYBALLMOTION:   t = "SDL_JOYBALLMOTION";   break;
			case SDL_JOYHATMOTION:    t = "SDL_JOYHATMOTION";    break;
			case SDL_JOYBUTTONDOWN:   t = "SDL_JOYBUTTONDOWN";   break;
			case SDL_JOYBUTTONUP:     t = "SDL_JOYBUTTONUP";     break;
			case SDL_QUIT:            t = "SDL_QUIT";            break;
			case SDL_SYSWMEVENT:      t = "SDL_SYSWMEVENT";      break;
			case SDL_VIDEORESIZE:     t = "SDL_VIDEORESIZE";     break;
			case SDL_VIDEOEXPOSE:     t = "SDL_VIDEOEXPOSE";     break;
			case SDL_USEREVENT:       t = "SDL_USEREVENT";       break;
			default:                  t = "UNKNOWN";             break;
		}
		std::cerr << "SDL event received, type: " << t << std::endl;
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


void InputEventGenerator::setNewOsdControlButtonState(
		unsigned newState, const EventPtr& origEvent)
{
	unsigned deltaState = osdControlButtonsState ^ newState;
	for (unsigned i = OsdControlEvent::LEFT_BUTTON;
			i <= OsdControlEvent::B_BUTTON; ++i) {
		if (deltaState & (1 << i)) {
			if (newState & (1 << i)) {
				eventDistributor.distributeEvent(
					make_shared<OsdControlReleaseEvent>(
						i, origEvent));
			} else {
				eventDistributor.distributeEvent(
					make_shared<OsdControlPressEvent>(
						i, origEvent));
			}
		}
	}
	osdControlButtonsState = newState;
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickAxisMotion(
	unsigned axis, int value, const EventPtr& origEvent)
{
	unsigned neg_button, pos_button;
	switch (axis) {
	case 0:
		neg_button = 1 << OsdControlEvent::LEFT_BUTTON;
		pos_button = 1 << OsdControlEvent::RIGHT_BUTTON;
		break; // axis 0
	case 1:
		neg_button = 1 << OsdControlEvent::UP_BUTTON;
		pos_button = 1 << OsdControlEvent::DOWN_BUTTON;
		break;
	default:
		// Ignore all other axis (3D joysticks and flight joysticks may
		// have more than 2 axis)
		return;
	}

	if (value > 0) {
		// release negative button, press positive button
		setNewOsdControlButtonState(
			(osdControlButtonsState | neg_button) & ~pos_button,
			origEvent);
	} else if (value < 0) {
		// press negative button, release positive button
		setNewOsdControlButtonState(
			(osdControlButtonsState | pos_button) & ~neg_button,
			origEvent);
	} else {
		// release both buttons
		setNewOsdControlButtonState(
			osdControlButtonsState | neg_button | pos_button,
			origEvent);
	}
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickHat(
	int value, const EventPtr& origEvent)
{
	unsigned dir = 0;
	if (!(value & SDL_HAT_UP   )) dir |= 1 << OsdControlEvent::UP_BUTTON;
	if (!(value & SDL_HAT_DOWN )) dir |= 1 << OsdControlEvent::DOWN_BUTTON;
	if (!(value & SDL_HAT_LEFT )) dir |= 1 << OsdControlEvent::LEFT_BUTTON;
	if (!(value & SDL_HAT_RIGHT)) dir |= 1 << OsdControlEvent::RIGHT_BUTTON;
	unsigned ab = osdControlButtonsState & ((1 << OsdControlEvent::A_BUTTON) |
	                                        (1 << OsdControlEvent::B_BUTTON));
	setNewOsdControlButtonState(ab | dir, origEvent);
}

void InputEventGenerator::osdControlChangeButton(
	bool up, unsigned changedButtonMask, const EventPtr& origEvent)
{
	auto newButtonState = up
		? osdControlButtonsState | changedButtonMask
		: osdControlButtonsState & ~changedButtonMask;
	setNewOsdControlButtonState(newButtonState, origEvent);
}

void InputEventGenerator::triggerOsdControlEventsFromJoystickButtonEvent(
	unsigned button, bool up, const EventPtr& origEvent)
{
	osdControlChangeButton(
		up,
		((button & 1) ? (1 << OsdControlEvent::B_BUTTON)
		              : (1 << OsdControlEvent::A_BUTTON)),
		origEvent);
}

void InputEventGenerator::triggerOsdControlEventsFromKeyEvent(
	Keys::KeyCode keyCode, bool up, const EventPtr& origEvent)
{
	keyCode = static_cast<Keys::KeyCode>(keyCode & Keys::K_MASK);
	if (keyCode == Keys::K_LEFT) {
		osdControlChangeButton(up, 1 << OsdControlEvent::LEFT_BUTTON,
		                       origEvent);
	} else if (keyCode == Keys::K_RIGHT) {
		osdControlChangeButton(up, 1 << OsdControlEvent::RIGHT_BUTTON,
		                       origEvent);
	} else if (keyCode == Keys::K_UP) {
		osdControlChangeButton(up, 1 << OsdControlEvent::UP_BUTTON,
		                       origEvent);
	} else if (keyCode == Keys::K_DOWN) {
		osdControlChangeButton(up, 1 << OsdControlEvent::DOWN_BUTTON,
		                       origEvent);
	} else if (keyCode == Keys::K_SPACE || keyCode == Keys::K_RETURN) {
		osdControlChangeButton(up, 1 << OsdControlEvent::A_BUTTON,
		                       origEvent);
	} else if (keyCode == Keys::K_ESCAPE) {
		osdControlChangeButton(up, 1 << OsdControlEvent::B_BUTTON,
		                       origEvent);
	}
}

static Uint16 maskPUA(Uint16 unicode)
{
	// Apparently on macOS keyboard events for keys like F1 have a non-zero
	// unicode field. This confuses the console code (it thinks it's a
	// printable character) and it prevents those keys from triggering
	// hotkey bindings. See this bug report:
	//   https://github.com/openMSX/openMSX/issues/1095
	// As a workaround we mask unicode chars in the 'Private Use Area' (PUA).
	//   https://en.wikipedia.org/wiki/Private_Use_Areas
	//
	// Note: because the unicode field in SDL1.2 is only 16 bits, we only need
	//       to look at the first area: U+E000..U+F8FF
	return ((0xE000 <= unicode) && (unicode <= 0xF8FF)) ? 0 : unicode;
}

void InputEventGenerator::handle(const SDL_Event& evt)
{
	EventPtr event;
	switch (evt.type) {
	case SDL_KEYUP:
		// Virtual joystick of SDL Android port does not have joystick
		// buttons. It has however up to 6 virtual buttons that can be
		// mapped to SDL keyboard events. Two of these virtual buttons
		// will be mapped to keys SDLK_WORLD_93 and 94 and are
		// interpeted here as joystick buttons (respectively button 0
		// and 1).
		if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_93) {
			event = make_shared<JoystickButtonUpEvent>(0, 0);
			triggerOsdControlEventsFromJoystickButtonEvent(
				0, true, event);
			androidButtonA = false;
		} else if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_94) {
			event = make_shared<JoystickButtonUpEvent>(0, 1);
			triggerOsdControlEventsFromJoystickButtonEvent(
				1, true, event);
			androidButtonB = false;
		} else {
			auto keyCode = Keys::getCode(
				evt.key.keysym.sym, evt.key.keysym.mod,
				evt.key.keysym.scancode, true);
			event = make_shared<KeyUpEvent>(
				keyCode, maskPUA(evt.key.keysym.unicode));
			triggerOsdControlEventsFromKeyEvent(keyCode, true, event);
		}
		break;
	case SDL_KEYDOWN:
		if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_93) {
			event = make_shared<JoystickButtonDownEvent>(0, 0);
			triggerOsdControlEventsFromJoystickButtonEvent(
				0, false, event);
			androidButtonA = true;
		} else if (PLATFORM_ANDROID && evt.key.keysym.sym == SDLK_WORLD_94) {
			event = make_shared<JoystickButtonDownEvent>(0, 1);
			triggerOsdControlEventsFromJoystickButtonEvent(
				1, false, event);
			androidButtonB = true;
		} else {
			auto keyCode = Keys::getCode(
				evt.key.keysym.sym, evt.key.keysym.mod,
				evt.key.keysym.scancode, false);
			event = make_shared<KeyDownEvent>(
				keyCode, maskPUA(evt.key.keysym.unicode));
			triggerOsdControlEventsFromKeyEvent(keyCode, false, event);
		}
		break;

	case SDL_MOUSEBUTTONUP:
		event = make_shared<MouseButtonUpEvent>(evt.button.button);
		break;
	case SDL_MOUSEBUTTONDOWN:
		event = make_shared<MouseButtonDownEvent>(evt.button.button);
		break;
	case SDL_MOUSEMOTION:
		event = make_shared<MouseMotionEvent>(
			evt.motion.xrel, evt.motion.yrel,
			evt.motion.x,    evt.motion.y);
		break;

	case SDL_JOYBUTTONUP:
		event = make_shared<JoystickButtonUpEvent>(
			evt.jbutton.which, evt.jbutton.button);
		triggerOsdControlEventsFromJoystickButtonEvent(
			evt.jbutton.button, true, event);
		break;
	case SDL_JOYBUTTONDOWN:
		event = make_shared<JoystickButtonDownEvent>(
			evt.jbutton.which, evt.jbutton.button);
		triggerOsdControlEventsFromJoystickButtonEvent(
			evt.jbutton.button, false, event);
		break;
	case SDL_JOYAXISMOTION: {
		auto& setting = globalSettings.getJoyDeadzoneSetting(evt.jaxis.which);
		int threshold = (setting.getInt() * 32768) / 100;
		auto value = (evt.jaxis.value < -threshold) ? evt.jaxis.value
		           : (evt.jaxis.value >  threshold) ? evt.jaxis.value
		                                            : 0;
		event = make_shared<JoystickAxisMotionEvent>(
			evt.jaxis.which, evt.jaxis.axis, value);
		triggerOsdControlEventsFromJoystickAxisMotion(
			evt.jaxis.axis, value, event);
		break;
	}
	case SDL_JOYHATMOTION:
		event = make_shared<JoystickHatEvent>(
			evt.jhat.which, evt.jhat.hat, evt.jhat.value);
		triggerOsdControlEventsFromJoystickHat(evt.jhat.value, event);
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

#if 0
	if (event) {
		std::cerr << "SDL event converted to: " + event->toString() << std::endl;
	} else {
		std::cerr << "SDL event was of unknown type, not converted to an openMSX event" << std::endl;
	}
#endif

	if (event) eventDistributor.distributeEvent(event);
}


void InputEventGenerator::update(const Setting& setting)
{
	assert(&setting == &grabInput); (void)setting;
	escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
	setGrabInput(grabInput.getBoolean());
}

int InputEventGenerator::signalEvent(const std::shared_ptr<const Event>& event)
{
	auto& focusEvent = checked_cast<const FocusEvent&>(*event);
	switch (escapeGrabState) {
		case ESCAPE_GRAB_WAIT_CMD:
			// nothing
			break;
		case ESCAPE_GRAB_WAIT_LOST:
			if (!focusEvent.getGain()) {
				escapeGrabState = ESCAPE_GRAB_WAIT_GAIN;
			}
			break;
		case ESCAPE_GRAB_WAIT_GAIN:
			if (focusEvent.getGain()) {
				escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
			}
			setGrabInput(true);
			break;
		default:
			UNREACHABLE;
	}
	return 0;
}

void InputEventGenerator::setGrabInput(bool grab)
{
	// Note that this setting is also changed in VisibleSurface constructor
	// because for Mac we want to enable it in fullscreen.
	// It's not worth it to get that exactly right here, because here
	// we don't have easy access to renderer settings and it may only
	// go wrong if you explicitly change grab input at full screen (on Mac)
	SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
}


// Wrap SDL joystick button functions to handle the 'fake' android joystick
// buttons. The method InputEventGenerator::handle() already takes care of fake
// events for the andoid joystick buttons, these two wrappers handle the direct
// joystick button state queries.
int InputEventGenerator::joystickNumButtons(SDL_Joystick* joystick)
{
	if (PLATFORM_ANDROID) {
		return 2;
	} else {
		return SDL_JoystickNumButtons(joystick);
	}
}
bool InputEventGenerator::joystickGetButton(SDL_Joystick* joystick, int button)
{
	if (PLATFORM_ANDROID) {
		switch (button) {
		case 0: return androidButtonA;
		case 1: return androidButtonB;
		default: UNREACHABLE; return false;
		}
	} else {
		return SDL_JoystickGetButton(joystick, button) != 0;
	}
}


// class EscapeGrabCmd

InputEventGenerator::EscapeGrabCmd::EscapeGrabCmd(
		CommandController& commandController_)
	: Command(commandController_, "escape_grab")
{
}

void InputEventGenerator::EscapeGrabCmd::execute(
	array_ref<TclObject> /*tokens*/, TclObject& /*result*/)
{
	auto& inputEventGenerator = OUTER(InputEventGenerator, escapeGrabCmd);
	if (inputEventGenerator.grabInput.getBoolean()) {
		inputEventGenerator.escapeGrabState =
			InputEventGenerator::ESCAPE_GRAB_WAIT_LOST;
		inputEventGenerator.setGrabInput(false);
	}
}

string InputEventGenerator::EscapeGrabCmd::help(
	const vector<string>& /*tokens*/) const
{
	return "Temporarily release input grab.";
}

} // namespace openmsx
