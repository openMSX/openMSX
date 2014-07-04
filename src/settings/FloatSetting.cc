#include "FloatSetting.hh"

namespace openmsx {

static std::string toString(double d)
{
	TclObject obj;
	obj.setDouble(d);
	return obj.getString().str();
}

FloatSetting::FloatSetting(CommandController& commandController,
                           string_ref name, string_ref description,
                           double initialValue,
                           double minValue_, double maxValue_)
	: Setting(commandController, name, description,
	          toString(initialValue), SAVE)
	, minValue(minValue_)
	, maxValue(maxValue_)
{
	setChecker([this](TclObject& newValue) {
		double value = newValue.getDouble(); // may throw
		double clipped = std::min(std::max(value, minValue), maxValue);
		newValue.setDouble(clipped);
	});
	init();
}

string_ref FloatSetting::getTypeString() const
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

void FloatSetting::setDouble (double d)
{
	setString(toString(d));
}

} // namespace openmsx
