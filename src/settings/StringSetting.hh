#ifndef STRINGSETTING_HH
#define STRINGSETTING_HH

#include "Setting.hh"

namespace openmsx {

class StringSetting final : public Setting
{
public:
	StringSetting(CommandController& commandController,
	              string_ref name, string_ref description,
	              string_ref initialValue, SaveSetting save = SAVE);

	string_ref getTypeString() const override;

	string_ref getString() const { return getValue().getString(); }
	void setString(string_ref str) { setValue(TclObject(str)); }
};

} // namespace openmsx

#endif
