// $Id$

#ifndef __HOTKEY_HH__
#define __HOTKEY_HH__

#include <string>
#include <map>
#include "EventListener.hh"
#include "Keys.hh"
#include "Command.hh"


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
		virtual bool signalEvent(SDL_Event &event);

		HotKey();

		std::multimap <Keys::KeyCode, HotKeyListener*> map;
		std::multimap <Keys::KeyCode, HotKeyCmd*> cmdMap;

		class BindCmd : public Command {
			public:
				virtual void execute(const std::vector<std::string> &tokens);
				virtual void help   (const std::vector<std::string> &tokens);
		};
		friend class BindCmd;
		BindCmd bindCmd;
			
		class UnbindCmd : public Command {
			public:
				virtual void execute(const std::vector<std::string> &tokens);
				virtual void help   (const std::vector<std::string> &tokens);
		};
		friend class UnbindCmd;
		UnbindCmd unbindCmd;
};

#endif
