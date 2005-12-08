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
	explicit KeyCodeSettingPolicy(CommandController& commandController);
	std::string toString(Keys::KeyCode key) const;
	Keys::KeyCode fromString(const std::string& str) const;
	std::string getTypeString() const;
};

class KeyCodeSetting : public SettingImpl<KeyCodeSettingPolicy>
{
public:
	KeyCodeSetting(CommandController& commandController,
	               const std::string& name, const std::string& description,
	               Keys::KeyCode initialValue);
};

} // namespace openmsx

#endif
