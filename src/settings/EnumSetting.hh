// $Id$

#ifndef __ENUMSETTING_HH__
#define __ENUMSETTING_HH__

#include "Setting.hh"
#include "CommandController.hh"
#include <map>
#include <set>

using std::map;
using std::set;


namespace openmsx {

/** Associates pairs of an a integer and a string.
  * Used internally by EnumSetting to associate enum constants with strings.
  */
class IntStringMap
{
public:
	typedef const map<string, int> BaseMap;

	IntStringMap(BaseMap* map);
	~IntStringMap();

	/** Gets the string associated with a given integer.
	  */
	const string& lookupInt(int n) const;

	/** Gets the string associated with a given string.
	  */
	int lookupString(const string& s) const;

	/** Get the list of all strings, separated by commas.
	  */
	string getSummary() const;

	/** Create a set containing all the strings in this association.
	  * The caller becomes the owner of the newly created set object.
	  */
	void getStringSet(set<string>& result) const;

private:
	typedef map<string, int>::const_iterator MapIterator;
	BaseMap* stringToInt;
};


/** A Setting with a string value out of a finite set.
  * The type parameter can be anything that can be converted from and to
  * an integer.
  */
template <typename ValueType>
class EnumSetting : public Setting<ValueType>
{
public:
	EnumSetting(const string& name, const string& description,
		    const ValueType& initialValue,
		    const map<string, ValueType>& map)
		: Setting<ValueType>(name, description, initialValue),
	          intStringMap(convertMap(map))
	{
		// GCC 3.4-pre complains if superclass is not explicit here.
		SettingLeafNode::type = intStringMap.getSummary();
	}

	EnumSetting(const string& name, const string& description,
		    const ValueType& initialValue,
		    const ValueType& defaultValue,
		    const map<string, ValueType>& map)
		: Setting<ValueType>(name, description, initialValue, defaultValue),
	          intStringMap(convertMap(map))
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

	virtual void setValueString(const string& valueString) {
		setValue(
			static_cast<ValueType>(intStringMap.lookupString(valueString))
			);
	}

	virtual void tabCompletion(vector<string>& tokens) const {
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
	static map<string, int>* convertMap(const map<string, ValueType>& map_);
};

template <typename ValueType>
map<string, int>* EnumSetting<ValueType>::convertMap(
	const map<string, ValueType>& map_
) {
	map<string, int>* ret = new map<string, int>();
	typename map<string, ValueType>::const_iterator it;
	for (it = map_.begin(); it != map_.end(); ++it) {
		(*ret)[it->first] = static_cast<int>(it->second);
	}
	return ret;
}

} // namespace openmsx

#endif //__ENUMSETTING_HH__
