// $Id$

#ifndef __HOTKEY_HH__
#define __HOTKEY_HH__

#include <SDL/SDL.h>
#include <string>
#include <map>
#include "EventListener.hh"


class HotKeyListener
{
	public:
		 /**
		  * This method gets called when the key you are interested in
		  * is pressed.
		  */
		 virtual void signalHotKey(SDLKey key) = 0;
}; 


class HotKey : private EventListener
{
	public:
		virtual ~HotKey();
		static HotKey *instance();

		/**
		 * This is just an extra filter for SDL_KEYDOWN events, now
		 * events are only passed for specific keys.
		 * See EventDistributor::registerListener for more details
		 */
		void registerHotKey(SDLKey key, HotKeyListener *listener);
		//void unregisterHotKey(HotKeyListener *listener);

		/**
		 * When the given hotkey is pressed the given command is
		 * automatically executed.
		 */
		void registerHotKeyCommand(SDLKey key, const std::string &command);
		//void unregisterHotKeyCommand(const std::string &command);

	private:
		// EventListener
		virtual void signalEvent(SDL_Event &event);

		HotKey();
		static HotKey *oneInstance;

		int nbListeners;
		std::multimap <SDLKey, HotKeyListener*> map;

	class HotKeyCmd : public HotKeyListener
	{
		public:
			HotKeyCmd(const std::string &cmd);
			virtual ~HotKeyCmd();
			virtual void signalHotKey(SDLKey key);
		private:
			std::string command;
	};
};

#endif
