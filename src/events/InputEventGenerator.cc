// $Id$

#include <cassert>
#include "openmsx.hh"
#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"

namespace openmsx {

InputEventGenerator::InputEventGenerator()
	: grabInput("grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false),
	  distributor(EventDistributor::instance())
{
	grabInput.addListener(this);
}

InputEventGenerator::~InputEventGenerator()
{
	grabInput.removeListener(this);
}

InputEventGenerator& InputEventGenerator::instance()
{
	static InputEventGenerator oneInstance;
	return oneInstance;
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


static MouseButtonEvent::Button convertMouseButton(Uint8 sdlButton)
{
	switch (sdlButton) {
	case SDL_BUTTON_LEFT:
		return MouseButtonEvent::LEFT;
	case SDL_BUTTON_RIGHT:
		return MouseButtonEvent::MIDDLE;
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

void InputEventGenerator::handle(const SDL_Event& event)
{
	switch (event.type) {
	case SDL_KEYUP: {
		KeyUpEvent event(Keys::getCode(event.key.keysym.sym,
		                               event.key.keysym.mod,
		                               true),
		                 event.key.keysym.unicode);
		distributor.distributeEvent(event);
		break;
	}
	case SDL_KEYDOWN: {
		KeyDownEvent event(Keys::getCode(event.key.keysym.sym,
		                                 event.key.keysym.mod,
		                                 false),
		                   event.key.keysym.unicode);
		distributor.distributeEvent(event);
		break;
	}

	case SDL_MOUSEBUTTONUP: {
		MouseButtonUpEvent event(convertMouseButton(event.button.button));
		distributor.distributeEvent(event);
		break;
	}
	case SDL_MOUSEBUTTONDOWN: {
		MouseButtonDownEvent event(convertMouseButton(event.button.button));
		distributor.distributeEvent(event);
		break;
	}
	case SDL_MOUSEMOTION: {
		MouseMotionEvent event(event.motion.xrel, event.motion.yrel);
		distributor.distributeEvent(event);
		break;
	}

	case SDL_JOYBUTTONUP: {
		JoystickButtonUpEvent event(event.jbutton.which, event.jbutton.button);
		distributor.distributeEvent(event);
		break;
	}
	case SDL_JOYBUTTONDOWN: {
		JoystickButtonDownEvent event(event.jbutton.which, event.jbutton.button);
		distributor.distributeEvent(event);
		break;
	}
	case SDL_JOYAXISMOTION: {
		JoystickAxisMotionEvent event(event.jaxis.which,
		                              convertJoyAxis(event.jaxis.axis),
		                              event.jaxis.value);
		distributor.distributeEvent(event);
		break;
	}
	      
	case SDL_QUIT: {
		QuitEvent event;
		distributor.distributeEvent(event);
	}

	default:
		break;
	}
}


void InputEventGenerator::update(const SettingLeafNode *setting) throw()
{
	assert(setting == &grabInput);
	SDL_WM_GrabInput(grabInput.getValue() ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

} // namespace openmsx
