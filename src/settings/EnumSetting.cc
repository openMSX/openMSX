#include "EnumSetting.hh"

#include "CommandException.hh"
#include "TclObject.hh"

#include "StringOp.hh"
#include "ranges.hh"

#include <algorithm>

namespace openmsx {

EnumSettingBase::EnumSettingBase(Map&& map)
	: baseMap(std::move(map))
{
	std::ranges::sort(baseMap, StringOp::caseless{}, &MapEntry::name);
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
	auto it = std::ranges::find(baseMap, value, &MapEntry::value);
	assert(it != baseMap.end());
	return it->name;
}

void EnumSettingBase::additionalInfoBase(TclObject& result) const
{
	TclObject valueList;
	valueList.addListElements(getPossibleValues());
	result.addListElement(valueList);
}

} // namespace openmsx
