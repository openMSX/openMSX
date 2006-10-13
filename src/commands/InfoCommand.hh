// $Id$

#ifndef INFOCOMMAND_HH
#define INFOCOMMAND_HH

#include "Command.hh"
#include <map>

namespace openmsx {

class InfoTopic;

class InfoCommand : public Command
{
public:
	InfoCommand(CommandRegistry& commandRegistry, const std::string& name);
	virtual ~InfoCommand();

	void   registerTopic(InfoTopic& topic, const std::string& name);
	void unregisterTopic(InfoTopic& topic, const std::string& name);

	bool hasTopic(const std::string& name) const;

	// Command
	virtual void execute(const std::vector<TclObject*>& tokens,
	                     TclObject& result);
	virtual std::string help(const std::vector<std::string>& tokens) const;
	virtual void tabCompletion(std::vector<std::string>& tokens) const;

private:
	std::map<std::string, const InfoTopic*> infoTopics;
};

} // namespace openmsx

#endif
