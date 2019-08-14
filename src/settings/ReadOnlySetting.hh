#ifndef READONLYSETTING_HH
#define READONLYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class ReadOnlySetting final : public Setting
{
public:
	ReadOnlySetting(CommandController& commandController,
	                string_view name, string_view description,
	                const TclObject& initialValue);

	void setReadOnlyValue(const TclObject& value);

	string_view getTypeString() const override;

private:
	TclObject roValue;
};

} // namespace openmsx

#endif
