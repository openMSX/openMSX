#ifndef READONLYSETTING_HH
#define READONLYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class ReadOnlySetting final : public Setting
{
public:
	ReadOnlySetting(CommandController& commandController,
	                std::string_view name, std::string_view description,
	                const TclObject& initialValue);

	void setReadOnlyValue(const TclObject& value);

	std::string_view getTypeString() const override;

private:
	TclObject roValue;
};

} // namespace openmsx

#endif
