#include "FloatSetting.hh"
#include "CommandController.hh"

namespace openmsx {

FloatSetting::FloatSetting(CommandController& commandController_,
                           string_view name_, string_view description_,
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
		double clipped = std::min(std::max(val, minValue), maxValue);
		newValue.setDouble(clipped);
	});
	init();
}

string_view FloatSetting::getTypeString() const
{
	return "float";
}

void FloatSetting::additionalInfo(TclObject& result) const
{
	TclObject range;
	range.addListElement(minValue);
	range.addListElement(maxValue);
	result.addListElement(range);
}

void FloatSetting::setDouble(double d)
{
	setValue(TclObject(d));
}

} // namespace openmsx
