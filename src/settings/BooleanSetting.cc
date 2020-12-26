#include "BooleanSetting.hh"
#include "Completer.hh"

namespace openmsx {

BooleanSetting::BooleanSetting(
		CommandController& commandController_, std::string_view name,
		static_string_view description_, bool initialValue, SaveSetting save_)
	: Setting(commandController_, name, description_,
	          TclObject(toString(initialValue)), save_)
{
	auto& interp = getInterpreter();
	setChecker([&interp](TclObject& newValue) {
		// May throw.
		// Re-set the queried value to get a normalized value.
		newValue = toString(newValue.getBoolean(interp));
	});
	init();
}

std::string_view BooleanSetting::getTypeString() const
{
	return "boolean";
}

void BooleanSetting::tabCompletion(std::vector<std::string>& tokens) const
{
	static constexpr const char* const values[] = {
		"true",  "on",  "yes",
		"false", "off", "no",
	};
	Completer::completeString(tokens, values, false); // case insensitive
}


} // namespace openmsx
