// $Id$

#include <cassert>
#include "openmsx.hh"
#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"
#include "BooleanSetting.hh"

namespace openmsx {

InputEventGenerator::InputEventGenerator()
	: grabInput(new BooleanSetting("grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false))
	, keyRepeat(false)
	, distributor(EventDistributor::instance())
{
	grabInput->addListener(this);
	reinit();
}

InputEventGenerator::~InputEventGenerator()
{
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
	      
	case SDL_QUIT: {
		Event* event = new QuitEvent();
		distributor.distributeEvent(event);
	}

	default:
		break;
	}
}


void InputEventGenerator::update(const Setting* setting)
{
	assert(setting == grabInput.get());
	SDL_WM_GrabInput(grabInput->getValue() ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

} // namespace openmsx
