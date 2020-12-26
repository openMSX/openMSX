#include "FloatSetting.hh"

namespace openmsx {

FloatSetting::FloatSetting(CommandController& commandController_,
                           std::string_view name_, static_string_view description_,
                           double initialValue,
                           double minValue_, double maxValue_)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue), SAVE)
	, minValue(minValue_)
	, maxValue(maxValue_)
{
	auto& interp = getInterpreter();
	setChecker([this, &interp](TclObject& newValue) {
		double val = newValue.getDouble(interp); // may throw
		newValue = std::min(std::max(val, minValue), maxValue);
	});
	init();
}

std::string_view FloatSetting::getTypeString() const
{
	return "float";
}

void FloatSetting::additionalInfo(TclObject& result) const
{
	result.addListElement(makeTclList(minValue, maxValue));
}

void FloatSetting::setDouble(double d)
{
	setValue(TclObject(d));
}

} // namespace openmsx
