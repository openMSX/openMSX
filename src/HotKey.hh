// $Id$

#ifndef __HOTKEY_HH__
#define __HOTKEY_HH__

#include <SDL/SDL.h>
#include "EventDistributor.hh"
#include "Mutex.hh"

class HotKeyListener
{
	 public:
		 /**
		  * This method gets called when the key you are interested in
		  * is pressed.
		  * Note: HotKeys are deliverd in a different thread!!
		  */
		 virtual void signalHotKey(SDLKey key) = 0;
}; 


class HotKey : public EventListener
{
	public:
		virtual ~HotKey();
		static HotKey *instance();

		/**
		 * This is just an extra filter for SDL_KEYDOWN events, now
		 * events are only passed for specific keys.
		 * See EventDistributor::registerAsyncListener for more details
		 */
		void registerAsyncHotKey(SDLKey key, HotKeyListener *listener);


		// EventListener
		void signalEvent(SDL_Event &event);

	private:
		HotKey();
		static HotKey *oneInstance;

		int nbListeners;
		std::multimap <SDLKey, HotKeyListener*> map;
		Mutex mapMutex;	// to lock variable map
};

#endif
