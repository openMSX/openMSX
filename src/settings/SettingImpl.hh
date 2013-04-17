// $Id$

#ifndef SETTINGIMPL_HH
#define SETTINGIMPL_HH

#include "Setting.hh"

namespace openmsx {

template<typename T> class SettingChecker;

// non-templatized base class for SettingImpl
class SettingImplBase : public Setting
{
protected:
	SettingImplBase(CommandController& commandController,
	                string_ref name, string_ref description,
	                SaveSetting save);
	void init();
	void destroy();
	void syncProxy();

	virtual void setValueString2(const std::string& valueString, bool check) = 0;
};

template <typename POLICY>
class SettingImpl : public SettingImplBase, public POLICY
{
public:
	typedef POLICY Policy;
	typedef typename POLICY::Type Type;

	SettingImpl(CommandController& commandController,
	            string_ref name, string_ref description,
	            const Type& initialValue, SaveSetting save);

	template <typename T1>
	SettingImpl(CommandController& commandController,
	            string_ref name, string_ref description,
	            const Type& initialValue, SaveSetting save,
	            T1 extra1);

	template <typename T1, typename T2>
	SettingImpl(CommandController& commandController,
	            string_ref name, string_ref description,
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
	void changeValue(Type newValue);

	/** Get the default value of this setting
	  */
	Type getDefaultValue() const;

	/** Set a new default value
	  */
	void setRestoreValue(const Type& value);

	/**
	 */
	void setChecker(SettingChecker<POLICY>* checker, bool checkNow = true);

	// virtual methods from Setting class
	virtual string_ref getTypeString() const;
	virtual std::string getValueString() const;
	virtual std::string getDefaultValueString() const;
	virtual std::string getRestoreValueString() const;
	virtual void setValueStringDirect(const std::string& valueString);
	virtual void restoreDefault();
	virtual bool hasDefaultValue() const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
	virtual void additionalInfo(TclObject& result) const;

private:
	void setValue2(Type newValue, bool check);
	virtual void setValueString2(const std::string& valueString, bool check);

	SettingChecker<POLICY>* checker;
	Type value;
	const Type defaultValue;
	Type restoreValue;
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
	string_ref name, string_ref description,
	const Type& initialValue, SaveSetting save)
	: SettingImplBase(commandController, name, description, save)
	, POLICY()
	, checker(nullptr)
	, value(initialValue), defaultValue(initialValue)
	, restoreValue(initialValue)
{
	init();
}

template<typename POLICY>
template<typename T1>
SettingImpl<POLICY>::SettingImpl(
	CommandController& commandController,
	string_ref name, string_ref description,
	const Type& initialValue, SaveSetting save, T1 extra1)
	: SettingImplBase(commandController, name, description, save)
	, POLICY(extra1)
	, checker(nullptr)
	, value(initialValue), defaultValue(initialValue)
	, restoreValue(initialValue)
{
	init();
}

template<typename POLICY>
template<typename T1, typename T2>
SettingImpl<POLICY>::SettingImpl(
	CommandController& commandController,
	string_ref name, string_ref description,
	const Type& initialValue, SaveSetting save, T1 extra1, T2 extra2)
	: SettingImplBase(commandController, name, description, save)
	, POLICY(extra1, extra2)
	, checker(nullptr)
	, value(initialValue), defaultValue(initialValue)
	, restoreValue(initialValue)
{
	init();
}

template<typename POLICY>
SettingImpl<POLICY>::~SettingImpl()
{
	destroy();
}

template<typename POLICY>
typename SettingImpl<POLICY>::Type SettingImpl<POLICY>::getValue() const
{
	return POLICY::checkGetValue(value);
}

template<typename POLICY>
void SettingImpl<POLICY>::changeValue(Type newValue)
{
	changeValueString(POLICY::toString(newValue));
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
	syncProxy();
}

template<typename POLICY>
typename SettingImpl<POLICY>::Type SettingImpl<POLICY>::getDefaultValue() const
{
	return POLICY::checkGetValue(defaultValue);
}

template<typename POLICY>
void SettingImpl<POLICY>::setRestoreValue(const Type& value)
{
	restoreValue = value;
}

template<typename POLICY>
void SettingImpl<POLICY>::setChecker(SettingChecker<POLICY>* checker_, bool checkNow)
{
	checker = checker_;
	if (checker && checkNow) {
		setValue2(getValue(), true);
	}
}

template<typename POLICY>
string_ref SettingImpl<POLICY>::getTypeString() const
{
	return POLICY::getTypeString();
}

template<typename POLICY>
std::string SettingImpl<POLICY>::getValueString() const
{
	return POLICY::toString(getValue());
}

template<typename POLICY>
std::string SettingImpl<POLICY>::getDefaultValueString() const
{
	return POLICY::toString(getDefaultValue());
}

template<typename POLICY>
std::string SettingImpl<POLICY>::getRestoreValueString() const
{
	return POLICY::toString(POLICY::checkGetValue(restoreValue));
}

template<typename POLICY>
void SettingImpl<POLICY>::setValueStringDirect(const std::string& valueString)
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
	changeValue(restoreValue);
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

template<typename POLICY>
void SettingImpl<POLICY>::additionalInfo(TclObject& result) const
{
	POLICY::additionalInfo(result);
}

} // namespace openmsx

#endif
