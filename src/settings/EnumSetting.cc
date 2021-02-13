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

EnumSettingBase::EnumSettingBase(Map&& map)
	: baseMap(std::move(map))
{
	ranges::sort(baseMap, StringOp::caseless{}, &MapEntry::name);
}

int EnumSettingBase::fromStringBase(std::string_view str) const
{
	auto it = ranges::lower_bound(baseMap, str, StringOp::caseless{}, &MapEntry::name);
	StringOp::casecmp cmp;
	if ((it == end(baseMap)) || !cmp(it->name, str)) {
		throw CommandException("not a valid value: ", str);
	}
	return it->value;
}

std::string_view EnumSettingBase::toStringBase(int value) const
{
	for (const auto& entry : baseMap) {
		if (entry.value == value) {
			return entry.name;
		}
	}
	UNREACHABLE; return {};
}

std::vector<std::string_view> EnumSettingBase::getPossibleValues() const
{
	return to_vector<std::string_view>(view::transform(baseMap, &MapEntry::name));
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
