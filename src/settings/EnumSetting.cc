#include "EnumSetting.hh"
#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"
#include "KeyRange.hh"
#include "StringOp.hh"
#include "ranges.hh"
#include "stl.hh"
#include "stringsp.hh"
#include "unreachable.hh"

namespace openmsx {

using Comp = CmpTupleElement<0, StringOp::caseless>;

EnumSettingBase::EnumSettingBase(BaseMap&& map)
	: baseMap(std::move(map))
{
	ranges::sort(baseMap, Comp());
}

int EnumSettingBase::fromStringBase(string_view str) const
{
	auto it = ranges::lower_bound(baseMap, str, Comp());
	StringOp::casecmp cmp;
	if ((it == end(baseMap)) || !cmp(it->first, str)) {
		throw CommandException("not a valid value: ", str);
	}
	return it->second;
}

string_view EnumSettingBase::toStringBase(int value) const
{
	for (auto& p : baseMap) {
		if (p.second == value) {
			return p.first;
		}
	}
	UNREACHABLE; return {};
}

std::vector<string_view> EnumSettingBase::getPossibleValues() const
{
	return to_vector<string_view>(keys(baseMap));
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
