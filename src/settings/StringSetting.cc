#include "StringSetting.hh"

namespace openmsx {

StringSetting::StringSetting(CommandController& commandController,
                             string_ref name, string_ref description,
                             string_ref initialValue, SaveSetting save)
	: Setting(commandController, name, description,
	          initialValue.str(), save)
{
}

string_ref StringSetting::getTypeString() const
{
	return "string";
}

} // namespace openmsx
