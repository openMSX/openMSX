// $Id$

#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "EventListener.hh"
#include "Keys.hh"
#include "Command.hh"
#include <map>
#include <set>
#include <string>

namespace openmsx {

class CommandController;
class EventDistributor;
class XMLElement;

class HotKey : private EventListener
{
public:
	HotKey(CommandController& commandController,
	       EventDistributor& eventDistributor);
	virtual ~HotKey();

	void loadBindings(const XMLElement& config);
	void saveBindings(XMLElement& config) const;

private:
	void initDefaultBindings();
	void bind  (Keys::KeyCode key, const std::string& command);
	void unbind(Keys::KeyCode key);
	void bindDefault  (Keys::KeyCode key, const std::string& command);
	void unbindDefault(Keys::KeyCode key);

	// EventListener
	virtual void signalEvent(const Event& event);

	class BindCmd : public SimpleCommand {
	public:
		BindCmd(CommandController& commandController, HotKey& hotKey);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& hotKey;
	} bindCmd;

	class UnbindCmd : public SimpleCommand {
	public:
		UnbindCmd(CommandController& commandController, HotKey& hotKey);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& hotKey;
	} unbindCmd;

	class BindDefaultCmd : public SimpleCommand {
	public:
		BindDefaultCmd(CommandController& commandController, HotKey& hotKey);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& hotKey;
	} bindDefaultCmd;

	class UnbindDefaultCmd : public SimpleCommand {
	public:
		UnbindDefaultCmd(CommandController& commandController, HotKey& hotKey);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& hotKey;
	} unbindDefaultCmd;

	typedef std::map<Keys::KeyCode, std::string> BindMap;
	typedef std::set<Keys::KeyCode> KeySet;
	BindMap cmdMap;
	BindMap defaultMap;
	KeySet boundKeys;
	KeySet unboundKeys;
	CommandController& commandController;
	EventDistributor& eventDistributor;
	bool loading; // hack
};

} // namespace openmsx

#endif
