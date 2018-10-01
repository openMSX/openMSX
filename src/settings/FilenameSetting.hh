#ifndef FILENAMESETTING_HH
#define FILENAMESETTING_HH

#include "Setting.hh"

namespace openmsx {

class FilenameSetting final : public Setting
{
public:
	FilenameSetting(CommandController& commandController,
	                string_view name, string_view description,
	                string_view initialValue);

	string_view getTypeString() const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	string_view getString() const { return getValue().getString(); }
	void setString(string_view str) { setValue(TclObject(str)); }
};

} // namespace openmsx

#endif
