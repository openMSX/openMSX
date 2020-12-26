#ifndef STRINGSETTING_HH
#define STRINGSETTING_HH

#include "Setting.hh"

namespace openmsx {

class StringSetting final : public Setting
{
public:
	StringSetting(CommandController& commandController,
	              std::string_view name, static_string_view description,
	              std::string_view initialValue, SaveSetting save = SAVE);

	[[nodiscard]] std::string_view getTypeString() const override;

	[[nodiscard]] std::string_view getString() const noexcept { return getValue().getString(); }
	void setString(std::string_view str) { setValue(TclObject(str)); }
};

} // namespace openmsx

#endif
