// $Id$

#include "StringSetting.hh"


namespace openmsx {

StringSetting::StringSetting(
	const string &name_, const string &description_,
	const string &initialValue)
	: Setting<string>(name_, description_, initialValue)
{
}

string StringSetting::getValueString() const
{
	return getValue();
}

void StringSetting::setValueString(const string &newValue)
{
	setValue(newValue);
}

} // namespace openmsx
