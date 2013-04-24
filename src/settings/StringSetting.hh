#ifndef STRINGSETTING_HH
#define STRINGSETTING_HH

#include "SettingPolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class StringSettingPolicy : public SettingPolicy<std::string>
{
protected:
	explicit StringSettingPolicy();
	const std::string& toString(const std::string& value) const;
	const std::string& fromString(const std::string& str) const;
	string_ref getTypeString() const;
};

class StringSetting : public SettingImpl<StringSettingPolicy>
{
public:
	StringSetting(CommandController& commandController,
	              string_ref name, string_ref description,
	              string_ref initialValue, SaveSetting save = SAVE);
};

} // namespace openmsx

#endif
