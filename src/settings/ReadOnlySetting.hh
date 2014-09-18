#ifndef READONLYSETTING_HH
#define READONLYSETTING_HH

#include "Setting.hh"

namespace openmsx {

class ReadOnlySetting final : public Setting
{
public:
	ReadOnlySetting(CommandController& commandController,
	                string_ref name, string_ref description,
	                const std::string& initialValue);

	const TclObject& getValue() const { return Setting::getValue(); }
	void setReadOnlyValue(const std::string& value);

	virtual string_ref getTypeString() const;

private:
	std::string roValue;
};

} // namespace openmsx

#endif
