// $Id$

#ifndef __USEREVENTS_HH__
#define __USEREVENTS_HH__

#include <SDL/SDL.h>
#include "RendererFactory.hh"

class UserEvents
{
public:
	/** List of all user events defined by openMSX.
	  */
	enum UserEventID {
		/** Quit openMSX.
		  * This is different from the SDL_QUIT event, which is sent
		  * when the window closes.
		  */
		QUIT,
		/** Used by the emulation thread to instruct to main thread
		  * to create a new Renderer.
		  */
		RENDERER_SWITCH,
		/** Used by the emulation thread to instruct to main thread
		  * to toggle the full screen mode of an SDL screen.
		  */
		FULL_SCREEN_TOGGLE,
		};

	/** Push a user event into the event queue.
	  * @param id The event to send.
	  * @param data1 Pointer to arbitraty user data.
	  * @param data2 Another pointer to arbitraty user data.
	  */
	static void push(UserEventID id, void *data1 = NULL, void *data2 = NULL);
	
	/** Handle the given user event.
	  */
	static void handle(SDL_UserEvent &event);

};

#endif // __USEREVENTS_HH__
