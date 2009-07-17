// $Id$

#ifndef BOOLEANSETTING_HH
#define BOOLEANSETTING_HH

#include "SettingPolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class BooleanSettingPolicy : public SettingPolicy<bool>
{
protected:
	explicit BooleanSettingPolicy(CommandController& commandController);
	std::string toString(bool value) const;
	bool fromString(const std::string& str) const;
	std::string getTypeString() const;
	void tabCompletion(std::vector<std::string>& tokens) const;
};

class BooleanSetting : public SettingImpl<BooleanSettingPolicy>
{
public:
	BooleanSetting(CommandController& commandController,
	               const std::string& name, const std::string& description,
	               bool initialValue, SaveSetting save = SAVE);
	BooleanSetting(CommandController& commandController,
	               const char* name, const char* description,
	               bool initialValue, SaveSetting save = SAVE);
};

} // namespace openmsx

#endif
