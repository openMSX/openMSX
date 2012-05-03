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

using Comp = CmpTupleElement<0, StringOp::caseless>;

EnumSettingBase::EnumSettingBase(BaseMap&& map)
	: baseMap(std::move(map))
{
	sort(begin(baseMap), end(baseMap), Comp());
}

int EnumSettingBase::fromStringBase(string_ref str) const
{
	auto it = lower_bound(begin(baseMap), end(baseMap), str, Comp());
	StringOp::casecmp cmp;
	if ((it == end(baseMap)) || !cmp(it->first, str)) {
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
	UNREACHABLE; return {};
}

std::vector<string_ref> EnumSettingBase::getPossibleValues() const
{
	std::vector<string_ref> result;
	for (auto& p : baseMap) {
		result.emplace_back(p.first);
	}
	return result;
}

void EnumSettingBase::additionalInfoBase(TclObject& result) const
{
	TclObject valueList;
	valueList.addListElements(getPossibleValues());
	result.addListElement(valueList);
}

void EnumSettingBase::tabCompletionBase(std::vector<std::string>& tokens) const
{
	Completer::completeString(tokens, getPossibleValues(),
	                          false); // case insensitive
}

} // namespace openmsx
