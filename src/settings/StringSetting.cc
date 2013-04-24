#include "StringSetting.hh"

using std::string;

namespace openmsx {

// class StringSettingPolicy

StringSettingPolicy::StringSettingPolicy()
{
}

const string& StringSettingPolicy::toString(const string& value) const
{
	return value;
}

const string& StringSettingPolicy::fromString(const string& str) const
{
	return str;
}

string_ref StringSettingPolicy::getTypeString() const
{
	return "string";
}

// class StringSetting

StringSetting::StringSetting(CommandController& commandController,
                             string_ref name, string_ref description,
                             string_ref initialValue, SaveSetting save)
	: SettingImpl<StringSettingPolicy>(
		commandController, name, description, initialValue.str(), save)
{
}

} // namespace openmsx
