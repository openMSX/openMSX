#ifndef USERSETTINGS_HH
#define USERSETTINGS_HH

#include "Command.hh"
#include "static_string_view.hh"
#include <memory>
#include <string_view>
#include <vector>

namespace openmsx {

class Setting;

class UserSettings
{
public:
	struct Info {
		std::unique_ptr<Setting> setting;
		StringStorage description; // because setting doesn't take ownership
	};
	using Settings = std::vector<Info>;

	explicit UserSettings(CommandController& commandController);

	void addSetting(Info&& info);
	void deleteSetting(Setting& setting);
	[[nodiscard]] Setting* findSetting(std::string_view name) const;
	[[nodiscard]] const Settings& getSettingsInfo() const { return settings; }

private:
	class Cmd final : public Command {
	public:
		explicit Cmd(CommandController& commandController);
		void execute(span<const TclObject> tokens,
			     TclObject& result) override;
		[[nodiscard]] std::string help(span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		void create (span<const TclObject> tokens, TclObject& result);
		void destroy(span<const TclObject> tokens, TclObject& result);
		void info   (span<const TclObject> tokens, TclObject& result);

		[[nodiscard]] Info createString (span<const TclObject> tokens);
		[[nodiscard]] Info createBoolean(span<const TclObject> tokens);
		[[nodiscard]] Info createInteger(span<const TclObject> tokens);
		[[nodiscard]] Info createFloat  (span<const TclObject> tokens);
		[[nodiscard]] Info createEnum   (span<const TclObject> tokens);

		[[nodiscard]] std::vector<std::string_view> getSettingNames() const;
	} userSettingCommand;

	Settings settings; // unordered
};

} // namespace openmsx

#endif
