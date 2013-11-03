#ifndef FILENAMESETTING_HH
#define FILENAMESETTING_HH

#include "Setting.hh"

namespace openmsx {

class FilenameSetting : public Setting
{
public:
	FilenameSetting(CommandController& commandController,
	                string_ref name, string_ref description,
	                string_ref initialValue);

	virtual string_ref getTypeString() const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
};

} // namespace openmsx

#endif
