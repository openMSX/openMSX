// $Id$

#include "StringSetting.hh"
#include "SettingsManager.hh"

namespace openmsx {

// class StringSettingBase

StringSettingBase::StringSettingBase(
	const string& name, const string& description,
        const string& initialValue)
	: Setting<string>(name, description, initialValue)
{
}

string StringSettingBase::getValueString() const
{
	return getValue();
}

void StringSettingBase::setValueString(const string& newValue)
{
	setValue(newValue);
}


// class StringSetting

StringSetting::StringSetting(const string& name, const string& description,
                             const string& initialValue)
	: StringSettingBase(name, description, initialValue)
{
	SettingsManager::instance().registerSetting(*this);
}

StringSetting::~StringSetting()
{
	SettingsManager::instance().unregisterSetting(*this);
}

} // namespace openmsx
