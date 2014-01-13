#include "BooleanSetting.hh"
#include "Completer.hh"

namespace openmsx {

BooleanSetting::BooleanSetting(
		CommandController& commandController, string_ref name,
		string_ref description, bool initialValue, SaveSetting save)
	: Setting(commandController, name, description,
	          toString(initialValue), save)
{
	setChecker([this](TclObject& newValue) {
		// May throw.
		// Re-set the queried value to get a normalized value.
		newValue.setString(toString(newValue.getBoolean()));
	});
	init();
}

string_ref BooleanSetting::getTypeString() const
{
	return "boolean";
}

void BooleanSetting::tabCompletion(std::vector<std::string>& tokens) const
{
	static const char* const values[] = {
		"true",  "on",  "yes",
		"false", "off", "no",
	};
	Completer::completeString(tokens, values, false); // case insensitive
}


} // namespace openmsx
