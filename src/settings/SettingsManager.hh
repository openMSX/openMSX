#ifndef SETTINGSMANAGER_HH
#define SETTINGSMANAGER_HH

#include "Command.hh"
#include "InfoTopic.hh"
#include "StringMap.hh"
#include "string_ref.hh"
#include "noncopyable.hh"

namespace openmsx {

class BaseSetting;
class GlobalCommandController;
class XMLElement;

/** Manages all settings.
  */
class SettingsManager : private noncopyable
{
public:
	explicit SettingsManager(GlobalCommandController& commandController);
	~SettingsManager();

	/** Find the setting with given name.
	  * @return The requested setting or nullptr.
	  */
	BaseSetting* findSetting(string_ref name) const;

	void loadSettings(const XMLElement& config);

	void registerSetting  (BaseSetting& setting, string_ref name);
	void unregisterSetting(BaseSetting& setting, string_ref name);

private:
	BaseSetting& getByName(string_ref cmd, string_ref name) const;

	class SettingInfo final : public InfoTopic {
	public:
		SettingInfo(InfoCommand& openMSXInfoCommand, SettingsManager& manager);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) const override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		SettingsManager& manager;
	} settingInfo;

	class SetCompleter final : public CommandCompleter {
	public:
		SetCompleter(CommandController& commandController,
			     SettingsManager& manager);
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		SettingsManager& manager;
	} setCompleter;

	class SettingCompleter final : public CommandCompleter {
	public:
		SettingCompleter(CommandController& commandController,
				 SettingsManager& manager,
				 const std::string& name);
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;
	private:
		SettingsManager& manager;
	};
	SettingCompleter incrCompleter;
	SettingCompleter unsetCompleter;

	StringMap<BaseSetting*> settingsMap;
};

} // namespace openmsx

#endif
