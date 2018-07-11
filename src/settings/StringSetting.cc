#include "StringSetting.hh"

namespace openmsx {

StringSetting::StringSetting(CommandController& commandController_,
                             string_view name_, string_view description_,
                             string_view initialValue, SaveSetting save_)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), save_)
{
	init();
}

string_view StringSetting::getTypeString() const
{
	return "string";
}

} // namespace openmsx
