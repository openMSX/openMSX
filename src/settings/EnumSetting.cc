#include "EnumSetting.hh"
#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"
#include "stringsp.hh"
#include "unreachable.hh"
#include "StringOp.hh"
#include "stl.hh"
#include <algorithm>

namespace openmsx {

typedef CmpTupleElement<0, StringOp::caseless> Comp;

EnumSettingBase::EnumSettingBase(BaseMap&& map)
	: baseMap(std::move(map))
{
	sort(baseMap.begin(), baseMap.end(), Comp());
}

int EnumSettingBase::fromStringBase(string_ref str) const
{
	auto it = lower_bound(baseMap.begin(), baseMap.end(), str, Comp());
	StringOp::casecmp cmp;
	if ((it == baseMap.end()) || !cmp(it->first, str)) {
		throw CommandException("not a valid value: " + str);
	}
	return it->second;
}

string_ref EnumSettingBase::toStringBase(int value) const
{
	for (auto& p : baseMap) {
		if (p.second == value) {
			return p.first;
		}
	}
	UNREACHABLE; return "";
}

std::vector<string_ref> EnumSettingBase::getPossibleValues() const
{
	std::vector<string_ref> result;
	for (auto& p : baseMap) {
		result.push_back(p.first);
	}
	return result;
}

void EnumSettingBase::additionalInfoBase(TclObject& result) const
{
	TclObject valueList(result.getInterpreter());
	valueList.addListElements(getPossibleValues());
	result.addListElement(valueList);
}

void EnumSettingBase::tabCompletionBase(std::vector<std::string>& tokens) const
{
	Completer::completeString(tokens, getPossibleValues(),
	                          false); // case insensitive
}

} // namespace openmsx
