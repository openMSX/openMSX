#ifndef USERSETTINGS_HH
#define USERSETTINGS_HH

#include "Command.hh"

#include "outer.hh"
#include "view.hh"

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

private:
	class Cmd final : public Command {
	public:
		explicit Cmd(CommandController& commandController);
		void execute(std::span<const TclObject> tokens,
			     TclObject& result) override;
		[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
		void tabCompletion(std::vector<std::string>& tokens) const override;

	private:
		void create (std::span<const TclObject> tokens, TclObject& result);
		void destroy(std::span<const TclObject> tokens, TclObject& result);
		void info   (std::span<const TclObject> tokens, TclObject& result) const;

		[[nodiscard]] Info createString (std::span<const TclObject> tokens) const;
		[[nodiscard]] Info createBoolean(std::span<const TclObject> tokens) const;
		[[nodiscard]] Info createInteger(std::span<const TclObject> tokens) const;
		[[nodiscard]] Info createFloat  (std::span<const TclObject> tokens) const;
		[[nodiscard]] Info createEnum   (std::span<const TclObject> tokens) const;

		[[nodiscard]] auto getSettingNames() const {
			return view::transform(
				OUTER(UserSettings, userSettingCommand).settings,
				[](const auto& info) { return info.setting->getFullName(); });
		}
	} userSettingCommand;

	Settings settings; // unordered
};

} // namespace openmsx

#endif
