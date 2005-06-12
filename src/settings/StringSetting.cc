// $Id$

#include "StringSetting.hh"

using std::string;

namespace openmsx {

// class StringSettingPolicy

const string& StringSettingPolicy::toString(const string& value) const
{
	return value;
}

const string& StringSettingPolicy::fromString(const string& str) const
{
	return str;
}

// class StringSetting

StringSetting::StringSetting(const string& name, const string& description,
                             const string& initialValue)
	: SettingImpl<StringSettingPolicy>(name, description, initialValue,
	                                   Setting::SAVE)
{
}

} // namespace openmsx
