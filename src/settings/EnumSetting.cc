// $Id$

#include "EnumSetting.hh"
#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"
#include <cassert>
#include <strings.h>

namespace openmsx {

int EnumSettingPolicyBase::fromStringBase(const std::string& str) const
{
	// An alternative we used in the past is to use StringOp::caseless
	// as the map comparator functor. This requires to #include StringOp.hh
	// in the header file. Because this header is included in many other
	// files, we prefer not to do that.
	// These maps are usually very small, so there is no disadvantage on
	// using a O(n) search here (instead of O(log n)).
	for (BaseMap::const_iterator it = baseMap.begin();
	     it != baseMap.end() ; ++it) {
		if (strcasecmp(str.c_str(), it->first.c_str()) == 0) {
			return it->second;
		}
	}
	throw CommandException("not a valid value: " + str);
}

std::string EnumSettingPolicyBase::toStringBase(int value) const
{
	for (BaseMap::const_iterator it = baseMap.begin();
	     it != baseMap.end() ; ++it) {
		if (it->second == value) {
			return it->first;
		}
	}
	assert(false);
	return ""; // avoid warning
}

void EnumSettingPolicyBase::getPossibleValues(std::set<std::string>& result) const
{
	for (BaseMap::const_iterator it = baseMap.begin();
	     it != baseMap.end(); ++it) {
		try {
			int value = it->second;
			checkSetValueBase(value);
			result.insert(it->first);
		} catch (MSXException& e) {
			// ignore
		}
	}
}

void EnumSettingPolicyBase::tabCompletionBase(std::vector<std::string>& tokens) const
{
	std::set<std::string> stringSet;
	getPossibleValues(stringSet);
	Completer::completeString(tokens, stringSet, false); // case insensitive
}

void EnumSettingPolicyBase::additionalInfoBase(TclObject& result) const
{
	TclObject valueList(result.getInterpreter());
	std::set<std::string> values;
	this->getPossibleValues(values);
	valueList.addListElements(values.begin(), values.end());
	result.addListElement(valueList);
}

} // namespace openmsx
