// $Id$

#ifndef SETTINGRANGEPOLICY_HH
#define SETTINGRANGEPOLICY_HH

#include "SettingPolicy.hh"
#include "TclObject.hh"
#include <algorithm>

namespace openmsx {

template <typename T> class SettingRangePolicy : public SettingPolicy<T>
{
protected:
	SettingRangePolicy(CommandController& commandController,
	                   T minValue_, T maxValue_)
		: SettingPolicy<T>(commandController)
		, minValue(minValue_), maxValue(maxValue_)
	{
	}

	void checkSetValue(T& value)
	{
		value = std::min(std::max(value, minValue), maxValue);
	}

	T getMinValue() const { return minValue; }
	T getMaxValue() const { return maxValue; }

	void additionalInfo(TclObject& result) const
	{
		TclObject range(result.getInterpreter());
		range.addListElement(getMinValue());
		range.addListElement(getMaxValue());
		result.addListElement(range);
	}

private:
	T minValue;
	T maxValue;
};

} // namespace openmsx

#endif

