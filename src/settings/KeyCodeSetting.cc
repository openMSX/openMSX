#include "KeyCodeSetting.hh"
#include "CommandException.hh"

namespace openmsx {

KeyCodeSetting::KeyCodeSetting(CommandController& commandController,
                               string_ref name, string_ref description,
                               Keys::KeyCode initialValue)
	: Setting(commandController, name, description,
	          TclObject(Keys::getName(initialValue)), SAVE)
{
	setChecker([this](TclObject& newValue) {
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
