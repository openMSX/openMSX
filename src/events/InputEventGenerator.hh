// $Id$

#ifndef __INPUTEVENTGENERATOR_HH__
#define __INPUTEVENTGENERATOR_HH__

#include "SettingListener.hh"
#include "EventListener.hh"
#include "Command.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class BooleanSetting;
class EventDistributor;

class InputEventGenerator : private SettingListener, private EventListener
{
public:
	static InputEventGenerator& instance();

	/** Poll / wait for an event and handle it.
	  * These methods should be called from the main thread.
	  */
	void poll();
	void wait();
	void notify();

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
	InputEventGenerator();
	virtual ~InputEventGenerator();

	void handle(const SDL_Event &event);
	void setGrabInput(bool grab);

	// SettingListener
	virtual void update(const Setting* setting);

	// EventListener
	virtual bool signalEvent(const Event& event);

	std::auto_ptr<BooleanSetting> grabInput;
	enum EscapeGrabState {
		ESCAPE_GRAB_WAIT_CMD,
		ESCAPE_GRAB_WAIT_LOST,
		ESCAPE_GRAB_WAIT_GAIN
	} escapeGrabState;

	class EscapeGrabCmd : public SimpleCommand {
	public:
		EscapeGrabCmd(InputEventGenerator& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		InputEventGenerator& parent;
	} escapeGrabCmd;
	
	bool keyRepeat;
	EventDistributor& distributor;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__
