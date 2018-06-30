#include "KeyCodeSetting.hh"
#include "CommandException.hh"

namespace openmsx {

KeyCodeSetting::KeyCodeSetting(CommandController& commandController_,
                               string_ref name_, string_ref description_,
                               Keys::KeyCode initialValue)
	: Setting(commandController_, name_, description_,
	          TclObject(Keys::getName(initialValue)), SAVE)
{
	setChecker([](TclObject& newValue) {
		const auto& str = newValue.getString();
		if (Keys::getCode(str) == Keys::K_NONE) {
			throw CommandException("Not a valid key: " + str);
		}
	});
	init();
}

string_ref KeyCodeSetting::getTypeString() const
{
	return "key";
}

Keys::KeyCode KeyCodeSetting::getKey() const
{
	return Keys::getCode(getValue().getString());
}

} // namespace openmsx
