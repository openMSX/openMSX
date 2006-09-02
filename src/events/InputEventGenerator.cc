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
	virtual std::string execute(const std::vector<std::string>& tokens);
	virtual std::string help(const std::vector<std::string>& tokens) const;
private:
	InputEventGenerator& inputEventGenerator;
};


InputEventGenerator::InputEventGenerator(CommandController& commandController,
                                         EventDistributor& eventDistributor_)
	: grabInput(new BooleanSetting(commandController, "grabinput",
		"This setting controls if openmsx takes over mouse and keyboard input",
		false, Setting::DONT_SAVE))
	, escapeGrabState(ESCAPE_GRAB_WAIT_CMD)
	, escapeGrabCmd(new EscapeGrabCmd(commandController, *this))
	, keyRepeat(false)
	, eventDistributor(eventDistributor_)
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
	shared_ptr<const Event> event;
	switch (evt.type) {
	case SDL_KEYUP:
		event.reset(new KeyUpEvent(
		        Keys::getCode(evt.key.keysym.sym,
		                      evt.key.keysym.mod,
		                      true),
		        evt.key.keysym.unicode));
		break;
	case SDL_KEYDOWN:
		event.reset(new KeyDownEvent(
		        Keys::getCode(evt.key.keysym.sym,
		                      evt.key.keysym.mod,
		                      false),
		        evt.key.keysym.unicode));
		break;

	case SDL_MOUSEBUTTONUP:
		event.reset(new MouseButtonUpEvent(evt.button.button));
		break;
	case SDL_MOUSEBUTTONDOWN:
		event.reset(new MouseButtonDownEvent(evt.button.button));
		break;
	case SDL_MOUSEMOTION:
		event.reset(new MouseMotionEvent(evt.motion.xrel, evt.motion.yrel));
		break;

	case SDL_JOYBUTTONUP:
		event.reset(new JoystickButtonUpEvent(evt.jbutton.which,
		                                  evt.jbutton.button));
		break;
	case SDL_JOYBUTTONDOWN:
		event.reset(new JoystickButtonDownEvent(evt.jbutton.which,
		                                    evt.jbutton.button));
		break;
	case SDL_JOYAXISMOTION:
		event.reset(new JoystickAxisMotionEvent(evt.jaxis.which,
		                                    evt.jaxis.axis,
		                                    evt.jaxis.value));
		break;

	case SDL_ACTIVEEVENT:
		event.reset(new FocusEvent(evt.active.gain));
		break;

	case SDL_VIDEORESIZE:
		event.reset(new ResizeEvent(evt.resize.w, evt.resize.h));
		break;

	case SDL_QUIT:
		event.reset(new QuitEvent());
		break;

	default:
		break;
	}

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
