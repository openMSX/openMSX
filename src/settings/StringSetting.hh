// $Id$

#ifndef __STRINGSETTING_HH__
#define __STRINGSETTING_HH__

#include "SettingPolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class StringSettingPolicy : public SettingPolicy<std::string>
{
protected:
	const std::string& toString(const std::string& value) const;
	const std::string& fromString(const std::string& str) const;
};

class StringSetting : public SettingImpl<StringSettingPolicy>
{
public:
	StringSetting(const std::string& name, const std::string& description,
	              const std::string& initialValue);
};

} // namespace openmsx

#endif //__STRINGSETTING_HH__
