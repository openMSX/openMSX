// $Id$

#ifndef SETTINGPOLICY_HH
#define SETTINGPOLICY_HH

#include <string>
#include <vector>

namespace openmsx {

class CommandController;
class TclObject;

template <typename T> class SettingPolicy
{
protected:
	typedef T Type;

	explicit SettingPolicy(CommandController& commandController);

	void checkSetValue(T& value) const;
	T checkGetValue(T value) const { return value; }
	// std::string toString(T value) const;
	// T fromString(const std::string& str) const;
	void tabCompletion(std::vector<std::string>& tokens) const;
	void additionalInfo(TclObject& result) const;

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

template <typename T>
void SettingPolicy<T>::additionalInfo(TclObject& /*result*/) const
{
}

} // namespace openmsx

#endif

