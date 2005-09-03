// $Id$

#ifndef SETTING_POLICY_HH
#define SETTING_POLICY_HH

#include <string>
#include <vector>
#include <algorithm>

namespace openmsx {

class CommandController;

template <typename T> class SettingPolicy
{
protected:
	typedef T Type;

	SettingPolicy(CommandController& commandController);

	void checkSetValue(T& value) const;
	T checkGetValue(T value) const { return value; }
	// std::string toString(T value) const;
	// T fromString(const std::string& str) const;
	void tabCompletion(std::vector<std::string>& tokens) const;
	
	CommandController& getCommandController() const { return commandController; }

private:
	CommandController& commandController;
};

template <typename T>
SettingPolicy<T>::SettingPolicy(CommandController& commandController_)
	: commandController(commandController_)
{
}

template <typename T>
void SettingPolicy<T>::checkSetValue(T& /*value*/) const
{
}

template <typename T>
void SettingPolicy<T>::tabCompletion(std::vector<std::string>& /*tokens*/) const
{
}

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

private:
	T minValue;
	T maxValue;
};

} // namespace openmsx

#endif

