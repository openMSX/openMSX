#ifndef FILENAMESETTING_HH
#define FILENAMESETTING_HH

#include "Setting.hh"

namespace openmsx {

class FilenameSetting final : public Setting
{
public:
	FilenameSetting(CommandController& commandController,
	                string_ref name, string_ref description,
	                string_ref initialValue);

	string_ref getTypeString() const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	string_ref getString() const { return getValue().getString(); }
	void setString(string_ref str) { setValue(TclObject(str)); }
};

} // namespace openmsx

#endif
