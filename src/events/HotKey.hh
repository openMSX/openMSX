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
	 virtual void signalHotKey(Keys::KeyCode key) throw() = 0;
};


class HotKey : private EventListener
{
public:
	static HotKey* instance();

	/**
	 * This is just an extra filter for SDL_KEYDOWN events, now
	 * events are only passed for specific keys.
	 * See EventDistributor::registerListener for more details
	 */
	void   registerHotKey(Keys::KeyCode key, HotKeyListener* listener);
	void unregisterHotKey(Keys::KeyCode key, HotKeyListener* listener);

	/**
	 * When the given hotkey is pressed the given command is
	 * automatically executed.
	 */
	void   registerHotKeyCommand(Keys::KeyCode key, const string& command);
	void unregisterHotKeyCommand(Keys::KeyCode key, const string& command);

private:
	HotKey();
	virtual ~HotKey();

	// EventListener
	virtual bool signalEvent(SDL_Event& event) throw();
	
	class HotKeyCmd;
	typedef multimap<Keys::KeyCode, HotKeyListener*> ListenerMap;
	ListenerMap map;
	typedef multimap<Keys::KeyCode, HotKeyCmd*> CommandMap;
	CommandMap cmdMap;


	class HotKeyCmd : public HotKeyListener {
	public:
		HotKeyCmd(const string& cmd);
		virtual ~HotKeyCmd();
		const string& getCommand() const;
		virtual void signalHotKey(Keys::KeyCode key) throw();
	private:
		string command;
	};

	class BindCmd : public Command {
	public:
		virtual string execute(const vector<string>& tokens)
			throw (CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
	} bindCmd;
	friend class BindCmd;

	class UnbindCmd : public Command {
	public:
		virtual string execute(const vector<string>& tokens)
			throw (CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
	} unbindCmd;
	friend class UnbindCmd;
};

} // namespace openmsx

#endif
