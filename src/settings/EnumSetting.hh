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
#include "InfoTopic.hh"
#include "InfoCommand.hh"
#include "CommandArgument.hh"
#include "StringOp.hh"

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
	typedef map<string, ValueType, StringOp::caseless> Map;

	// Implementation of Setting interface:
	virtual string getValueString() const;
	virtual void setValueString(const string& valueString);
	virtual void tabCompletion(vector<string>& tokens) const;
	void getPossibleValues(set<string>& result) const;

protected:
	EnumSettingBase(const string& name, const string& description,
	                const ValueType& initialValue, const Map& map_);
	EnumSettingBase(XMLElement& node, const string& description,
	                const ValueType& initialValue, const Map& map_);
	virtual ~EnumSettingBase();

private:
	void init();
	string getSummary() const;

	class EnumInfo : public InfoTopic {
	public:
		EnumInfo(EnumSettingBase& parent);
		virtual void execute(const vector<CommandArgument>& tokens,
		                     CommandArgument& result) const;
		virtual string help(const vector<string>& tokens) const;
	private:
		EnumSettingBase& parent;
	} enumInfo;

	Map enumMap;
};

NON_INHERITABLE_PRE2(EnumSetting, ValueType)
template <typename ValueType>
class EnumSetting : public EnumSettingBase<ValueType>,
                    NON_INHERITABLE(EnumSetting<ValueType>)
{
public:
	typedef typename EnumSettingBase<ValueType>::Map Map;

	EnumSetting(const string& name, const string& description,
		    const ValueType& initialValue, const Map& map);
	EnumSetting(XMLElement& node, const string& description,
		    const ValueType& initialValue, const Map& map);
	virtual ~EnumSetting();
};



// class EnumSetting

template<typename ValueType>
EnumSettingBase<ValueType>::EnumSettingBase(
	const string& name,
	const string& description,
	const ValueType& initialValue,
	const Map& map_)
	: Setting<ValueType>(name, description, initialValue)
	, enumInfo(*this), enumMap(map_)
{
	init();
}

template<typename ValueType>
EnumSettingBase<ValueType>::EnumSettingBase(
	XMLElement& node,
	const string& description,
	const ValueType& initialValue,
	const Map& map_)
	: Setting<ValueType>(node, description, initialValue)
	, enumInfo(*this), enumMap(map_)
{
	init();
}

template<typename ValueType>
void EnumSettingBase<ValueType>::init()
{
	// GCC 3.4-pre complains if "this" is not explicit here.
	this->type = getSummary();
	InfoCommand::instance().registerTopic(this->getName(), &enumInfo);
}

template<typename ValueType>
EnumSettingBase<ValueType>::~EnumSettingBase()
{
	// GCC 3.4-pre complains if "this" is not explicit here.
	InfoCommand::instance().unregisterTopic(this->getName(), &enumInfo);
}

template<typename ValueType>
string EnumSettingBase<ValueType>::getValueString() const
{
	for (typename Map::const_iterator it = enumMap.begin();
	     it != enumMap.end() ; ++it) {
		// GCC 3.4-pre complains if "this" is not explicit here.
		if (it->second == this->getValue()) {
			return it->first;
		}
	}
	assert(false);
	return "";	// avoid warning
}

template<typename ValueType>
void EnumSettingBase<ValueType>::setValueString(const string& valueString)
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

template<typename ValueType>
EnumSettingBase<ValueType>::EnumInfo::EnumInfo(EnumSettingBase& parent_)
	: parent(parent_)
{
}

template<typename ValueType>
void EnumSettingBase<ValueType>::EnumInfo::execute(
	const vector<CommandArgument>& /*tokens*/,
	CommandArgument& result) const
{
	set<string> values;
	parent.getPossibleValues(values);
	for (set<string>::const_iterator it = values.begin();
	     it != values.end(); ++it) {
		result.addListElement(*it);
	}
}

template<typename ValueType>
string EnumSettingBase<ValueType>::EnumInfo::help(
	const vector<string>& /*tokens*/) const
{
	return "Returns all possible values for this setting.";
}


// class EnumSetting

template<typename ValueType>
EnumSetting<ValueType>::EnumSetting(
	const string& name, const string& description,
	const ValueType& initialValue, const Map& map_)
	: EnumSettingBase<ValueType>(name, description, initialValue, map_)
{
	Setting<ValueType>::initSetting();
}

template<typename ValueType>
EnumSetting<ValueType>::EnumSetting(
	XMLElement& node, const string& description,
	const ValueType& initialValue, const Map& map_)
	: EnumSettingBase<ValueType>(node, description, initialValue, map_)
{
	Setting<ValueType>::initSetting();
}

template<typename ValueType>
EnumSetting<ValueType>::~EnumSetting()
{
	Setting<ValueType>::exitSetting();
}

} // namespace openmsx

#endif //__ENUMSETTING_HH__
