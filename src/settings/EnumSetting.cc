#include "EnumSetting.hh"
#include "TclObject.hh"
#include "Completer.hh"
#include "CommandException.hh"
#include "stringsp.hh"
#include "unreachable.hh"

namespace openmsx {

EnumSettingBase::EnumSettingBase(BaseMap&& map)
	: baseMap(std::move(map))
{
}

int EnumSettingBase::fromStringBase(string_ref str_) const
{
	// An alternative we used in the past is to use StringOp::caseless
	// as the map comparator functor. This requires to #include StringOp.hh
	// in the header file. Because this header is included in many other
	// files, we prefer not to do that.
	// These maps are usually very small, so there is no disadvantage on
	// using a O(n) search here (instead of O(log n)).
	std::string str = str_.str();
	for (auto& p : baseMap) {
		if (strcasecmp(str.c_str(), p.first.c_str()) == 0) {
			return p.second;
		}
	}
	throw CommandException("not a valid value: " + str);
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
