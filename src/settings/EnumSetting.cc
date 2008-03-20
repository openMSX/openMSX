// $Id$

#include "EnumSetting.hh"
#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"
#include <cassert>

namespace openmsx {

int EnumSettingPolicyBase::fromStringBase(const std::string& str) const
{
	BaseMap::const_iterator it = baseMap.find(str);
	if (it == baseMap.end()) {
		throw CommandException("not a valid value: " + str);
	}
	return it->second;
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
	return "";	// avoid warning
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
