// $Id$

#ifndef __EVENTLISTENER_HH__
#define __EVENTLISTENER_HH__

#include <SDL/SDL.h>


class EventListener
{
	public:
		/**
		 * This method gets called when an event you are interested in
		 * occurs. 
		 * Note: asynchronous events are deliverd in a different thread!!
		 */
		virtual void signalEvent(SDL_Event &event) = 0;
};

#endif
