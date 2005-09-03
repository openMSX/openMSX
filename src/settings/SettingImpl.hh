// $Id$

#ifndef SETTING_HH
#define SETTING_HH

#include <cassert>
#include "SettingsManager.hh"
#include "Setting.hh"
#include "SettingsConfig.hh"
#include "MSXException.hh"
#include "CommandController.hh"

namespace openmsx {

template<typename T> class SettingChecker;

template <typename POLICY>
class SettingImpl : public Setting, public POLICY
{
public:
	typedef POLICY Policy;
	typedef typename POLICY::Type Type;

	SettingImpl(CommandController& commandController,
	            const std::string& name, const std::string& description,
	            const Type& initialValue, SaveSetting save);

	template <typename T1, typename T2>
	SettingImpl(CommandController& commandController,
	            const std::string& name, const std::string& description,
	            const Type& initialValue, SaveSetting save,
	            T1 extra1, T2 extra2);

	virtual ~SettingImpl();

	/** Gets the current value of this setting.
	  */
	Type getValue() const;

	/** Changes the current value of this setting.
	  * If the given value is invalid, it will be mapped to the closest
	  * valid value.
	  * If that is not appropriate or possible,
	  * the value of this setting will not change.
	  * @param newValue The new value.
	  */
	void setValue(Type newValue);

	/** Get the default value of this setting
	  */
	const Type& getDefaultValue() const;

	/** Set a new default value
	  */
	void setDefaultValue(const Type& value);

	/**
	 */
	void setChecker(SettingChecker<POLICY>* checker);

	// virtual methods from Setting class
	virtual std::string getValueString() const;
	virtual void setValueString(const std::string& valueString);
	virtual void restoreDefault();
	virtual bool hasDefaultValue() const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	void init();
	void setValue2(Type newValue, bool check);
	void setValueString2(const std::string& valueString, bool check);

	Type value;
	Type defaultValue;
	SettingChecker<POLICY>* checker;
};

template<typename POLICY> class SettingChecker
{
public:
	virtual void check(SettingImpl<POLICY>& setting,
	                   typename SettingImpl<POLICY>::Type& value) = 0;
protected:
	virtual ~SettingChecker() {}
};



template<typename POLICY>
SettingImpl<POLICY>::SettingImpl(
	CommandController& commandController,
	const std::string& name, const std::string& description,
	const Type& initialValue, SaveSetting save)
	: Setting(commandController, name, description, save)
	, POLICY(commandController)
	, value(initialValue), defaultValue(initialValue)
	, checker(NULL)
{
	init();
}

template<typename POLICY>
template<typename T1, typename T2>
SettingImpl<POLICY>::SettingImpl(
	CommandController& commandController,
	const std::string& name, const std::string& description,
	const Type& initialValue, SaveSetting save, T1 extra1, T2 extra2)
	: Setting(commandController, name, description, save)
	, POLICY(commandController, extra1, extra2)
	, value(initialValue), defaultValue(initialValue)
	, checker(NULL)
{
	init();
}

template<typename POLICY>
void SettingImpl<POLICY>::init()
{
	SettingsConfig& settingsConfig = Setting::getCommandController().getSettingsConfig();
	if (needLoadSave()) {
		const XMLElement* config = settingsConfig.findChild("settings");
		if (config) {
			const XMLElement* elem = config->findChildWithAttribute(
				"setting", "id", getName());
			if (elem) {
				try {
					setValueString2(elem->getData(), false);
				} catch (MSXException& e) {
					// saved value no longer valid, just keep default
				}
			}
		}
	}
	settingsConfig.getSettingsManager().registerSetting(*this);
}

template<typename POLICY>
SettingImpl<POLICY>::~SettingImpl()
{
	SettingsConfig& settingsConfig = Setting::getCommandController().getSettingsConfig();
	sync(settingsConfig);
	settingsConfig.getSettingsManager().unregisterSetting(*this);
}

template<typename POLICY>
typename SettingImpl<POLICY>::Type SettingImpl<POLICY>::getValue() const
{
	return POLICY::checkGetValue(value);
}

template<typename POLICY>
void SettingImpl<POLICY>::setValue(Type newValue)
{
	setValue2(newValue, true);
}

template<typename POLICY>
void SettingImpl<POLICY>::setValue2(Type newValue, bool check)
{
	if (check) {
		POLICY::checkSetValue(newValue);
	}
	if (checker) {
		checker->check(*this, newValue);
	}
	if (newValue != value) {
		value = newValue;
		notify();
	}
}

template<typename POLICY>
const typename SettingImpl<POLICY>::Type& SettingImpl<POLICY>::getDefaultValue() const
{
	return defaultValue;
}

template<typename POLICY>
void SettingImpl<POLICY>::setDefaultValue(const Type& value)
{
	defaultValue = value;
}

template<typename POLICY>
void SettingImpl<POLICY>::setChecker(SettingChecker<POLICY>* checker_)
{
	checker = checker_;
	if (checker) {
		setValue(getValue());
	}
}

template<typename POLICY>
std::string SettingImpl<POLICY>::getValueString() const
{
	return POLICY::toString(getValue());
}

template<typename POLICY>
void SettingImpl<POLICY>::setValueString(const std::string& valueString)
{
	setValueString2(valueString, true);
}

template<typename POLICY>
void SettingImpl<POLICY>::setValueString2(const std::string& valueString, bool check)
{
	setValue2(POLICY::fromString(valueString), check);
}

template<typename POLICY>
void SettingImpl<POLICY>::restoreDefault()
{
	setValue(getDefaultValue());
}

template<typename POLICY>
bool SettingImpl<POLICY>::hasDefaultValue() const
{
	return getValue() == getDefaultValue();
}

template<typename POLICY>
void SettingImpl<POLICY>::tabCompletion(std::vector<std::string>& tokens) const
{
	POLICY::tabCompletion(tokens);
}

} // namespace openmsx

#endif
