// $Id$

#ifndef __SETTINGSMANAGER_HH__
#define __SETTINGSMANAGER_HH__

#include <cassert>
#include <map>
#include <set>
#include <string>
#include <vector>
#include "Setting.hh"
#include "Command.hh"

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
	typedef map<string, Setting*> SettingsMap;
	SettingsMap settingsMap;

public:
	static SettingsManager& instance();

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or NULL if there is no such Setting.
	  */
	Setting* getByName(const string& name) const {
		SettingsMap::const_iterator it = settingsMap.find(name);
		// TODO: The cast is valid because currently all nodes are leaves.
		//       In the future this will no longer be the case.
		return it != settingsMap.end() ? it->second : NULL;
	}

	void registerSetting(Setting& setting);
	void unregisterSetting(Setting& setting);

private:
	SettingsManager();
	~SettingsManager();

	template <typename T>
	void getSettingNames(string& result) const;

	template <typename T>
	void getSettingNames(set<string>& result) const;

	template <typename T>
	T& getByName(const string& cmd, const string& name) const;

	class SetCompleter : public CommandCompleter {
	public:
		SetCompleter(SettingsManager& manager);
		virtual string help(const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;
	private:
		SettingsManager& manager;
	} setCompleter;

	class SettingCompleter : public CommandCompleter {
	public:
		SettingCompleter(SettingsManager& manager);
		virtual string help(const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;
	private:
		SettingsManager& manager;
	} settingCompleter;

	class ToggleCommand : public SimpleCommand {
	public:
		ToggleCommand(SettingsManager& manager);
		virtual string execute(const vector<string>& tokens);
		virtual string help(const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;
	private:
		SettingsManager& manager;
	} toggleCommand;

	CommandController& commandController;
	Interpreter& interpreter;
};

} // namespace openmsx

#endif //__SETTINGSMANAGER_HH__
