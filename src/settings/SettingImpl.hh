// $Id$

#ifndef __SETTING_HH__
#define __SETTING_HH__

#include <cassert>
#include "SettingsManager.hh"
#include "Setting.hh"
#include "SettingsConfig.hh"
#include "XMLElementListener.hh"
#include "MSXException.hh"

namespace openmsx {

template<typename T> class SettingChecker;

template <typename POLICY>
class SettingImpl : public Setting, public POLICY, private XMLElementListener
{
public:
	typedef POLICY Policy;
	typedef typename POLICY::Type Type;

	SettingImpl(const std::string& name, const std::string& description,
	            const Type& initialValue, SaveSetting save);

	template <typename T1, typename T2>
	SettingImpl(const std::string& name, const std::string& description,
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
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

protected:
	XMLElement* xmlNode;

private:
	void init(SaveSetting save);
	void setValue2(Type newValue, bool check);
	void setValueString2(const std::string& valueString, bool check);

	// XMLElementListener
	virtual void updateData(const XMLElement& element);

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
	const std::string& name, const std::string& description,
	const Type& initialValue, SaveSetting save)
	: Setting(name, description)
	, xmlNode(NULL)
	, value(initialValue), defaultValue(initialValue)
	, checker(NULL)
{
	init(save);
}

template<typename POLICY>
template<typename T1, typename T2>
SettingImpl<POLICY>::SettingImpl(
	const std::string& name, const std::string& description,
	const Type& initialValue, SaveSetting save, T1 extra1, T2 extra2)
	: Setting(name, description)
	, POLICY(extra1, extra2)
	, xmlNode(NULL)
	, value(initialValue), defaultValue(initialValue)
	, checker(NULL)
{
	init(save);
}

template<typename POLICY>
void SettingImpl<POLICY>::init(SaveSetting save)
{
	if (save == SAVE) {
		XMLElement& config = SettingsConfig::instance().
			getCreateChild("settings");
		xmlNode = &config.getCreateChildWithAttribute(
			"setting", "id", getName(), getValueString());
		updateData(*xmlNode);
		xmlNode->addListener(*this);
	}
	SettingsManager::instance().registerSetting(*this);
}

template<typename POLICY>
SettingImpl<POLICY>::~SettingImpl()
{
	SettingsManager::instance().unregisterSetting(*this);
	if (xmlNode) {
		xmlNode->removeListener(*this);
	}
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
		if (xmlNode) {
			xmlNode->setData(getValueString());
		}
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
void SettingImpl<POLICY>::tabCompletion(std::vector<std::string>& tokens) const
{
	POLICY::tabCompletion(tokens);
}

template<typename POLICY>
void SettingImpl<POLICY>::updateData(const XMLElement& element)
{
	assert(&element == xmlNode);
	try {
		setValueString2(xmlNode->getData(), false);
	} catch (MSXException& e) {
		// saved value no longer valid, just keep default
	}
}

} // namespace openmsx

#endif //__SETTING_HH__
