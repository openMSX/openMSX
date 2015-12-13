#include "StringSetting.hh"

namespace openmsx {

StringSetting::StringSetting(CommandController& commandController_,
                             string_ref name_, string_ref description_,
                             string_ref initialValue, SaveSetting save_)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), save_)
{
	init();
}

string_ref StringSetting::getTypeString() const
{
	return "string";
}

} // namespace openmsx
