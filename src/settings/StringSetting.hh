#ifndef STRINGSETTING_HH
#define STRINGSETTING_HH

#include "Setting.hh"

namespace openmsx {

class StringSetting : public Setting
{
public:
	StringSetting(CommandController& commandController,
	              string_ref name, string_ref description,
	              string_ref initialValue, SaveSetting save = SAVE);

	virtual string_ref getTypeString() const;
};

} // namespace openmsx

#endif
