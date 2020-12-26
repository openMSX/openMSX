#include "IntegerSetting.hh"

namespace openmsx {

IntegerSetting::IntegerSetting(CommandController& commandController_,
                               std::string_view name_, static_string_view description_,
                               int initialValue, int minValue_, int maxValue_,
                               SaveSetting save_)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), save_)
	, minValue(minValue_)
	, maxValue(maxValue_)
{
	auto& interp = getInterpreter();
	setChecker([this, &interp](TclObject& newValue) {
		int val = newValue.getInt(interp); // may throw
		newValue = std::min(std::max(val, minValue), maxValue);
	});
	init();
}

std::string_view IntegerSetting::getTypeString() const
{
	return "integer";
}

void IntegerSetting::additionalInfo(TclObject& result) const
{
	result.addListElement(makeTclList(minValue, maxValue));
}

void IntegerSetting::setInt(int i)
{
	setValue(TclObject(i));
}

} // namespace openmsx
