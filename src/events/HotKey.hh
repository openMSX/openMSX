// $Id$

#ifndef __HOTKEY_HH__
#define __HOTKEY_HH__

#include <SDL/SDL.h>
#include <string>
#include <map>
#include "EventListener.hh"
#include "Keys.hh"


class HotKeyListener
{
	public:
		 /**
		  * This method gets called when the key you are interested in
		  * is pressed.
		  */
		 virtual void signalHotKey(Keys::KeyCode key) = 0;
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
		void   registerHotKey(Keys::KeyCode key, HotKeyListener *listener);
		void unregisterHotKey(Keys::KeyCode key, HotKeyListener *listener);

		/**
		 * When the given hotkey is pressed the given command is
		 * automatically executed.
		 */
		void   registerHotKeyCommand(Keys::KeyCode key, const std::string &command);
		void unregisterHotKeyCommand(Keys::KeyCode key, const std::string &command);

	private:
		class HotKeyCmd : public HotKeyListener
		{
			public:
				HotKeyCmd(const std::string &cmd);
				virtual ~HotKeyCmd();
				const std::string &getCommand();
				virtual void signalHotKey(Keys::KeyCode key);
			private:
				std::string command;
		};
		
		// EventListener
		virtual void signalEvent(SDL_Event &event);

		HotKey();
		static HotKey *oneInstance;

		std::multimap <Keys::KeyCode, HotKeyListener*> map;
		std::multimap <Keys::KeyCode, HotKeyCmd*> cmdMap;
};

#endif
