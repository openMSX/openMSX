#ifndef BOOLEANSETTING_HH
#define BOOLEANSETTING_HH

#include "SettingPolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class BooleanSettingPolicy : public SettingPolicy<bool>
{
protected:
	explicit BooleanSettingPolicy();
	std::string toString(bool value) const;
	bool fromString(const std::string& str) const;
	string_ref getTypeString() const;
	void tabCompletion(std::vector<std::string>& tokens) const;
};

class BooleanSetting : public SettingImpl<BooleanSettingPolicy>
{
public:
	BooleanSetting(CommandController& commandController,
	               string_ref name, string_ref description,
	               bool initialValue, SaveSetting save = SAVE);
};

} // namespace openmsx

#endif
