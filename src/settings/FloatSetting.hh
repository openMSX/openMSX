// $Id$

#ifndef FLOATSETTING_HH
#define FLOATSETTING_HH

#include "SettingRangePolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class FloatSettingPolicy : public SettingRangePolicy<double>
{
protected:
	FloatSettingPolicy(CommandController& commandController,
	                   double minValue, double maxValue);
	std::string toString(double value) const;
	double fromString(const std::string& str) const;
	std::string getTypeString() const;
};

/** A Setting with a floating point value.
  */
class FloatSetting : public SettingImpl<FloatSettingPolicy>
{
public:
	FloatSetting(CommandController& commandController,
	             const std::string& name, const std::string& description,
	             double initialValue, double minValue, double maxValue);
};

} // namespace openmsx

#endif
