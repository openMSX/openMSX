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

class XMLElement;

class HotKey : private EventListener
{
public:
	HotKey();
	virtual ~HotKey();

private:
	void   registerHotKeyCommand(Keys::KeyCode key, const string& command);
	void unregisterHotKeyCommand(Keys::KeyCode key);

	// EventListener
	virtual bool signalEvent(const Event& event);
	
	class HotKeyCmd {
	public:
		HotKeyCmd(const XMLElement& elem);
		const string& getCommand() const;
		const XMLElement& getElement() const;
		void execute();
	private:
		const XMLElement& elem;
	};

	class BindCmd : public SimpleCommand {
	public:
		BindCmd(HotKey& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		HotKey& parent;
	} bindCmd;

	class UnbindCmd : public SimpleCommand {
	public:
		UnbindCmd(HotKey& parent);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
	private:
		HotKey& parent;
	} unbindCmd;
	
	typedef map<Keys::KeyCode, HotKeyCmd*> CommandMap;
	CommandMap cmdMap;
	
	XMLElement& bindingsElement;
};

} // namespace openmsx

#endif
