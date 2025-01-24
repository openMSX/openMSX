#include "EnumSetting.hh"

#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"

#include "StringOp.hh"
#include "ranges.hh"
#include "stl.hh"
#include "stringsp.hh"

namespace openmsx {

EnumSettingBase::EnumSettingBase(Map&& map)
	: baseMap(std::move(map))
{
	ranges::sort(baseMap, StringOp::caseless{}, &MapEntry::name);
}

int EnumSettingBase::fromStringBase(std::string_view str) const
{
	if (auto s = binary_find(baseMap, str, StringOp::caseless{}, &MapEntry::name)) {
		return s->value;
	}
	throw CommandException("not a valid value: ", str);
}

std::string_view EnumSettingBase::toStringBase(int value) const
{
	auto it = ranges::find(baseMap, value, &MapEntry::value);
	assert(it != baseMap.end());
	return it->name;
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
