// $Id$

#ifndef __SETTING_HH__
#define __SETTING_HH__

#include "SettingsManager.hh"
#include "SettingNode.hh"
#include "xmlx.hh"
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
			if (xmlNode) {
				xmlNode->setData(getValueString());
			}
		}
	}

	virtual void restoreDefault() {
		setValue(defaultValue);
	}

protected:
	Setting(const string& name, const string& description,
		const ValueType& initialValue, XMLElement* node = NULL)
		: SettingLeafNode(name, description)
		, value(initialValue), defaultValue(initialValue)
		, xmlNode(node) { }
	Setting(const string& name, const string& description,
		const ValueType& initialValue, const ValueType& defaultValue_,
		XMLElement* node = NULL)
		: SettingLeafNode(name, description)
		, value(initialValue), defaultValue(defaultValue_)
		, xmlNode(node) { }

	/**
	 * This method must be called from the constructor of the child class
	 */
	void initSetting() {
		if (xmlNode) {
			setValueString(xmlNode->getData());
		}
		SettingsManager::instance().registerSetting(*this);
	}

	/**
	 * This method must be called from the destructor of the child class
	 */
	void exitSetting() {
		SettingsManager::instance().unregisterSetting(*this);
	}

private:
	/** The current value of this setting.
	  * Private to force use of setValue().
	  */
	ValueType value;
	ValueType defaultValue;
	XMLElement* xmlNode;
};

} // namespace openmsx

#endif //__SETTING_HH__
