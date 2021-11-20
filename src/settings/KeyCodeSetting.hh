#ifndef KEYCODESETTING_HH
#define KEYCODESETTING_HH

#include "Setting.hh"
#include "Keys.hh"

namespace openmsx {

class KeyCodeSetting final : public Setting
{
public:
	KeyCodeSetting(CommandController& commandController,
	               std::string_view name, static_string_view description,
	               Keys::KeyCode initialValue);

	[[nodiscard]] std::string_view getTypeString() const override;

	[[nodiscard]] Keys::KeyCode getKey() const noexcept;
};

} // namespace openmsx

#endif
