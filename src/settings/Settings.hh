// $Id$

#ifndef __SETTINGS_HH__
#define __SETTINGS_HH__

#include "SettingNode.hh"
#include "Command.hh"
#include "CommandController.hh"
#include <map>
#include <set>
#include <string>
#include <vector>

using std::map;
using std::set;
using std::string;
using std::vector;

namespace openmsx {

/*
TODO: Reduce code duplication.
It is possible to put a lot of the common code in templates:
- make OrderedSetting/OrdinalSetting/NumberSetting/ScalarSetting
  base class for IntegerSetting and FloatSetting
*/

/** Abstract base class for Settings.
  */
template <typename ValueType>
class Setting: public SettingLeafNode
{
public:
	/** Gets the current value of this setting.
	  */
	const ValueType& getValue() const { return value; }

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

	virtual ~Setting() { }

private:
	/** The current value of this setting.
	  * Private to force use of setValue().
	  */
	ValueType value;
	ValueType defaultValue;
};


/** A Setting with an integer value.
  */
class IntegerSetting: public Setting<int>
{
public:
	IntegerSetting(
		const string &name, const string &description,
		int initialValue, int minValue, int maxValue);

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(int minValue, int maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);
	virtual void setValue(const int &newValue);

protected:
	int minValue;
	int maxValue;
};

/** A Setting with a floating point value.
  */
class FloatSetting: public Setting<float>
{
public:
	FloatSetting(
		const string &name, const string &description,
		float initialValue, float minValue, float maxValue);

	/** Change the allowed range.
	  * @param minValue New minimal value (inclusive).
	  * @param maxValue New maximal value (inclusive).
	  */
	void setRange(float minValue, float maxValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);
	virtual void setValue(const float &newValue);

protected:
	float minValue;
	float maxValue;
};

/** Associates pairs of an a integer and a string.
  * Used internally by EnumSetting to associate enum constants with strings.
  */
class IntStringMap
{
public:
	typedef const map<string, int> BaseMap;

	IntStringMap(BaseMap *map);
	~IntStringMap();

	/** Gets the string associated with a given integer.
	  */
	const string &lookupInt(int n) const;

	/** Gets the string associated with a given string.
	  */
	int lookupString(const string &s) const;

	/** Get the list of all strings, separated by commas.
	  */
	string getSummary() const;

	/** Create a set containing all the strings in this association.
	  * The caller becomes the owner of the newly created set object.
	  */
	void getStringSet(set<string>& result) const;

private:
	typedef map<string, int>::const_iterator MapIterator;
	BaseMap *stringToInt;
};


/** A Setting with a string value out of a finite set.
  * The type parameter can be anything that can be converted from and to
  * an integer.
  */
template <typename ValueType>
class EnumSetting : public Setting<ValueType>
{
public:
	EnumSetting(
		const string &name, const string &description,
		const ValueType &initialValue,
		const map<string, ValueType> &map
	) : Setting<ValueType>(name, description, initialValue)
	  , intStringMap(convertMap(map))
	{
		// GCC 3.4-pre complains if superclass is not explicit here.
		SettingLeafNode::type = intStringMap.getSummary();
	}

	// Implementation of Setting interface:
	virtual string getValueString() const {
		// GCC 3.4-pre complains if superclass is not explicit here.
		return intStringMap.lookupInt(
			static_cast<int>(Setting<ValueType>::getValue())
			);
	}

	virtual void setValueString(const string &valueString) {
		setValue(
			static_cast<ValueType>(intStringMap.lookupString(valueString))
			);
	}

	virtual void tabCompletion(vector<string> &tokens) const {
		set<string> stringSet;
		intStringMap.getStringSet(stringSet);
		CommandController::completeString(tokens, stringSet);
	}
	
	void getPossibleValues(set<string>& result) const {
		intStringMap.getStringSet(result);
	}

protected:
	IntStringMap intStringMap;

private:
	static map<string, int> *convertMap(
		const map<string, ValueType> &map_
	) {
		map<string, int> *ret = new map<string, int>();
		typename map<string, ValueType>::const_iterator it;
		for (it = map_.begin(); it != map_.end(); ++it) {
			(*ret)[it->first] = static_cast<int>(it->second);
		}
		return ret;
	}
};

/** A Setting with a boolean value.
  */
class BooleanSetting : public EnumSetting<bool>
{
public:
	BooleanSetting(
		const string &name, const string &description,
		bool initialValue = false);

private:
	static const map<string, bool> &getMap();
};


/** A Setting with a string value.
  */
class StringSetting : public Setting<string>
{
public:
	StringSetting(const string &name, const string &description,
		const string &initialValue);

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string &valueString);

};

class File;
class FileContext;

/** A Setting with a filename value.
  */
class FilenameSetting : public StringSetting
{
public:
	FilenameSetting(const string &name,
		const string &description,
		const string &initialValue);

	// Implementation of Setting interface:
	virtual void setValue(const string &newValue);
	virtual void tabCompletion(vector<string> &tokens) const;

protected:
	/** Used by subclass to check a new file name and/or contents.
	  * The default implementation accepts any file.
	  * @param filename The file path to an existing file.
	  * @return true to accept this file name; false to reject it.
	  */
	virtual bool checkFile(const string &filename);
};

} // namespace openmsx

#endif //__SETTINGS_HH__
