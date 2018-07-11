#ifndef STRINGSETTING_HH
#define STRINGSETTING_HH

#include "Setting.hh"

namespace openmsx {

class StringSetting final : public Setting
{
public:
	StringSetting(CommandController& commandController,
	              string_view name, string_view description,
	              string_view initialValue, SaveSetting save = SAVE);

	string_view getTypeString() const override;

	string_view getString() const { return getValue().getString(); }
	void setString(string_view str) { setValue(TclObject(str)); }
};

} // namespace openmsx

#endif
