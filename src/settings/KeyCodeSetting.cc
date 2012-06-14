// $Id$

#include "KeyCodeSetting.hh"
#include "CommandException.hh"

using std::string;

namespace openmsx {

// class KeyCodeSettingPolicy

KeyCodeSettingPolicy::KeyCodeSettingPolicy()
{
}

string KeyCodeSettingPolicy::toString(Keys::KeyCode key) const
{
	return Keys::getName(key);
}

Keys::KeyCode KeyCodeSettingPolicy::fromString(const string& str) const
{
	Keys::KeyCode code = Keys::getCode(str);
	if (code == Keys::K_NONE) {
		throw CommandException("Not a valid key: " + str);
	}
	return code;
}

string_ref KeyCodeSettingPolicy::getTypeString() const
{
	return "key";
}

// class KeyCodeSetting

KeyCodeSetting::KeyCodeSetting(CommandController& commandController,
                               string_ref name, string_ref description,
                               Keys::KeyCode initialValue)
	: SettingImpl<KeyCodeSettingPolicy>(
		commandController, name, description, initialValue,
		Setting::SAVE)
{
}

} // namespace openmsx
