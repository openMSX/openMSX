// $Id$

#ifndef __SETTING_HH__
#define __SETTING_HH__

#include "SettingNode.hh"
#include <string>

using std::string;


namespace openmsx {

/** Abstract base class for Settings.
  */
template <typename ValueType>
class Setting: public SettingLeafNode
{
// All implementation is located in this header file to avoid template
// instantiation problems.
public:
	/** Gets the current value of this setting.
	  */
	const ValueType& getValue() const { return value; }

	/** Get the default value of this setting
	  */
	const ValueType& getDefaultValue() const { return defaultValue; }

	/** Changes the current value of this setting.
	  * If the given value is invalid, it will be mapped to the closest
	  * valid value.
	  * If that is not appropriate or possible,
	  * the value of this setting will not change.
	  * The implementation of setValueString should use this method
	  * to set the new value.
	  * @param newValue The new value.
	  */
	virtual void setValue(const ValueType& newValue) {
		if (newValue != value) {
			value = newValue;
			notify();
		}
	}

	virtual void restoreDefault() {
		setValue(defaultValue);
	}

protected:
	Setting(const string& name, const string& description,
		const ValueType& initialValue)
		: SettingLeafNode(name, description),
	          value(initialValue), defaultValue(initialValue) { }
	Setting(const string& name, const string& description,
		const ValueType& initialValue, const ValueType& defaultValue_)
		: SettingLeafNode(name, description),
	          value(initialValue), defaultValue(defaultValue_) { }

private:
	/** The current value of this setting.
	  * Private to force use of setValue().
	  */
	ValueType value;
	ValueType defaultValue;
};

} // namespace openmsx

#endif //__SETTING_HH__
