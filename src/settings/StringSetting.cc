// $Id$

#include "StringSetting.hh"

namespace openmsx {

// class StringSettingBase

StringSettingBase::StringSettingBase(
	const string& name, const string& description,
        const string& initialValue)
	: Setting<string>(name, description, initialValue)
{
}

StringSettingBase::StringSettingBase(
	XMLElement& node, const string& description)
	: Setting<string>(node, description, node.getData())
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
	initSetting();
}

StringSetting::StringSetting(XMLElement& node, const string& description)
	: StringSettingBase(node, description)
{
	initSetting();
}

StringSetting::~StringSetting()
{
	exitSetting();
}

} // namespace openmsx
