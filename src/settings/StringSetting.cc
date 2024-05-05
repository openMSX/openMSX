#include "StringSetting.hh"

namespace openmsx {

StringSetting::StringSetting(CommandController& commandController_,
                             std::string_view name_, static_string_view description_,
                             std::string_view initialValue, Save save_)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), save_)
{
	init();
}

std::string_view StringSetting::getTypeString() const
{
	return "string";
}

} // namespace openmsx
