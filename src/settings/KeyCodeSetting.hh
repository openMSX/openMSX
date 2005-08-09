// $Id$

#ifndef KEYCODESETTING_HH
#define KEYCODESETTING_HH

#include "SettingPolicy.hh"
#include "SettingImpl.hh"
#include "Keys.hh"

namespace openmsx {

class KeyCodeSettingPolicy : public SettingPolicy<Keys::KeyCode>
{
protected:
	std::string toString(Keys::KeyCode key) const;
	Keys::KeyCode fromString(const std::string& str) const;
};

class KeyCodeSetting : public SettingImpl<KeyCodeSettingPolicy>
{
public:
	KeyCodeSetting(const std::string& name, const std::string& description,
	               Keys::KeyCode initialValue);
};

} // namespace openmsx

#endif
