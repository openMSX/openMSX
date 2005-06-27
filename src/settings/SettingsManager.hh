// $Id$

#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "Command.hh"
#include <map>
#include <set>
#include <string>
#include <vector>

namespace openmsx {

class Setting;
class CommandController;
class Interpreter;

/** Manages all settings.
  */
class SettingsManager
{
private:
	typedef std::map<std::string, Setting*> SettingsMap;
	SettingsMap settingsMap;

public:
	SettingsManager();
	~SettingsManager();

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or NULL if there is no such Setting.
	  */
	Setting* getByName(const std::string& name) const;

	/** Fills the given vector with all known settings
	 */
	void getAllSettings(std::vector<const Setting*>& result) const;

	void registerSetting(Setting& setting);
	void unregisterSetting(Setting& setting);

private:
	template <typename T>
	void getSettingNames(std::string& result) const;

	template <typename T>
	void getSettingNames(std::set<std::string>& result) const;

	template <typename T>
	T& getByName(const std::string& cmd, const std::string& name) const;

	class SetCompleter : public CommandCompleter {
	public:
		SetCompleter(SettingsManager& manager);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsManager& manager;
	} setCompleter;

	class SettingCompleter : public CommandCompleter {
	public:
		SettingCompleter(SettingsManager& manager);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsManager& manager;
	} settingCompleter;

	class ToggleCommand : public SimpleCommand {
	public:
		ToggleCommand(SettingsManager& manager);
		virtual std::string execute(const std::vector<std::string>& tokens);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsManager& manager;
	} toggleCommand;

	CommandController& commandController;
	Interpreter& interpreter;
};

} // namespace openmsx

#endif
