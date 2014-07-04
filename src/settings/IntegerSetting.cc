#include "IntegerSetting.hh"
#include "CommandController.hh"
#include "StringOp.hh"

namespace openmsx {

IntegerSetting::IntegerSetting(CommandController& commandController,
                               string_ref name, string_ref description,
                               int initialValue, int minValue_, int maxValue_)
	: Setting(commandController, name, description,
	          StringOp::toString(initialValue), SAVE)
	, minValue(minValue_)
	, maxValue(maxValue_)
{
	auto& interp = commandController.getInterpreter();
	setChecker([this, &interp](TclObject& newValue) {
		int value = newValue.getInt(interp); // may throw
		int clipped = std::min(std::max(value, minValue), maxValue);
		newValue.setInt(clipped);
	});
	init();
}

string_ref IntegerSetting::getTypeString() const
{
	return "integer";
}

void IntegerSetting::additionalInfo(TclObject& result) const
{
	TclObject range;
	range.addListElement(minValue);
	range.addListElement(maxValue);
	result.addListElement(range);
}

void IntegerSetting::setInt(int i)
{
	setString(StringOp::toString(i));
}

} // namespace openmsx
