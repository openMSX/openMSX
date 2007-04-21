// $Id$

#include "StringSetting.hh"

using std::string;

namespace openmsx {

// class StringSettingPolicy

StringSettingPolicy::StringSettingPolicy(CommandController& commandController)
	: SettingPolicy<string>(commandController)
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

string StringSettingPolicy::getTypeString() const
{
	return "string";
}

// class StringSetting

StringSetting::StringSetting(CommandController& commandController,
                             const string& name, const string& description,
                             const string& initialValue, SaveSetting save)
	: SettingImpl<StringSettingPolicy>(
		commandController, name, description, initialValue, save)
{
}

} // namespace openmsx
