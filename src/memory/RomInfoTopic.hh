// $Id$

#ifndef ROMINFOTOPIC_HH
#define ROMINFOTOPIC_HH

#include "InfoTopic.hh"

namespace openmsx {

class RomInfoTopic : public InfoTopic
{
public:
	RomInfoTopic();

	virtual void execute(const std::vector<CommandArgument>& tokens,
	                     CommandArgument& result) const;
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;
};

} // namespace openmsx

#endif
