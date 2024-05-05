#include "KeyCodeSetting.hh"
#include "CommandException.hh"

namespace openmsx {

KeyCodeSetting::KeyCodeSetting(CommandController& commandController_,
                               std::string_view name_, static_string_view description_,
                               SDLKey initialValue)
	: Setting(commandController_, name_, description_,
	          TclObject(initialValue.toString()), Save::YES)
{
	setChecker([](const TclObject& newValue) {
		const auto& str = newValue.getString();
		if (!SDLKey::fromString(str)) {
			throw CommandException("Not a valid key: ", str);
		}
	});
	init();
}

std::string_view KeyCodeSetting::getTypeString() const
{
	return "key";
}

SDLKey KeyCodeSetting::getKey() const noexcept
{
	auto key = SDLKey::fromString(getValue().getString());
	assert(key);
	return *key;
}

} // namespace openmsx
