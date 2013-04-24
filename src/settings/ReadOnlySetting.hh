#ifndef READONLYSETTING_HH
#define READONLYSETTING_HH

#include "SettingImpl.hh"
#include "MSXException.hh"

namespace openmsx {

template <typename Setting>
class ReadOnlySetting : public SettingChecker<typename Setting::Policy>
{
	typedef typename Setting::Policy Policy;
	typedef SettingImpl<Policy> Impl;
	typedef typename Impl::Type Type;
public:
	ReadOnlySetting(CommandController& commandController,
	                string_ref name, string_ref description,
	                Type initialValue);
	void setReadOnlyValue(Type value);
	Type getValue() const;

private:
	// SettingChecker
	virtual void check(Impl& setting, Type& value);

	Type newValue; // order is important
	Setting setting;
};


template <typename Setting>
ReadOnlySetting<Setting>::ReadOnlySetting(
		CommandController& commandController,
		string_ref name, string_ref description,
		Type initialValue)
	: newValue(initialValue)
	, setting(commandController, name, description, initialValue,
	          Setting::DONT_TRANSFER)
{
	setting.setChecker(this);
}

template <typename Setting>
void ReadOnlySetting<Setting>::setReadOnlyValue(Type value)
{
	newValue = value;
	setting.changeValue(value);
}

template <typename Setting>
typename ReadOnlySetting<Setting>::Type ReadOnlySetting<Setting>::getValue() const
{
	return setting.getValue();
}

template <typename Setting>
void ReadOnlySetting<Setting>::check(Impl& /*setting*/, Type& value)
{
	if (value != newValue) {
		throw MSXException("Read-only setting");
	}
}

} // namespace openmsx

#endif
