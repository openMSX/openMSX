// $Id$

#include <cassert>
#include "openmsx.hh"
#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "CommandController.hh"
#include "InputEvents.hh"
#include "BooleanSetting.hh"

using std::string;
using std::vector;

namespace openmsx {

InputEventGenerator::InputEventGenerator()
	: grabInput(new BooleanSetting("grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false))
	, escapeGrabCmd(*this)
	, keyRepeat(false)
	, distributor(EventDistributor::instance())
{
	setGrabInput(grabInput->getValue());
	grabInput->addListener(this);
	distributor.registerEventListener(FOCUS_EVENT, *this,
	                                  EventDistributor::NATIVE);
	CommandController::instance().registerCommand(&escapeGrabCmd, "escape_grab");

	reinit();
}

InputEventGenerator::~InputEventGenerator()
{
	CommandController::instance().unregisterCommand(&escapeGrabCmd, "escape_grab");
	distributor.unregisterEventListener(FOCUS_EVENT, *this,
	                                  EventDistributor::NATIVE);
	grabInput->removeListener(this);
}

InputEventGenerator& InputEventGenerator::instance()
{
	static InputEventGenerator oneInstance;
	return oneInstance;
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

void InputEventGenerator::notify()
{
	static SDL_Event event;
	event.type = SDL_USEREVENT;
	SDL_PushEvent(&event);
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

static MouseButtonEvent::Button convertMouseButton(Uint8 sdlButton)
{
	switch (sdlButton) {
	case SDL_BUTTON_LEFT:
		return MouseButtonEvent::LEFT;
	case SDL_BUTTON_RIGHT:
		return MouseButtonEvent::RIGHT;
	case SDL_BUTTON_MIDDLE:
		return MouseButtonEvent::MIDDLE;
	default:
		return MouseButtonEvent::OTHER;
	}
}

static JoystickAxisMotionEvent::Axis convertJoyAxis(Uint8 sdlAxis)
{
	switch (sdlAxis) {
	case 0:
		return JoystickAxisMotionEvent::X_AXIS;
	case 1:
		return JoystickAxisMotionEvent::Y_AXIS;
	default:
		return JoystickAxisMotionEvent::OTHER;
	}
}

void InputEventGenerator::handle(const SDL_Event& evt)
{
	switch (evt.type) {
	case SDL_KEYUP: {
		Event* event = new KeyUpEvent(Keys::getCode(evt.key.keysym.sym,
		                                            evt.key.keysym.mod,
		                                            true),
		                              evt.key.keysym.unicode);
		distributor.distributeEvent(event);
		break;
	}
	case SDL_KEYDOWN: {
		Event* event = new KeyDownEvent(Keys::getCode(evt.key.keysym.sym,
		                                              evt.key.keysym.mod,
		                                              false),
		                                evt.key.keysym.unicode);
		distributor.distributeEvent(event);
		break;
	}

	case SDL_MOUSEBUTTONUP: {
		Event* event = new MouseButtonUpEvent(
		                        convertMouseButton(evt.button.button));
		distributor.distributeEvent(event);
		break;
	}
	case SDL_MOUSEBUTTONDOWN: {
		Event* event = new MouseButtonDownEvent(
		                        convertMouseButton(evt.button.button));
		distributor.distributeEvent(event);
		break;
	}
	case SDL_MOUSEMOTION: {
		Event* event = new MouseMotionEvent(evt.motion.xrel,
		                                    evt.motion.yrel);
		distributor.distributeEvent(event);
		break;
	}

	case SDL_JOYBUTTONUP: {
		Event* event = new JoystickButtonUpEvent(evt.jbutton.which,
		                                         evt.jbutton.button);
		distributor.distributeEvent(event);
		break;
	}
	case SDL_JOYBUTTONDOWN: {
		Event* event = new JoystickButtonDownEvent(evt.jbutton.which,
		                                           evt.jbutton.button);
		distributor.distributeEvent(event);
		break;
	}
	case SDL_JOYAXISMOTION: {
		Event* event = new JoystickAxisMotionEvent(evt.jaxis.which,
		                              convertJoyAxis(evt.jaxis.axis),
		                              evt.jaxis.value);
		distributor.distributeEvent(event);
		break;
	}
	      
	case SDL_ACTIVEEVENT: {
		Event* event = new FocusEvent(evt.active.gain);
		distributor.distributeEvent(event);
		break;
	}
	
	case SDL_QUIT:
		distributor.distributeEvent(new QuitEvent());
		break;

	default:
		break;
	}
}


void InputEventGenerator::update(const Setting* setting)
{
	assert(setting == grabInput.get());
	escapeGrabState = ESCAPE_GRAB_WAIT_CMD;
	setGrabInput(grabInput->getValue());
}

bool InputEventGenerator::signalEvent(const Event& event)
{
	assert(event.getType() == FOCUS_EVENT);

	const FocusEvent& focusEvent = static_cast<const FocusEvent&>(event);
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
	return true;
}

void InputEventGenerator::setGrabInput(bool grab)
{
	SDL_WM_GrabInput(grab ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

// class EscapeGrabCmd
InputEventGenerator::EscapeGrabCmd::EscapeGrabCmd(InputEventGenerator& parent_)
	: parent(parent_)
{
}

string InputEventGenerator::EscapeGrabCmd::execute(const vector<string>& tokens)
{
	if (parent.grabInput->getValue()) {
		parent.escapeGrabState = ESCAPE_GRAB_WAIT_LOST;
		parent.setGrabInput(false);
	}
	return "";
}

string InputEventGenerator::EscapeGrabCmd::help(const vector<string>& tokens) const
{
	return "Temporarily release input grab.";
}

} // namespace openmsx
