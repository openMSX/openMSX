#ifndef READONLYSETTING_HH
#define READONLYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class ReadOnlySetting final : public Setting
{
public:
	ReadOnlySetting(CommandController& commandController,
	                string_ref name, string_ref description,
	                const TclObject& initialValue);

	void setReadOnlyValue(const TclObject& value);

	string_ref getTypeString() const override;

private:
	TclObject roValue;
};

} // namespace openmsx

#endif
