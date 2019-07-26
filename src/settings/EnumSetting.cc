#include "EnumSetting.hh"
#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"
#include "StringOp.hh"
#include "ranges.hh"
#include "stl.hh"
#include "stringsp.hh"
#include "view.hh"
#include "unreachable.hh"

namespace openmsx {

using Comp = CmpTupleElement<0, StringOp::caseless>;

EnumSettingBase::EnumSettingBase(BaseMap&& map)
	: baseMap(std::move(map))
{
	ranges::sort(baseMap, Comp());
}

int EnumSettingBase::fromStringBase(std::string_view str) const
{
	auto it = ranges::lower_bound(baseMap, str, Comp());
	StringOp::casecmp cmp;
	if ((it == end(baseMap)) || !cmp(it->first, str)) {
		throw CommandException("not a valid value: ", str);
	}
	return it->second;
}

std::string_view EnumSettingBase::toStringBase(int value) const
{
	for (const auto& [name, val] : baseMap) {
		if (val == value) {
			return name;
		}
	}
	UNREACHABLE; return {};
}

std::vector<std::string_view> EnumSettingBase::getPossibleValues() const
{
	return to_vector<std::string_view>(view::keys(baseMap));
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
