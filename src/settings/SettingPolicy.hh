// $Id$

#ifndef _SETTING_POLICY_HH_
#define _SETTING_POLICY_HH_

#include <string>
#include <vector>
#include <algorithm>

namespace openmsx {

template <typename T> class SettingPolicy
{
protected:
	typedef T Type;

	void checkSetValue(T& value) const { }
	T checkGetValue(T value) const { return value; }
	// std::string toString(T value) const;
	// T fromString(const std::string& str) const;
	void tabCompletion(std::vector<std::string>& tokens) const { }
};

template <typename T> class SettingRangePolicy : public SettingPolicy<T>
{
protected:
	SettingRangePolicy(T minValue_, T maxValue_)
		: minValue(minValue_), maxValue(maxValue_)
	{
	}
	
	void checkSetValue(T& value)
	{
		value = std::min(std::max(value, minValue), maxValue);
	}

private:
	T minValue;
	T maxValue;
};

} // namespace openmsx

#endif //__INTEGERSETTING_HH__

