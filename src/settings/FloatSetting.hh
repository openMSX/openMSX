#ifndef FLOATSETTING_HH
#define FLOATSETTING_HH

#include "SettingRangePolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class FloatSettingPolicy : public SettingRangePolicy<double>
{
protected:
	FloatSettingPolicy(double minValue, double maxValue);
	std::string toString(double value) const;
	double fromString(const std::string& str) const;
	string_ref getTypeString() const;
};

/** A Setting with a floating point value.
  */
class FloatSetting : public SettingImpl<FloatSettingPolicy>
{
public:
	FloatSetting(CommandController& commandController,
	             string_ref name, string_ref description,
	             double initialValue, double minValue, double maxValue);
};

} // namespace openmsx

#endif
