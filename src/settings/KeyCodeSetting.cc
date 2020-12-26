#include "KeyCodeSetting.hh"
#include "CommandException.hh"

namespace openmsx {

KeyCodeSetting::KeyCodeSetting(CommandController& commandController_,
                               std::string_view name_, static_string_view description_,
                               Keys::KeyCode initialValue)
	: Setting(commandController_, name_, description_,
	          TclObject(Keys::getName(initialValue)), SAVE)
{
	setChecker([](TclObject& newValue) {
		const auto& str = newValue.getString();
		if (Keys::getCode(str) == Keys::K_NONE) {
			throw CommandException("Not a valid key: ", str);
		}
	});
	init();
}

std::string_view KeyCodeSetting::getTypeString() const
{
	return "key";
}

Keys::KeyCode KeyCodeSetting::getKey() const noexcept
{
	return Keys::getCode(getValue().getString());
}

} // namespace openmsx
