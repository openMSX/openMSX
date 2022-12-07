#ifndef TCLCALLBACK_HH
#define TCLCALLBACK_HH

#include "static_string_view.hh"
#include "StringSetting.hh"
#include "TclObject.hh"
#include <optional>
#include <string_view>

namespace openmsx {

class CommandController;

class TclCallback
{
public:
	TclCallback(CommandController& controller,
	            std::string_view name,
	            static_string_view description,
	            std::string_view defaultValue,
	            Setting::SaveSetting saveSetting,
	            bool useCliComm = true);
	explicit TclCallback(StringSetting& setting);

	TclObject execute();
	TclObject execute(int arg1);
	TclObject execute(int arg1, int arg2);
	TclObject execute(int arg1, std::string_view arg2);
	TclObject execute(std::string_view arg1, std::string_view arg2);

	[[nodiscard]] TclObject getValue() const;
	[[nodiscard]] StringSetting& getSetting() const { return callbackSetting; }

private:
	TclObject executeCommon(TclObject& command);

	std::optional<StringSetting> callbackSetting2;
	StringSetting& callbackSetting;
	const bool useCliComm;
};

} // namespace openmsx

#endif
