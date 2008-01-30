// $Id$

#ifndef INPUTEVENTGENERATOR_HH
#define INPUTEVENTGENERATOR_HH

#include "Observer.hh"
#include "EventListener.hh"
#include "noncopyable.hh"
#include "build-info.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class CommandController;
class BooleanSetting;
class EventDistributor;
class Setting;
class EscapeGrabCmd;

class InputEventGenerator : private Observer<Setting>, private EventListener,
                            private noncopyable
{
public:
	InputEventGenerator(CommandController& commandController,
	                    EventDistributor& eventDistributor);
	virtual ~InputEventGenerator();

	/** Poll / wait for an event and handle it.
	  * These methods should be called from the main thread.
	  */
	void wait();
	void notify();
	void poll();

	/**
	 * Enable or disable keyboard event repeats
	 */
	void setKeyRepeat(bool enable);

	/**
	 * This functions shouldn't be needed, but in the SDL library input
	 * and video or closely coupled (sigh). For example when the video mode
	 * is changed we need to reset the keyrepeat and unicode settings.
	 */
	void reinit();

private:
	void handle(const SDL_Event& event);
	void setGrabInput(bool grab);

	// Observer<Setting>
	virtual void update(const Setting& setting);

	// EventListener
	virtual bool signalEvent(shared_ptr<const Event> event);

	EventDistributor& eventDistributor;
	const std::auto_ptr<BooleanSetting> grabInput;
	const std::auto_ptr<EscapeGrabCmd> escapeGrabCmd;
	friend class EscapeGrabCmd;

	enum EscapeGrabState {
		ESCAPE_GRAB_WAIT_CMD,
		ESCAPE_GRAB_WAIT_LOST,
		ESCAPE_GRAB_WAIT_GAIN
	} escapeGrabState;

#if PLATFORM_GP2X
	int stat8; // last joystick status (8 input switches)
#endif
	bool keyRepeat;
};

} // namespace openmsx

#endif
