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
	virtual ~HotKeyListener() {}
	
	 /**
	  * This method gets called when the key you are interested in
	  * is pressed.
	  */
	 virtual void signalHotKey(Keys::KeyCode key) throw() = 0;
};

class HotKey : private EventListener
{
public:
	HotKey();
	virtual ~HotKey();

	/**
	 * This is just an extra filter for KEY_DOWN events, now
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
	void unregisterHotKeyCommand(Keys::KeyCode key);

private:
	// EventListener
	virtual bool signalEvent(const Event& event) throw();
	
	class HotKeyCmd;
	typedef multimap<Keys::KeyCode, HotKeyListener*> ListenerMap;
	ListenerMap listenerMap;
	typedef map<Keys::KeyCode, HotKeyCmd*> CommandMap;
	CommandMap cmdMap;


	class HotKeyCmd : public HotKeyListener {
	public:
		HotKeyCmd(const string& cmd);
		virtual ~HotKeyCmd();
		const string& getCommand() const;
		virtual void signalHotKey(Keys::KeyCode key) throw();
	private:
		const string command;
	};

	class BindCmd : public SimpleCommand {
	public:
		BindCmd(HotKey& parent);
		virtual string execute(const vector<string>& tokens)
			throw (CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
	private:
		HotKey& parent;
	} bindCmd;

	class UnbindCmd : public SimpleCommand {
	public:
		UnbindCmd(HotKey& parent);
		virtual string execute(const vector<string>& tokens)
			throw (CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
	private:
		HotKey& parent;
	} unbindCmd;
};

} // namespace openmsx

#endif
