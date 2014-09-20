#ifndef KEYCODESETTING_HH
#define KEYCODESETTING_HH

#include "Setting.hh"
#include "Keys.hh"

namespace openmsx {

class KeyCodeSetting final : public Setting
{
public:
	KeyCodeSetting(CommandController& commandController,
	               string_ref name, string_ref description,
	               Keys::KeyCode initialValue);

	string_ref getTypeString() const override;

	Keys::KeyCode getKey() const;
};

} // namespace openmsx

#endif
