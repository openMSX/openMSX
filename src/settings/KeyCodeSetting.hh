#ifndef KEYCODESETTING_HH
#define KEYCODESETTING_HH

#include "Setting.hh"
#include "Keys.hh"

namespace openmsx {

class KeyCodeSetting final : public Setting
{
public:
	KeyCodeSetting(CommandController& commandController,
	               std::string_view name, std::string_view description,
	               Keys::KeyCode initialValue);

	std::string_view getTypeString() const override;

	Keys::KeyCode getKey() const noexcept;
};

} // namespace openmsx

#endif
