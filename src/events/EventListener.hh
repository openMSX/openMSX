// $Id$

#ifndef __EVENTLISTENER_HH__
#define __EVENTLISTENER_HH__

#include <SDL/SDL.h>


namespace openmsx {

class EventListener
{
public:
	/**
	 * This method gets called when an event you are interested in
	 * occurs.
	 * This method should return true when lower priority
	 * EventListener may also receive this event (normally always
	 * the case except for Console)
	 */
	virtual bool signalEvent(SDL_Event &event) throw() = 0;
};

} // namespace openmsx

#endif
