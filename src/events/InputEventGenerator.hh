// $Id$

#ifndef __INPUTEVENTGENERATOR_HH__
#define __INPUTEVENTGENERATOR_HH__

#include <SDL/SDL.h>
#include "BooleanSetting.hh"
#include "SettingListener.hh"

namespace openmsx {

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

private:
	InputEventGenerator();
	virtual ~InputEventGenerator();

	void handle(const SDL_Event &event);

	// SettingListener
	virtual void update(const SettingLeafNode* setting) throw();

	BooleanSetting grabInput;

	EventDistributor& distributor;
};

} // namespace openmsx

#endif // __EVENTDISTRIBUTOR_HH__
