// $Id$

#ifndef __SETTING_HH__
#define __SETTING_HH__

#include "SettingsManager.hh"
#include "SettingNode.hh"
#include "SettingsConfig.hh"
#include "xmlx.hh"
#include <string>

using std::string;

namespace openmsx {

enum SaveSetting {
	SAVE_SETTING,
	DONT_SAVE_SETTING,
};

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

	/** Set a new default value
	  */
	void setDefaultValue(const ValueType& value) { defaultValue = value; }

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
		const ValueType& initialValue)
		: SettingLeafNode(name, description)
		, xmlNode(NULL)
		, value(initialValue), defaultValue(initialValue) { }

	/**
	 * This method must be called from the constructor of the child class
	 */
	void initSetting(SaveSetting save) {
		if (save == SAVE_SETTING) {
			XMLElement& config = SettingsConfig::instance().
				getCreateChild("settings");
			xmlNode = &config.getCreateChildWithAttribute(
				"setting", "id", getName(), getValueString());
			try {
				setValueString(xmlNode->getData());
			} catch (CommandException& e) {
				// saved value no longer valid, just keep default
			}
		}
		SettingsManager::instance().registerSetting(*this);
	}

	/**
	 * This method must be called from the destructor of the child class
	 */
	void exitSetting() {
		SettingsManager::instance().unregisterSetting(*this);
	}
	
	XMLElement* xmlNode;

private:
	/** The current value of this setting.
	  * Private to force use of setValue().
	  */
	ValueType value;
	ValueType defaultValue;
};

} // namespace openmsx

#endif //__SETTING_HH__
