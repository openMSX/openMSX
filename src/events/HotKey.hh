// $Id$

#ifndef HOTKEY_HH
#define HOTKEY_HH

#include "UserInputEventListener.hh"
#include "Keys.hh"
#include "Command.hh"
#include <map>
#include <set>
#include <string>

namespace openmsx {

class XMLElement;
class UserInputEventDistributor;

class HotKey : private UserInputEventListener
{
public:
	HotKey(UserInputEventDistributor& userInputEventDistributor);
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
	virtual bool signalEvent(const UserInputEvent& event);

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

	class BindDefaultCmd : public SimpleCommand {
	public:
		BindDefaultCmd(HotKey& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& parent;
	} bindDefaultCmd;

	class UnbindDefaultCmd : public SimpleCommand {
	public:
		UnbindDefaultCmd(HotKey& parent);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
	private:
		HotKey& parent;
	} unbindDefaultCmd;

	typedef std::map<Keys::KeyCode, std::string> BindMap;
	typedef std::set<Keys::KeyCode> KeySet;
	BindMap cmdMap;
	BindMap defaultMap;
	KeySet boundKeys;
	KeySet unboundKeys;
	UserInputEventDistributor& userInputEventDistributor;
	bool loading; // hack
};

} // namespace openmsx

#endif
