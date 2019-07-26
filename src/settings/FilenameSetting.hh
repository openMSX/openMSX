#ifndef FILENAMESETTING_HH
#define FILENAMESETTING_HH

#include "Setting.hh"

namespace openmsx {

class FilenameSetting final : public Setting
{
public:
	FilenameSetting(CommandController& commandController,
	                std::string_view name, std::string_view description,
	                std::string_view initialValue);

	std::string_view getTypeString() const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;

	std::string_view getString() const noexcept { return getValue().getString(); }
	void setString(std::string_view str) { setValue(TclObject(str)); }
};

} // namespace openmsx

#endif
