// $Id$

#ifndef __SETTINGSMANAGER_HH__
#define __SETTINGSMANAGER_HH__

#include "SettingNode.hh"
#include "Command.hh"
#include <cassert>
#include <map>
#include <set>
#include <string>
#include <vector>

using std::map;
using std::set;
using std::string;
using std::vector;


namespace openmsx {

class CommandController;
class Interpreter;

/** Manages all settings.
  */
class SettingsManager
{
private:
	map<string, SettingNode*> settingsMap;

public:
	static SettingsManager& instance();

	/** Get a setting by specifying its name.
	  * @return The SettingLeafNode with the given name,
	  *   or NULL if there is no such SettingLeafNode.
	  */
	SettingLeafNode* getByName(const string& name) const {
		map<string, SettingNode*>::const_iterator it =
			settingsMap.find(name);
		// TODO: The cast is valid because currently all nodes are leaves.
		//       In the future this will no longer be the case.
		return it == settingsMap.end()
			? NULL
			: static_cast<SettingLeafNode*>(it->second);
	}

	void registerSetting(SettingNode& setting);
	void unregisterSetting(SettingNode& setting);

private:
	SettingsManager();
	~SettingsManager();

	template <typename T>
	void getSettingNames(string& result) const;

	template <typename T>
	void getSettingNames(set<string>& result) const;

	template <typename T>
	T* getByName(const string& cmd, const string& name) const;

	class SetCompleter : public CommandCompleter {
	public:
		SetCompleter(SettingsManager* manager);
		virtual void tabCompletion(vector<string>& tokens) const
			throw();
	private:
		SettingsManager* manager;
	} setCompleter;

	class SettingCompleter : public CommandCompleter {
	public:
		SettingCompleter(SettingsManager* manager);
		virtual void tabCompletion(vector<string>& tokens) const
			throw();
	private:
		SettingsManager* manager;
	} settingCompleter;

	class ToggleCommand : public Command {
	public:
		ToggleCommand(SettingsManager* manager);
		virtual string execute(const vector<string>& tokens)
			throw (CommandException);
		virtual string help(const vector<string>& tokens) const
			throw();
		virtual void tabCompletion(vector<string>& tokens) const
			throw();
	private:
		SettingsManager* manager;
	} toggleCommand;

	CommandController& commandController;
	Interpreter& interpreter;
};

} // namespace openmsx

#endif //__SETTINGSMANAGER_HH__
