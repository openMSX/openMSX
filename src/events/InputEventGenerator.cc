// $Id$

#include "Command.hh"
#include "InputEventGenerator.hh"
#include "EventDistributor.hh"
#include "InputEvents.hh"
#include "BooleanSetting.hh"
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
		"This setting controls if openmsx takes over mouse and keyboard input",
		false, Setting::DONT_SAVE))
	, escapeGrabCmd(new EscapeGrabCmd(commandController, *this))
	, escapeGrabState(ESCAPE_GRAB_WAIT_CMD)
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

void InputEventGenerator::handle(const SDL_Event& evt)
{
	Event* event;
	switch (evt.type) {
	case SDL_KEYUP:
		event = new KeyUpEvent(
		        Keys::getCode(evt.key.keysym.sym,
		                      evt.key.keysym.mod,
		                      true),
		        evt.key.keysym.unicode);
		break;
	case SDL_KEYDOWN:
		event = new KeyDownEvent(
		        Keys::getCode(evt.key.keysym.sym,
		                      evt.key.keysym.mod,
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

	case SDL_JOYBUTTONUP:
		event = new JoystickButtonUpEvent(evt.jbutton.which,
		                                  evt.jbutton.button);
		break;
	case SDL_JOYBUTTONDOWN:
		event = new JoystickButtonDownEvent(evt.jbutton.which,
		                                    evt.jbutton.button);
		break;
	case SDL_JOYAXISMOTION:
		event = new JoystickAxisMotionEvent(evt.jaxis.which,
		                                    evt.jaxis.axis,
		                                    evt.jaxis.value);
		break;

	case SDL_ACTIVEEVENT:
		event = new FocusEvent(evt.active.gain);
		break;

	case SDL_VIDEORESIZE:
		event = new ResizeEvent(evt.resize.w, evt.resize.h);
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
