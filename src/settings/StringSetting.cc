// $Id$

#include "StringSetting.hh"

namespace openmsx {

// class StringSettingBase

StringSettingBase::StringSettingBase(
	const string& name, const string& description,
        const string& initialValue, XMLElement* node)
	: Setting<string>(name, description, initialValue, node)
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
                             const string& initialValue, XMLElement* node)
	: StringSettingBase(name, description, initialValue, node)
{
	initSetting();
}

StringSetting::~StringSetting()
{
	exitSetting();
}

} // namespace openmsx
