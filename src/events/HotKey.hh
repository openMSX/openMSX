// $Id$

#ifndef __HOTKEY_HH__
#define __HOTKEY_HH__

#include <string>
#include <map>
#include "EventListener.hh"
#include "Keys.hh"
#include "Command.hh"

using std::string;
using std::multimap;

namespace openmsx {

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
	void   registerHotKeyCommand(Keys::KeyCode key, const string &command);
	void unregisterHotKeyCommand(Keys::KeyCode key, const string &command);

private:
	class HotKeyCmd : public HotKeyListener
	{
		public:
			HotKeyCmd(const string &cmd);
			virtual ~HotKeyCmd();
			const string &getCommand();
			virtual void signalHotKey(Keys::KeyCode key);
		private:
			string command;
	};
	
	// EventListener
	virtual bool signalEvent(SDL_Event &event) throw();

	HotKey();

	multimap <Keys::KeyCode, HotKeyListener*> map;
	multimap <Keys::KeyCode, HotKeyCmd*> cmdMap;

	class BindCmd : public Command {
		public:
			virtual string execute(const vector<string> &tokens)
				throw (CommandException);
			virtual string help(const vector<string> &tokens) const
				throw();
	};
	friend class BindCmd;
	BindCmd bindCmd;

	class UnbindCmd : public Command {
		public:
			virtual string execute(const vector<string> &tokens)
				throw (CommandException);
			virtual string help(const vector<string> &tokens) const
				throw();
	};
	friend class UnbindCmd;
	UnbindCmd unbindCmd;
};

} // namespace openmsx

#endif
