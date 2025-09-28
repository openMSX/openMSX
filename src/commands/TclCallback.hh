#ifndef TCLCALLBACK_HH
#define TCLCALLBACK_HH

#include "StringSetting.hh"
#include "TclObject.hh"

#include "static_string_view.hh"

#include <optional>
#include <string_view>

namespace openmsx {

class CommandController;

class TclCallback
{
public:
	friend class TclCallbackMessages;

	TclCallback(CommandController& controller,
	            std::string_view name,
	            static_string_view description,
	            std::string_view defaultValue,
	            Setting::Save saveSetting,
	            bool isMessageCallback = false);
	explicit TclCallback(StringSetting& setting);

	TclObject execute() const;
	TclObject execute(int arg1) const;
	TclObject execute(int arg1, int arg2) const;
	TclObject execute(int arg1, std::string_view arg2) const;
	TclObject execute(std::string_view arg1, std::string_view arg2) const;

	[[nodiscard]] TclObject getValue() const;
	[[nodiscard]] StringSetting& getSetting() const { return callbackSetting; }

private:
	TclObject executeCommon(TclObject& command) const;

	std::optional<StringSetting> callbackSetting2;
	StringSetting& callbackSetting;
	const bool isMessageCallback;
};

} // namespace openmsx

#endif
