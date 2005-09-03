// $Id$

#include "KeyCodeSetting.hh"
#include "CommandException.hh"

using std::string;

namespace openmsx {

// class KeyCodeSettingPolicy

KeyCodeSettingPolicy::KeyCodeSettingPolicy(CommandController& commandController)
	: SettingPolicy<Keys::KeyCode>(commandController)
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

// class KeyCodeSetting

KeyCodeSetting::KeyCodeSetting(CommandController& commandController,
                               const string& name, const string& description,
                               Keys::KeyCode initialValue)
	: SettingImpl<KeyCodeSettingPolicy>(
		commandController, name, description, initialValue,
		Setting::SAVE)
{
}

} // namespace openmsx
