#include "BooleanSetting.hh"
#include "CommandController.hh"
#include "Completer.hh"

namespace openmsx {

BooleanSetting::BooleanSetting(
		CommandController& commandController_, string_ref name,
		string_ref description_, bool initialValue, SaveSetting save_)
	: Setting(commandController_, name, description_,
	          TclObject(toString(initialValue)), save_)
{
	auto& interp = getInterpreter();
	setChecker([&interp](TclObject& newValue) {
		// May throw.
		// Re-set the queried value to get a normalized value.
		newValue.setString(toString(newValue.getBoolean(interp)));
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
