#ifndef READONLYSETTING_HH
#define READONLYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class ReadOnlySetting final : public Setting
{
public:
	ReadOnlySetting(CommandController& commandController,
	                std::string_view name, static_string_view description,
	                const TclObject& initialValue);

	void setReadOnlyValue(const TclObject& value);

	[[nodiscard]] std::string_view getTypeString() const override;

private:
	TclObject roValue;
};

} // namespace openmsx

#endif
