#ifndef ROMINFOTOPIC_HH
#define ROMINFOTOPIC_HH

#include "InfoTopic.hh"

namespace openmsx {

class RomInfoTopic final : public InfoTopic
{
public:
	explicit RomInfoTopic(InfoCommand& openMSXInfoCommand);

	void execute(std::span<const TclObject> tokens,
	             TclObject& result) const override;
	[[nodiscard]] std::string help(std::span<const TclObject> tokens) const override;
	void tabCompletion(std::vector<std::string>& tokens) const override;
};

} // namespace openmsx

#endif
