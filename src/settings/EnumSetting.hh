// $Id$

#ifndef __ENUMSETTING_HH__
#define __ENUMSETTING_HH__

#include <map>
#include <set>
#include <sstream>
#include <cassert>
#include "Setting.hh"
#include "CommandController.hh"
#include "NonInheritable.hh"
#include "SettingsManager.hh"

using std::map;
using std::set;
using std::ostringstream;

namespace openmsx {

/** A Setting with a string value out of a finite set.
  * The type parameter can be anything that can be converted from and to
  * an integer.
  */
template <typename ValueType>
class EnumSettingBase : public Setting<ValueType>
{
public:
	struct caseltstr {
		bool operator()(const string& s1, const string& s2) const {
			return strcasecmp(s1.c_str(), s2.c_str()) < 0;
		}
	};

	typedef map<string, ValueType, caseltstr> Map;

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string& valueString)
		throw(CommandException);
	virtual void tabCompletion(vector<string>& tokens) const;
	void getPossibleValues(set<string>& result) const;

protected:
	EnumSettingBase(const string& name, const string& description,
	                const ValueType& initialValue,
	                const Map& map_);
	EnumSettingBase(const string& name, const string& description,
	                const ValueType& initialValue,
	                const ValueType& defaultValue,
	                const Map& map_);

private:
	string getSummary() const;

	Map enumMap;
};


template <typename ValueType>
class EnumSetting : public EnumSettingBase<ValueType>,
                    NON_INHERITABLE(EnumSetting<ValueType>)
{
public:
	typedef typename EnumSettingBase<ValueType>::Map Map;

	EnumSetting(const string& name, const string& description,
		    const ValueType& initialValue,
		    const Map& map_);
	EnumSetting(const string& name, const string& description,
		    const ValueType& initialValue,
		    const ValueType& defaultValue,
		    const Map& map_);
	virtual ~EnumSetting();
};



// class EnumSetting

template<typename ValueType>
EnumSettingBase<ValueType>::EnumSettingBase(
	const string& name,
	const string& description,
	const ValueType& initialValue,
	const Map& map_)
	: Setting<ValueType>(name, description, initialValue),
	  enumMap(map_)
{
	// GCC 3.4-pre complains if superclass is not explicit here.
	SettingLeafNode::type = getSummary();
}

template<typename ValueType>
EnumSettingBase<ValueType>::EnumSettingBase(
	const string& name,
	const string& description,
	const ValueType& initialValue,
	const ValueType& defaultValue,
	const Map& map_)
	: Setting<ValueType>(name, description, initialValue, defaultValue),
	  enumMap(map_)
{
	// GCC 3.4-pre complains if superclass is not explicit here.
	SettingLeafNode::type = getSummary();
}

template<typename ValueType>
string EnumSettingBase<ValueType>::getValueString() const
{
	for (typename Map::const_iterator it = enumMap.begin();
	     it != enumMap.end() ; ++it) {
		if (it->second == getValue()) {
			return it->first;
		}
	}
	assert(false);
	return "";	// avoid warning
}

template<typename ValueType>
void EnumSettingBase<ValueType>::setValueString(const string& valueString)
	throw(CommandException)
{
	typename Map::const_iterator it = enumMap.find(valueString);
	if (it == enumMap.end()) {
		throw CommandException("set: " + valueString +
				       ": not a valid value");
	}
	setValue(it->second);
}

template<typename ValueType>
void EnumSettingBase<ValueType>::tabCompletion(vector<string>& tokens) const
{
	set<string> stringSet;
	getPossibleValues(stringSet);
	CommandController::completeString(tokens, stringSet, false); // case insensitive
}

template<typename ValueType>
void EnumSettingBase<ValueType>::getPossibleValues(set<string>& result) const
{
	for (typename Map::const_iterator it = enumMap.begin();
	     it != enumMap.end(); ++it) {
		result.insert(it->first);
	}
}

template<typename ValueType>
string EnumSettingBase<ValueType>::getSummary() const
{
	ostringstream out;
	typename Map::const_iterator it = enumMap.begin();
	out << it->first;
	for (++it; it != enumMap.end(); ++it) {
		out << ", " << it->first;
	}
	return out.str();
}


// class EnumSetting

template<typename ValueType>
EnumSetting<ValueType>::EnumSetting(
	const string& name, const string& description,
	const ValueType& initialValue, const Map& map_)
	: EnumSettingBase<ValueType>(name, description, initialValue, map_)
{
	SettingsManager::instance().registerSetting(*this);
}

template<typename ValueType>
EnumSetting<ValueType>::EnumSetting(
	const string& name, const string& description,
	const ValueType& initialValue, const ValueType& defaultValue,
	const Map& map_)
	: EnumSettingBase<ValueType>(
		name, description, initialValue, defaultValue, map_)
{
	SettingsManager::instance().registerSetting(*this);
}

template<typename ValueType>
EnumSetting<ValueType>::~EnumSetting()
{
	SettingsManager::instance().unregisterSetting(*this);
}

} // namespace openmsx

#endif //__ENUMSETTING_HH__
