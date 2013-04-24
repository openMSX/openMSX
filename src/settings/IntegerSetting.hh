#ifndef INTEGERSETTING_HH
#define INTEGERSETTING_HH

#include "SettingRangePolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class IntegerSettingPolicy : public SettingRangePolicy<int>
{
protected:
	IntegerSettingPolicy(int minValue, int maxValue);
	std::string toString(int value) const;
	int fromString(const std::string& str) const;
	string_ref getTypeString() const;
};

/** A Setting with an integer value.
  */
class IntegerSetting : public SettingImpl<IntegerSettingPolicy>
{
public:
	IntegerSetting(CommandController& commandController,
	               string_ref name, string_ref description,
	               int initialValue, int minValue, int maxValue);
};

} // namespace openmsx

#endif
