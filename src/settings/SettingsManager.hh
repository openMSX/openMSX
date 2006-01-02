// $Id$

#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include "noncopyable.hh"
#include <map>
#include <set>
#include <string>

namespace openmsx {

class Setting;
class CommandController;
class XMLElement;

/** Manages all settings.
  */
class SettingsManager : private noncopyable
{
private:
	typedef std::map<std::string, Setting*> SettingsMap;
	SettingsMap settingsMap;

public:
	explicit SettingsManager(CommandController& commandController);
	~SettingsManager();

	/** Get a setting by specifying its name.
	  * @return The Setting with the given name,
	  *   or NULL if there is no such Setting.
	  */
	Setting* getByName(const std::string& name) const;

	void loadSettings(const XMLElement& config);
	void saveSettings(XMLElement& config) const;

	void registerSetting(Setting& setting);
	void unregisterSetting(Setting& setting);

private:
	template <typename T>
	void getSettingNames(std::string& result) const;

	template <typename T>
	void getSettingNames(std::set<std::string>& result) const;

	template <typename T>
	T& getByName(const std::string& cmd, const std::string& name) const;

	class SettingInfo : public InfoTopic {
	public:
		SettingInfo(CommandController& commandController,
		            SettingsManager& manager);
		virtual void execute(const std::vector<TclObject*>& tokens,
		                     TclObject& result) const;
		virtual std::string help(
			const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsManager& manager;
	} settingInfo;

	class SetCompleter : public CommandCompleter {
	public:
		SetCompleter(CommandController& commandController,
		             SettingsManager& manager);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsManager& manager;
	} setCompleter;

	class SettingCompleter : public CommandCompleter {
	public:
		SettingCompleter(CommandController& commandController,
		                 SettingsManager& manager,
		                 const std::string& name);
		virtual std::string help(const std::vector<std::string>& tokens) const;
		virtual void tabCompletion(std::vector<std::string>& tokens) const;
	private:
		SettingsManager& manager;
	};
	SettingCompleter incrCompleter;
	SettingCompleter unsetCompleter;

	CommandController& commandController;
};

} // namespace openmsx

#endif
