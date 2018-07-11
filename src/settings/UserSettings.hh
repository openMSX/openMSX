#ifndef USERSETTINGS_HH
#define USERSETTINGS_HH

#include "Command.hh"
#include "string_view.hh"
#include <vector>
#include <memory>

namespace openmsx {

class Setting;

class UserSettings
{
public:
	using Settings = std::vector<std::unique_ptr<Setting>>;

	explicit UserSettings(CommandController& commandController);

	void addSetting(std::unique_ptr<Setting> setting);
	void deleteSetting(Setting& setting);
	Setting* findSetting(string_view name) const;
	const Settings& getSettings() const { return settings; }

private:
	class Cmd final : public Command {
	public:
		explicit Cmd(CommandController& commandController);
		void execute(array_ref<TclObject> tokens,
			     TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		void create (array_ref<TclObject> tokens, TclObject& result);
		void destroy(array_ref<TclObject> tokens, TclObject& result);
		void info   (array_ref<TclObject> tokens, TclObject& result);

		std::unique_ptr<Setting> createString (array_ref<TclObject> tokens);
		std::unique_ptr<Setting> createBoolean(array_ref<TclObject> tokens);
		std::unique_ptr<Setting> createInteger(array_ref<TclObject> tokens);
		std::unique_ptr<Setting> createFloat  (array_ref<TclObject> tokens);

		std::vector<string_view> getSettingNames() const;
	} userSettingCommand;

	Settings settings; // unordered
};

} // namespace openmsx

#endif
