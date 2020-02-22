#ifndef USERSETTINGS_HH
#define USERSETTINGS_HH

#include "Command.hh"
#include <memory>
#include <string_view>
#include <vector>

namespace openmsx {

class Setting;

class UserSettings
{
public:
	using Settings = std::vector<std::unique_ptr<Setting>>;

	explicit UserSettings(CommandController& commandController);

	void addSetting(std::unique_ptr<Setting> setting);
	void deleteSetting(Setting& setting);
	Setting* findSetting(std::string_view name) const;
	const Settings& getSettings() const { return settings; }

private:
	class Cmd final : public Command {
	public:
		explicit Cmd(CommandController& commandController);
		void execute(span<const TclObject> tokens,
			     TclObject& result) override;
		std::string help(const std::vector<std::string>& tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		void create (span<const TclObject> tokens, TclObject& result);
		void destroy(span<const TclObject> tokens, TclObject& result);
		void info   (span<const TclObject> tokens, TclObject& result);

		std::unique_ptr<Setting> createString (span<const TclObject> tokens);
		std::unique_ptr<Setting> createBoolean(span<const TclObject> tokens);
		std::unique_ptr<Setting> createInteger(span<const TclObject> tokens);
		std::unique_ptr<Setting> createFloat  (span<const TclObject> tokens);

		std::vector<std::string_view> getSettingNames() const;
	} userSettingCommand;

	Settings settings; // unordered
};

} // namespace openmsx

#endif
