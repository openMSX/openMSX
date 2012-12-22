// $Id$

#include "EnumSetting.hh"
#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"
#include "stringsp.hh"
#include "unreachable.hh"

namespace openmsx {

int EnumSettingPolicyBase::fromStringBase(const std::string& str) const
{
	// An alternative we used in the past is to use StringOp::caseless
	// as the map comparator functor. This requires to #include StringOp.hh
	// in the header file. Because this header is included in many other
	// files, we prefer not to do that.
	// These maps are usually very small, so there is no disadvantage on
	// using a O(n) search here (instead of O(log n)).
	for (auto& p : baseMap) {
		if (strcasecmp(str.c_str(), p.first.c_str()) == 0) {
			return p.second;
		}
	}
	throw CommandException("not a valid value: " + str);
}

std::string EnumSettingPolicyBase::toStringBase(int value) const
{
	for (auto& p : baseMap) {
		if (p.second == value) {
			return p.first;
		}
	}
	UNREACHABLE; return "";
}

std::vector<std::string> EnumSettingPolicyBase::getPossibleValues() const
{
	std::vector<std::string> result;
	for (auto& p : baseMap) {
		try {
			int value = p.second;
			checkSetValueBase(value);
			result.push_back(p.first);
		} catch (MSXException&) {
			// ignore
		}
	}
	return result;
}

void EnumSettingPolicyBase::tabCompletionBase(std::vector<std::string>& tokens) const
{
	Completer::completeString(tokens, getPossibleValues(),
	                          false); // case insensitive
}

void EnumSettingPolicyBase::additionalInfoBase(TclObject& result) const
{
	TclObject valueList(result.getInterpreter());
	valueList.addListElements(this->getPossibleValues());
	result.addListElement(valueList);
}

} // namespace openmsx
