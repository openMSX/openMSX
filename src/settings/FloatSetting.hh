// $Id$

#ifndef __FLOATSETTING_HH__
#define __FLOATSETTING_HH__

#include "SettingPolicy.hh"
#include "SettingImpl.hh"

namespace openmsx {

class FloatSettingPolicy : public SettingRangePolicy<double>
{
protected:
	FloatSettingPolicy(double minValue, double maxValue);
	std::string toString(double value) const;
	double fromString(const std::string& str) const;
};

/** A Setting with a floating point value.
  */
class FloatSetting : public SettingImpl<FloatSettingPolicy>
{
public:
	FloatSetting(const string& name, const string& description,
	             double initialValue, double minValue, double maxValue);
};

} // namespace openmsx

#endif //__FLOATSETTING_HH__
