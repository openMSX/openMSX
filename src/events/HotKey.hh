// $Id$

#ifndef HOTKEY_HH
#define HOTKEY_HH

#include <string>
#include <map>
#include "EventListener.hh"
#include "XMLElementListener.hh"
#include "Keys.hh"
#include "Command.hh"

namespace openmsx {

class XMLElement;

class HotKey : private EventListener, private XMLElementListener
{
public:
	HotKey();
	virtual ~HotKey();

private:
	void initBindings();
	void registerHotKeyCommand(const std::string& key,
	                           const std::string& command);
	void registerHotKeyCommand(const XMLElement& elem);
	void unregisterHotKeyCommand(Keys::KeyCode key);

	// EventListener
	virtual bool signalEvent(const Event& event);

	// XMLElementListener
	virtual void childAdded(const XMLElement& parent,
	                        const XMLElement& child);

	class HotKeyCmd {
	public:
		HotKeyCmd(const XMLElement& elem);
		const std::string& getCommand() const;
		const XMLElement& getElement() const;
		void execute();
	private:
		const XMLElement& elem;
	};

	class BindCmd : public SimpleCommand {
	public:
		BindCmd(HotKey& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& parent;
	} bindCmd;

	class UnbindCmd : public SimpleCommand {
	public:
		UnbindCmd(HotKey& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& parent;
	} unbindCmd;

	typedef std::map<Keys::KeyCode, HotKeyCmd*> CommandMap;
	CommandMap cmdMap;

	XMLElement& bindingsElement;
};

} // namespace openmsx

#endif
