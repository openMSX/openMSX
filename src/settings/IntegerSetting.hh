// $Id$

#ifndef INTEGERSETTING_HH
#define INTEGERSETTING_HH

#include "SettingPolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class IntegerSettingPolicy : public SettingRangePolicy<int>
{
protected:
	IntegerSettingPolicy(CommandController& commandController,
	                     int minValue, int maxValue);
	std::string toString(int value) const;
	int fromString(const std::string& str) const;
	std::string getTypeString() const;
};

/** A Setting with an integer value.
  */
class IntegerSetting : public SettingImpl<IntegerSettingPolicy>
{
public:
	IntegerSetting(CommandController& commandController,
	               const std::string& name, const std::string& description,
	               int initialValue, int minValue, int maxValue);
};

} // namespace openmsx

#endif

