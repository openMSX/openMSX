// $Id$

#ifndef INPUTEVENTGENERATOR_HH
#define INPUTEVENTGENERATOR_HH

#include "Observer.hh"
#include "EventListener.hh"
#include "PollInterface.hh"
#include "Command.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class CommandController;
class BooleanSetting;
class EventDistributor;
class Setting;

class InputEventGenerator : private Observer<Setting>, private EventListener,
                            private PollInterface
{
public:
	InputEventGenerator(Scheduler& scheduler,
	                    CommandController& commandController,
	                    EventDistributor& eventDistributor);
	virtual ~InputEventGenerator();

	/** Poll / wait for an event and handle it.
	  * These methods should be called from the main thread.
	  */
	void wait();
	void notify();
	
	// PollInterface
	virtual void poll();

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
	virtual void signalEvent(const Event& event);

	std::auto_ptr<BooleanSetting> grabInput;
	enum EscapeGrabState {
		ESCAPE_GRAB_WAIT_CMD,
		ESCAPE_GRAB_WAIT_LOST,
		ESCAPE_GRAB_WAIT_GAIN
	} escapeGrabState;

	class EscapeGrabCmd : public SimpleCommand {
	public:
		EscapeGrabCmd(CommandController& commandController,
		              InputEventGenerator& inputEventGenerator);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		InputEventGenerator& inputEventGenerator;
	} escapeGrabCmd;

	bool keyRepeat;
	EventDistributor& eventDistributor;
};

} // namespace openmsx

#endif
