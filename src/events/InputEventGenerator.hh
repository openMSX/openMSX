// $Id$

#ifndef __INPUTEVENTGENERATOR_HH__
#define __INPUTEVENTGENERATOR_HH__

#include "SettingListener.hh"
#include <SDL.h>
#include <memory>

namespace openmsx {

class BooleanSetting;
class EventDistributor;

class InputEventGenerator : private SettingListener
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

	// SettingListener
	virtual void update(const Setting* setting);

	std::auto_ptr<BooleanSetting> grabInput;
	bool keyRepeat;

	EventDistributor& distributor;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__
